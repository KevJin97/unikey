#include "Bluetooth/BlueZ_Interface.hpp"
#include "Bluetooth/BlueZ_HID_Services.hpp"
#include "Bluetooth/Gatt/Application.hpp"
#include "Bluetooth/Gatt/Base_App_Obj.hpp"
#include "Bluetooth/Gatt/Characteristic.hpp"
#include "Bluetooth/Gatt/Descriptor.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <sdbus-c++/Error.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/Types.h>
#include <sdbus-c++/IObject.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Message.h>

// ─── Kernel Bluetooth management interface ───────────────────────────────────
// BlueZ has no D-Bus API to switch a dual-mode controller to LE-only, so this
// talks to the kernel's mgmt control channel directly (the same interface
// bluetoothd and btmgmt use). Requires CAP_NET_ADMIN.
namespace
{
	namespace mgmt
	{
		constexpr int      BTPROTO_HCI_PROTO   = 1;
		constexpr uint16_t HCI_CHANNEL_CONTROL = 3;
		constexpr uint16_t INDEX_NONE          = 0xFFFF;

		constexpr uint16_t OP_READ_INFO   = 0x0004;
		constexpr uint16_t OP_SET_POWERED = 0x0005;
		constexpr uint16_t OP_SET_LE      = 0x000D;
		constexpr uint16_t OP_SET_BREDR   = 0x002A;

		constexpr uint16_t EV_CMD_COMPLETE = 0x0001;
		constexpr uint16_t EV_CMD_STATUS   = 0x0002;

		constexpr uint32_t SETTING_POWERED = 1u << 0;
		constexpr uint32_t SETTING_BREDR   = 1u << 7;
		constexpr uint32_t SETTING_LE      = 1u << 9;

		struct sockaddr_hci_mgmt
		{
			sa_family_t    hci_family;
			unsigned short hci_dev;
			unsigned short hci_channel;
		};

		class Socket
		{
			private:
				int fd = -1;

			public:
				Socket()
				{
					this->fd = ::socket(AF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK, BTPROTO_HCI_PROTO);
					if (this->fd < 0) return;

					sockaddr_hci_mgmt addr{};
					addr.hci_family  = AF_BLUETOOTH;
					addr.hci_dev     = INDEX_NONE;
					addr.hci_channel = HCI_CHANNEL_CONTROL;
					if (::bind(this->fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0)
					{
						::close(this->fd);
						this->fd = -1;
					}
				}
				~Socket() { if (this->fd >= 0) ::close(this->fd); }
				Socket(const Socket&) = delete;
				Socket& operator=(const Socket&) = delete;

				bool valid() const { return this->fd >= 0; }

				// Sends a command and waits for its Command Complete/Status.
				// Returns the status byte and the return parameters.
				std::optional<std::pair<uint8_t, std::vector<uint8_t>>> command(uint16_t opcode, uint16_t index, const std::vector<uint8_t>& params = {})
				{
					if (!this->valid()) return std::nullopt;

					std::vector<uint8_t> packet(6 + params.size());
					const uint16_t header[3] = { opcode, index, static_cast<uint16_t>(params.size()) };	// Little-endian host (ARM/x86)
					std::memcpy(packet.data(), header, sizeof(header));
					std::copy(params.begin(), params.end(), packet.begin() + 6);
					if (::write(this->fd, packet.data(), packet.size()) != static_cast<ssize_t>(packet.size()))
						return std::nullopt;

					const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
					while (std::chrono::steady_clock::now() < deadline)
					{
						pollfd pfd{ this->fd, POLLIN, 0 };
						if (::poll(&pfd, 1, 100) <= 0) continue;

						uint8_t buffer[512];
						const ssize_t length = ::read(this->fd, buffer, sizeof(buffer));
						if (length < 9) continue;	// Header (6) + opcode (2) + status (1)

						uint16_t event, event_index, event_opcode;
						std::memcpy(&event, buffer, 2);
						std::memcpy(&event_index, buffer + 2, 2);
						std::memcpy(&event_opcode, buffer + 6, 2);
						if ((event != EV_CMD_COMPLETE && event != EV_CMD_STATUS) || event_index != index || event_opcode != opcode)
							continue;	// Unrelated event (New Settings, other controllers, ...)

						return std::make_pair(buffer[8], std::vector<uint8_t>(buffer + 9, buffer + length));
					}
					return std::nullopt;
				}

				std::optional<uint32_t> current_settings(uint16_t index)
				{
					// Read Controller Information: address(6) version(1) manufacturer(2) supported(4) current(4) ...
					auto reply = this->command(OP_READ_INFO, index);
					if (!reply || reply->first != 0 || reply->second.size() < 17) return std::nullopt;
					uint32_t settings;
					std::memcpy(&settings, reply->second.data() + 13, sizeof(settings));
					return settings;
				}

				static const char* status_name(uint8_t status)
				{
					switch (status)
					{
						case 0x00: return "Success";
						case 0x0B: return "Rejected";
						case 0x0C: return "Not Supported";
						case 0x0D: return "Invalid Parameters";
						case 0x0F: return "Not Powered";
						case 0x10: return "Cancelled";
						case 0x11: return "Invalid Index";
						case 0x14: return "Permission Denied";
						default:   return "Error";
					}
				}

				// Returns the mgmt status (0 = success), or nullopt if there was no reply.
				std::optional<uint8_t> set(uint16_t opcode, uint16_t index, bool on)
				{
					auto reply = this->command(opcode, index, { static_cast<uint8_t>(on ? 1 : 0) });
					if (!reply)
					{
						std::cerr << "Bluetooth mgmt command 0x" << std::hex << opcode << std::dec << ": no reply" << std::endl;
						return std::nullopt;
					}
					if (reply->first != 0)
					{
						std::cerr << "Bluetooth mgmt command 0x" << std::hex << opcode << std::dec << " failed: "
							<< status_name(reply->first) << " (0x" << std::hex << static_cast<int>(reply->first) << std::dec << ")" << std::endl;
					}
					return reply->first;
				}
		};

		// "/org/bluez/hci0" → 0
		std::optional<uint16_t> index_from_path(const std::string& path)
		{
			const std::size_t pos = path.rfind("/hci");
			if (pos == std::string::npos) return std::nullopt;
			try { return static_cast<uint16_t>(std::stoul(path.substr(pos + 4))); }
			catch (...) { return std::nullopt; }
		}
	}
}

// A dual-mode controller that is discoverable over BR/EDR is found and
// connected over BR/EDR by dual-mode hosts (PCs, phones). HID over GATT only
// exists on LE, so such a host pairs over BR/EDR, finds no HID service there
// and binds whatever audio profiles the system offers instead. Turning BR/EDR
// off makes the controller LE-only: the kernel then advertises the "BR/EDR Not
// Supported" flag, stops page/inquiry scanning, and hosts must use LE/GATT.
// BR/EDR can only be switched while the controller is powered off.
namespace
{
	void print_le_only_help()
	{
		std::cerr <<
			"\n"
			"  ERROR: the Bluetooth controller is still dual-mode (BR/EDR enabled).\n"
			"  PCs and phones will pair over BR/EDR, where HID over GATT does not exist,\n"
			"  so no keyboard/mouse/touch reports can ever reach them. Fix one of:\n"
			"    * set  ControllerMode = le  in /etc/bluetooth/main.conf, then\n"
			"      sudo systemctl restart bluetooth   (no extra privileges needed), or\n"
			"    * give unikey CAP_NET_ADMIN, e.g.  sudo setcap cap_net_admin+ep <path to unikey>\n"
			"      (or AmbientCapabilities=CAP_NET_ADMIN in its systemd unit).\n"
			"  Then remove the existing pairing on the host and on this device.\n" << std::endl;
	}
}

bool BlueZ_Interface::make_controller_le_only()
{
	if (this->adapter_index == mgmt::INDEX_NONE) return false;

	mgmt::Socket socket;
	if (!socket.valid())
	{
		std::cerr << "Cannot open the Bluetooth management socket: " << std::strerror(errno) << std::endl;
		return false;
	}

	std::optional<uint32_t> settings = socket.current_settings(this->adapter_index);
	if (!settings)
	{
		std::cerr << "Cannot read the Bluetooth controller settings" << std::endl;
		return false;
	}

	if (!(*settings & mgmt::SETTING_BREDR) && (*settings & mgmt::SETTING_LE))
		return true;	// Already LE-only (e.g. ControllerMode = le)

	// Without CAP_NET_ADMIN the kernel accepts the socket but answers every
	// state-changing command with Permission Denied, so check this first
	// instead of leaving the controller powered off.
	auto ok = [](std::optional<uint8_t> status) { return status && *status == 0; };

	if (*settings & mgmt::SETTING_POWERED)
	{
		std::optional<uint8_t> status = socket.set(mgmt::OP_SET_POWERED, this->adapter_index, false);
		if (!ok(status))
		{
			if (status && *status == 0x14)
				std::cerr << "unikey lacks CAP_NET_ADMIN, so it cannot switch the controller to LE-only mode" << std::endl;
			return false;
		}
	}

	if (!(*settings & mgmt::SETTING_LE) && !ok(socket.set(mgmt::OP_SET_LE, this->adapter_index, true)))
		return false;

	if (!ok(socket.set(mgmt::OP_SET_BREDR, this->adapter_index, false)))
		return false;
	this->restore_bredr = true;

	std::cout << "Bluetooth controller switched to LE-only mode" << std::endl;
	return true;	// enable() powers the controller back on
}

// Reads the live controller state; works without CAP_NET_ADMIN.
bool BlueZ_Interface::controller_is_le_only() const
{
	if (this->adapter_index == mgmt::INDEX_NONE) return false;
	mgmt::Socket socket;
	std::optional<uint32_t> settings = socket.current_settings(this->adapter_index);
	return settings && !(*settings & mgmt::SETTING_BREDR) && (*settings & mgmt::SETTING_LE);
}

void BlueZ_Interface::restore_controller_bredr()
{
	if (!this->restore_bredr || this->adapter_index == mgmt::INDEX_NONE) return;

	mgmt::Socket socket;
	std::optional<uint32_t> settings = socket.current_settings(this->adapter_index);
	if (!settings) return;

	if (*settings & mgmt::SETTING_POWERED)
		socket.set(mgmt::OP_SET_POWERED, this->adapter_index, false);
	if (std::optional<uint8_t> status = socket.set(mgmt::OP_SET_BREDR, this->adapter_index, true); status && *status == 0)
		this->restore_bredr = false;
	// disable() restores the original power state afterwards
}

std::vector<std::string> BlueZ_Interface::find_adapter() const
{
	std::vector<std::string> adapter_list;
	std::unique_ptr<sdbus::IProxy> proxy = sdbus::createProxy(*this->dbus_connection, "org.bluez", "/");
	ManagedObjectsMap objects;

	try
	{
		proxy->callMethod("GetManagedObjects")
			.onInterface("org.freedesktop.DBus.ObjectManager")
				.storeResultsTo(objects);
	}
	catch (...)
	{
		return adapter_list;
	}

	for (const auto& [path, interfaces] : objects)
	{
		if (interfaces.contains("org.bluez.GattManager1"))
		{
			adapter_list.push_back(path);
		}
	}

	return adapter_list;
}

void BlueZ_Interface::create_advertisement()
{
	std::string ad_path = this->gatt_app->get_full_path() + "/advertisement0";
	this->ad_object = sdbus::createObject(*this->dbus_connection, ad_path);

	// "peripheral" — we are the GATT server, not a central/scanner.
	this->ad_object->registerProperty("Type")
		.onInterface("org.bluez.LEAdvertisement1")
			.withGetter(
				[]()
				{
					return std::string("peripheral");
				}
			);

	// Advertise all three service UUIDs so the connecting device knows
	// exactly which profiles to expect before it connects.
	this->ad_object->registerProperty("ServiceUUIDs")
		.onInterface("org.bluez.LEAdvertisement1")
			.withGetter(
				[]()
				{
					return std::vector<std::string>{ "1812", "180f", "180a" };
				}
			);

	this->ad_object->registerProperty("LocalName")
		.onInterface("org.bluez.LEAdvertisement1")
			.withGetter(
				[this]()
				{
					return this->device_name;
				}
			);

	// GAP Appearance "Generic Human Interface Device" (0x03C0), so hosts list
	// the device as an input device. BlueZ requires the D-Bus type to be
	// uint16 ('q'); any other integer type is rejected with InvalidArguments.
	this->ad_object->registerProperty("Appearance")
		.onInterface("org.bluez.LEAdvertisement1")
			.withGetter(
				[]()
				{
					return static_cast<uint16_t>(0x03C0);
				}
			);

	this->ad_object->registerProperty("Includes")
		.onInterface("org.bluez.LEAdvertisement1")
			.withGetter(
				[]()
				{
					return std::vector<std::string>{ "tx-power" };
				}
			);

	this->ad_object->registerMethod("Release")
		.onInterface("org.bluez.LEAdvertisement1")
			.implementedAs(
				[this]()
				{
					std::cout << "Advertisement released by BlueZ" << std::endl;
					this->advertising.store(false, std::memory_order_release);
				}
			);

	this->ad_object->finishRegistration();
}

void BlueZ_Interface::register_advertisement()
{
	if (!this->bluez_proxy) return;

	try
	{
		OptionsMap options;

		// Must run in asynchronous mode otherwise the system won't respond
		this->bluez_proxy->getProxy().callMethodAsync("RegisterAdvertisement")
			.onInterface("org.bluez.LEAdvertisingManager1")
				.withArguments(sdbus::ObjectPath(this->gatt_app->get_full_path() + "/advertisement0"), options)
					.uponReplyInvoke(
						[this](const sdbus::Error* error)
						{
							if (error)
							{
                            	std::cerr << "Failed to register advertisement: " << error->getMessage() << std::endl;
							}
							else
							{
								std::cout << "Advertisement registered successfully" << std::endl;
								this->advertising.store(true, std::memory_order_release);
								this->advertising.notify_all();
							}
						}
					);
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "Failed to register GATT advertisement: " << e.getMessage() << std::endl;
	}
}

void BlueZ_Interface::register_gatt_application()
{
	if (!this->bluez_proxy) return;

	std::cout << "Registering GATT application at: " << this->gatt_app->get_full_path() << std::endl;

	try
	{
		OptionsMap options;

		// Must run in asynchronous mode otherwise the system won't respond
		this->bluez_proxy->getProxy().callMethodAsync("RegisterApplication")
			.onInterface("org.bluez.GattManager1")
				.withArguments(sdbus::ObjectPath(this->gatt_app->get_full_path()), options)
					.uponReplyInvoke(
						[this](const sdbus::Error* error)
						{
							if (error)
							{
								std::cerr << "GATT registration failed: " << error->getMessage() << std::endl;
							}
							else
							{
								std::cout << "GATT application has been registered successfully" << std::endl;
                            	this->registered.store(true, std::memory_order_release);
                            	this->registered.notify_all();
							}
						}
					);
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "GATT registration failed: " << e.getMessage() << std::endl;
	}
}

void BlueZ_Interface::register_agent()
{
	if (!this->dbus_connection) return;

	std::string agent_path = this->path_name + "/agent";
	this->agent = std::make_unique<BlueZ_Agent>(*this->dbus_connection, agent_path);

	try
	{
		this->bluez_agent_man_proxy->RegisterAgent(sdbus::ObjectPath(this->agent->get_path()), this->agent->get_capability());
		std::cout << "Agent registered at " << this->agent->get_path() << " with capability: " << this->agent->get_capability() << std::endl;
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "Failed to register agent: " << e.getMessage() << std::endl;
		this->agent.reset();
		return;
	}

	try
	{
		this->bluez_agent_man_proxy->RequestDefaultAgent(sdbus::ObjectPath(this->agent->get_path()));
		std::cout << "Agent set as default" << std::endl;
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "Failed to set default agent: " << e.getMessage() << std::endl;
	}
}

void BlueZ_Interface::unregister_advertisement()
{
	if (!this->bluez_proxy || !this->advertising.load(std::memory_order_acquire)) return;
	
	try
	{
		this->bluez_proxy->UnregisterAdvertisement(sdbus::ObjectPath(this->gatt_app->get_full_path() + "/advertisement0"));
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "Failed to unregister advertisement: " << e.getMessage() << std::endl;
		return;
	}

	this->advertising.store(false, std::memory_order_release);
}

void BlueZ_Interface::unregister_gatt_application()
{
	if (!this->bluez_proxy || !this->registered.load(std::memory_order_acquire)) return;

	try
	{
		this->bluez_proxy->UnregisterApplication(sdbus::ObjectPath(this->gatt_app->get_full_path()));
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "Failed to unregister GATT application: " << e.getMessage() << std::endl;
		return;
	}

	this->registered.store(false, std::memory_order_release);
}

void BlueZ_Interface::unregister_agent()
{
	if (!this->agent || !this->dbus_connection) return;

	try
	{
		this->bluez_agent_man_proxy->UnregisterAgent(sdbus::ObjectPath(this->agent->get_path()));
		std::cout << "Agent has been unregistered" << std::endl;
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "Failed to unregister agent: " << e.getMessage() << std::endl;
	}

	this->agent.reset();
}

void BlueZ_Interface::subscribe_to_device(const std::string& obj_path)
{
	// Create a persistent proxy for this device path so we can subscribe
	// to its PropertiesChanged signal. This proxy MUST be stored in the
	// device_proxies member — a local unique_ptr would be destroyed when
	// this function returns, killing the subscription immediately and
	// meaning Connected changes would never be received.
	auto device_proxy = sdbus::createProxy(*this->dbus_connection, "org.bluez", obj_path);

	device_proxy->uponSignal("PropertiesChanged")
		.onInterface("org.freedesktop.DBus.Properties")
			.call(
				[this, obj_path](const std::string& interface, const std::map<std::string, sdbus::Variant>& changed, const std::vector<std::string>&)
				{
					if (interface != "org.bluez.Device1" || !changed.contains("Connected"))
					{
						return;
					}

					bool connected = changed.at("Connected").get<bool>();
					this->connected_to_host.store(connected, std::memory_order_release);
					this->connected_to_host.notify_all();

					if (connected)
					{
						std::cout << "BLE device connected: " << obj_path << std::endl;

						try
						{
							// Mark as trusted so the device can reconnect without
							// going through the full pairing flow again.
							sdbus::createProxy(*this->dbus_connection, "org.bluez", obj_path)
								->setProperty("Trusted")
									.onInterface("org.bluez.Device1")
										.toValue(true);
						}
						catch (const sdbus::Error& e)
						{
							std::cerr << "Failed to set device as trusted: " << e.getMessage() << std::endl;
						}
					}
					else
					{
						std::cout << "BLE device disconnected: " << obj_path << std::endl;
					}
				}
			);

	device_proxy->finishRegistration();
	this->device_proxies.push_back(std::move(device_proxy));
}

void BlueZ_Interface::monitor_connection()
{
	if (!this->bluez_proxy) return;

	// --- Part 1: Subscribe to future device connections via InterfacesAdded ---
	//
	// The watcher must be a member. A local unique_ptr would be destroyed when
	// this function returns, taking the InterfacesAdded handler with it.
	this->connection_watcher = sdbus::createProxy(*this->dbus_connection, "org.bluez", "/");

	this->connection_watcher->registerSignalHandler("org.freedesktop.DBus.ObjectManager", "InterfacesAdded",
		[this](sdbus::Signal signal)
		{
			sdbus::ObjectPath obj_path;
			InterfacesMap interfaces;
			signal >> obj_path >> interfaces;

			if (interfaces.contains("org.bluez.Device1"))
			{
				this->subscribe_to_device(obj_path);
			}
		}
	);

	this->connection_watcher->finishRegistration();

	// --- Part 2: Handle reconnections for already-known devices ---
	//
	// When a previously paired device reconnects, BlueZ reuses its existing
	// device object. InterfacesAdded is therefore never emitted for that
	// reconnection — only PropertiesChanged fires. We must subscribe to
	// PropertiesChanged on every existing Device1 object right now so that
	// reconnections are not missed.
	//
	// This is confirmed by the pairing capture: a classic BR/EDR connection
	// to the same device MAC appears mid-session on handle 0x0100 alongside
	// the BLE connection on 0x0108, meaning the host has had prior contact
	// with this device and its object may already exist in BlueZ.
	try
	{
		std::unique_ptr<sdbus::IProxy> om_proxy = sdbus::createProxy(*this->dbus_connection, "org.bluez", "/");
		ManagedObjectsMap existing_objects;
		om_proxy->callMethod("GetManagedObjects")
			.onInterface("org.freedesktop.DBus.ObjectManager")
				.storeResultsTo(existing_objects);

		for (const auto& [path, interfaces] : existing_objects)
		{
			if (interfaces.contains("org.bluez.Device1"))
			{
				this->subscribe_to_device(path);

				// If the device is already connected right now (e.g. we are
				// restarting the daemon mid-session), reflect that immediately.
				const auto& dev_props = interfaces.at("org.bluez.Device1");
				if (dev_props.contains("Connected") && dev_props.at("Connected").get<bool>())
				{
					std::cout << "Device already connected on startup: " << path << std::endl;
					this->connected_to_host.store(true, std::memory_order_release);
					this->connected_to_host.notify_all();
				}
			}
		}
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "Warning: Could not enumerate existing BlueZ devices: " << e.getMessage() << std::endl;
	}
}

BlueZ_Interface::BlueZ_Interface(sdbus::IConnection* dbus_connection, const std::string& connection_name)
{
	if (dbus_connection == nullptr)
	{
		this->new_connection = sdbus::createSystemBusConnection(connection_name);
		this->dbus_connection = this->new_connection.get();
	}
	else
	{
		this->dbus_connection = dbus_connection;
	}

	this->path_name = connection_name;
	for (std::size_t n = 0; n < this->path_name.size(); ++n)
	{
		if (this->path_name[n] == '.')
		{
			this->path_name[n] = '/';
		}
	}
	this->path_name = "/" + this->path_name + "/Bluetooth";
	this->bluez_agent_man_proxy = std::make_unique<BlueZ_Agent_Manager_Proxy>(*this->dbus_connection, "/org/bluez");
}

BlueZ_Interface::BlueZ_Interface(const std::unique_ptr<sdbus::IConnection>& dbus_connection, const std::string& connection_name) : BlueZ_Interface(dbus_connection.get(), connection_name) {}

BlueZ_Interface::BlueZ_Interface(const std::string& device_name, sdbus::IConnection* dbus_connection, const std::string& connection_name) : BlueZ_Interface(dbus_connection, connection_name)
{
	this->device_name = device_name;
}

BlueZ_Interface::BlueZ_Interface(const std::string& device_name, const std::unique_ptr<sdbus::IConnection>& dbus_connection, const std::string& connection_name) : BlueZ_Interface(device_name, dbus_connection.get(), connection_name) {}

BlueZ_Interface::~BlueZ_Interface()
{
	if (this->dbus_connection != nullptr)
	{
		this->disable();
	}
}

void BlueZ_Interface::set_path_name(const std::string& path_name)
{
	if (this->gatt_app == nullptr)
	{
		this->path_name = path_name;
	}
	else
	{
		std::cout << "Bluetooth device has already been registered. Unregister the device before trying to modify the path." << std::endl;
	}
}

void BlueZ_Interface::set_device_name(const std::string& device_name)
{
	if (this->gatt_app == nullptr)
	{
		this->device_name = device_name;
	}
	else
	{
		std::cout << "Bluetooth device has already been registered. Unregister the device before trying to modify the device name." << std::endl;
	}
}

bool BlueZ_Interface::set_hid_report_map(const hid::Report_Map& report_map)
{
	if (this->gatt_app == nullptr)
	{
		this->hid_report_map = report_map;
		return true;
	}

	std::cout << "Bluetooth device has already been registered. Unregister the device before trying to modify the HID report map." << std::endl;
	return false;
}

bool BlueZ_Interface::enable()
{
	if (this->registered.load(std::memory_order_acquire))
	{
		return true;
	}

	try
	{
		std::vector<std::string> adapters = this->find_adapter();
		if (adapters.size() == 0)
		{
			std::cerr << "No bluetooth adapters were found" << std::endl;
			return false;
		}
		
		std::cout << "Found adapter(s):" << std::endl;
		for (std::size_t n = 0; n < adapters.size(); ++n)
		{
			std::cout << "\t* " << adapters[n] << std::endl;
		}

		// Defaulting to first adapter
		this->bluez_proxy = std::make_unique<BlueZ_Adapter_Proxy>(*this->dbus_connection, adapters.front());
		this->adapter_index = mgmt::index_from_path(adapters.front()).value_or(mgmt::INDEX_NONE);

		try	// Ensure bluetooth adapter is on and discoverable
		{
			this->orig_powered_state = this->bluez_proxy->Powered();
			this->orig_discover_state = this->bluez_proxy->Discoverable();
			this->orig_pairable_state = this->bluez_proxy->Pairable();
			this->orig_alias = this->bluez_proxy->Alias();
			this->orig_discoverable_timeout = this->bluez_proxy->DiscoverableTimeout();

			// Must happen before powering on: BR/EDR can only be toggled while off
			this->make_controller_le_only();

			this->bluez_proxy->Powered(true);
			this->bluez_proxy->Discoverable(true);
			this->bluez_proxy->Pairable(true);

			// Keep discoverable indefinitely for the session lifetime so the
			// remote device always has an LE path to follow on reconnect.
			this->bluez_proxy->DiscoverableTimeout(0);

			// Align the BR/EDR adapter name with our LE advertisement name.
			// Without this, BR/EDR inquiries report the system hostname, which
			// can cause the connecting device to associate our adapter with its
			// cached system audio profile instead of treating it as a new HID
			// peripheral.
			this->bluez_proxy->Alias(this->device_name);
		}
		catch (const sdbus::Error& e)
		{
			std::cerr << "Warning: Bluetooth adapter properties could not be set: " << e.getMessage() << std::endl;
		}

		// Check the result, not the attempt: HID over GATT is unreachable
		// from dual-mode hosts while BR/EDR is enabled.
		if (this->controller_is_le_only())
			std::cout << "Bluetooth controller is LE-only" << std::endl;
		else
			print_le_only_help();

		this->register_agent();

		this->gatt_app = new Application(this->path_name + "/GattApplication", this->dbus_connection);
		this->gatt_app->add_subelement(new HIDService(this->hid_report_map));
		this->gatt_app->add_subelement(new DeviceInfoService);
		this->gatt_app->add_subelement(new BatteryService);

		this->register_gatt_application();
		this->create_advertisement();
		this->register_advertisement();
		this->monitor_connection();

		if (this->new_connection != nullptr)
		{
			this->dbus_connection->enterEventLoopAsync();
		}

		std::cout << "BlueZ interface has been enabled" << std::endl;
		return true;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Failed to enable BlueZ interface: " << e.what() << std::endl;
		this->disable();

		return false;
	}
}

bool BlueZ_Interface::connection_status() const
{
	return this->connected_to_host.load(std::memory_order_acquire);
}

void BlueZ_Interface::wait_until_connected()
{
	this->connected_to_host.wait(false, std::memory_order_acquire);
}

void BlueZ_Interface::disable()
{
	// Guard against being called when enable() never fully succeeded
	// (e.g. no adapter found). The proxy may legitimately be null here.
	if (this->bluez_proxy)
	{
		this->restore_controller_bredr();

		try
		{
			this->bluez_proxy->Powered(this->orig_powered_state);
			this->bluez_proxy->Discoverable(this->orig_discover_state);
			this->bluez_proxy->Pairable(this->orig_pairable_state);
			this->bluez_proxy->DiscoverableTimeout(this->orig_discoverable_timeout);
			this->bluez_proxy->Alias(this->orig_alias);
		}
		catch (const sdbus::Error& e)
		{
			std::cerr << "Warning: Could not restore adapter properties: " << e.getMessage() << std::endl;
		}
	}

	this->unregister_advertisement();
	this->unregister_gatt_application();
	this->unregister_agent();

	this->connection_watcher.reset();
	this->device_proxies.clear();
	this->ad_object.reset();
	this->bluez_proxy.reset();

	if (this->gatt_app != nullptr)
	{
		delete this->gatt_app;
		this->gatt_app = nullptr;
	}

	if (this->new_connection != nullptr)
	{
		this->dbus_connection->leaveEventLoop();
		this->new_connection.reset();
	}
	
	this->dbus_connection = nullptr;
	this->connected_to_host.store(false, std::memory_order_release);
	this->advertising.store(false, std::memory_order_release);
	this->registered.store(false, std::memory_order_release);

	std::cout << "BlueZ interface has been disabled" << std::endl;
}

void BlueZ_Interface::send_hid_report(uint8_t report_id, const void* data, uint64_t data_size) const
{
	if (data == nullptr || data_size == 0)
	{
		return;
	}

	std::vector<uint8_t> report((uint8_t*)data, (uint8_t*)data + data_size);
	this->send_hid_report(report_id, report);
}

void BlueZ_Interface::send_hid_report(uint8_t report_id, const std::vector<uint8_t>& report) const
{
	if (!this->connected_to_host.load(std::memory_order_acquire) || report.empty())
	{
		return;
	}

	// HID service is always subelement 0 of the GATT application.
	const Base_App_Obj* hid_service = this->gatt_app->get_subelement(0);
	if (hid_service == nullptr)
	{
		std::cerr << "HID service not found" << std::endl;
		return;
	}

	// Scan the HID service's characteristics for an Input Report (UUID 0x2A4D)
	// whose desc0 (Report Reference) matches [report_id, 0x01=Input].
	// This is robust to any reordering of characteristics in HIDService.
	Characteristic* target = nullptr;
	for (std::size_t i = 0; ; ++i)
	{
		const Base_App_Obj* child = hid_service->get_subelement(i);
		if (child == nullptr) break;
		if (child->get_uuid() != "2a4d") continue;

		const Base_App_Obj* desc0 = child->get_subelement(0);
		if (desc0 == nullptr) continue;

		ByteArray ref = ((Descriptor*)desc0)->on_read_value({});
		if (ref.size() >= 2 && ref[0] == report_id && ref[1] == 0x01)
		{
			target = (Characteristic*)child;
			break;
		}
	}

	if (target == nullptr)
	{
		std::cerr << "No input report characteristic found for report ID " << (int)report_id << std::endl;
		return;
	}

	target->update_value(report);
}