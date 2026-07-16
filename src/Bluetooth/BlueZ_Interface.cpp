#include "Bluetooth/BlueZ_Interface.hpp"
#include "Bluetooth/BlueZ_HID_Services.hpp"
#include "Bluetooth/Gatt/Application.hpp"
#include "Bluetooth/Gatt/Base_App_Obj.hpp"
#include "Bluetooth/Gatt/Characteristic.hpp"
#include "Bluetooth/Gatt/Descriptor.hpp"

#include <atomic>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <fstream>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <sdbus-c++/Error.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/Types.h>
#include <sdbus-c++/IObject.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Message.h>

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

	// NOTE: Do NOT register Appearance or Discoverable as LEAdvertisement1
	// properties. BlueZ validates the full object schema when
	// RegisterAdvertisement is called and rejects with InvalidArguments if
	// it encounters any property it does not recognise on that interface.
	// Appearance is set on the adapter directly (via Alias workaround) and
	// Discoverable is set on the adapter via bluez_proxy->Discoverable(true).

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

						this->request_connection_parameters(obj_path);
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

void BlueZ_Interface::request_connection_parameters(const std::string& obj_path) const
{
	std::size_t hci_start = obj_path.find("/hci");
	
	if (hci_start == std::string::npos)
		return;

	++hci_start;
	std::size_t hci_end = obj_path.find('/', hci_start);

	if (hci_end == std::string::npos)
		return;

	std::string hci = obj_path.substr(hci_start, hci_end - hci_start);
	std::size_t dev_start = obj_path.find("/dev_");

	if (dev_start == std::string::npos)
		return;

	dev_start += 5;
	std::string mac_underscored = obj_path.substr(dev_start);
	std::string mac = mac_underscored;

	for (std::size_t n = 0; n < mac.size(); ++n)
	{
		if (mac[n] == '_')
			mac[n] = ':';
	}
	
	const std::string base = "/sys/kernel/debug/bluetooth/" + hci + "/" + mac + "/";

	std::thread([base]()
	{
		auto write_param = [&base](const char* name, const char* value) -> bool
		{
			std::ofstream filename(base + name);
			
			if (!filename.is_open())
				return false;

			filename << value;
			return filename.good();
		};

		for (int attempt = 0; attempt < 10; ++attempt)
		{
			if (write_param("conn_min_interval", "6") &&
				write_param("conn_max_interval", "12") &&
				write_param("conn_latency", "0") &&
				write_param("supervision_timeout", "200"))
				{
					std::cout << "Connection parameters updated: 7.5-15 ms interval" << std::endl;
					return;
				}
			
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}

		std::cerr << "Warning: could not write connection parameters to " << base << " (need root privileges or central rejected the request)" << std::endl;
	}).detach();
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

		try	// Ensure bluetooth adapter is on and discoverable
		{
			this->orig_powered_state = this->bluez_proxy->Powered();
			this->orig_discover_state = this->bluez_proxy->Discoverable();
			this->orig_pairable_state = this->bluez_proxy->Pairable();
			this->orig_alias = this->bluez_proxy->Alias();
			this->orig_discoverable_timeout = this->bluez_proxy->DiscoverableTimeout();

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

		this->register_agent();

		this->gatt_app = new Application(this->path_name + "/GattApplication", this->dbus_connection);
		this->gatt_app->add_subelement(new HIDService);
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