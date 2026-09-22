#include "Bluetooth/BlueZ_Interface.hpp"
#include "Bluetooth/BlueZ_HID_Services.hpp"
#include "Bluetooth/Gatt/Application.hpp"
#include "Bluetooth/Gatt/Base_App_Obj.hpp"
#include "Bluetooth/Gatt/Characteristic.hpp"
#include "Bluetooth/Gatt/Descriptor.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <sdbus-c++/Error.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/Types.h>
#include <sdbus-c++/IObject.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Message.h>

namespace
{
	// Errors meaning "BlueZ no longer has this object", e.g. because it
	// already released it or bluetoothd was restarted. Nothing to undo then.
	bool already_released(const sdbus::Error& e)
	{
		const std::string& name = e.getName();
		return name == "org.bluez.Error.DoesNotExist"
			|| name == "org.freedesktop.DBus.Error.UnknownMethod"
			|| name == "org.freedesktop.DBus.Error.UnknownObject"
			|| name == "org.freedesktop.DBus.Error.UnknownInterface"
			|| name == "org.freedesktop.DBus.Error.ServiceUnknown";
	}
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

	// Make only this LE advertisement discoverable. The adapter's own
	// Discoverable property is left alone: setting it also makes the
	// controller discoverable over BR/EDR, and a dual-mode host that finds
	// us over BR/EDR pairs over BR/EDR, where HID over GATT does not exist.
	this->ad_object->registerProperty("Discoverable")
		.onInterface("org.bluez.LEAdvertisement1")
			.withGetter(
				[]()
				{
					return true;
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
	if (!this->bluez_proxy)
		return;

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
	if (!this->bluez_proxy)
		return;

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
	if (!this->dbus_connection)
		return;

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
	if (!this->bluez_proxy || !this->advertising.load(std::memory_order_acquire))
		return;
	
	try
	{
		this->bluez_proxy->UnregisterAdvertisement(sdbus::ObjectPath(this->gatt_app->get_full_path() + "/advertisement0"));
	}
	catch (const sdbus::Error& e)
	{
		if (!already_released(e))
		{
			std::cerr << "Failed to unregister advertisement: " << e.getMessage() << std::endl;
			return;
		}
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
		if (!already_released(e))
		{
			std::cerr << "Failed to unregister GATT application: " << e.getMessage() << std::endl;
			return;
		}
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
		if (!already_released(e))
			std::cerr << "Failed to unregister agent: " << e.getMessage() << std::endl;
	}

	this->agent.reset();
}

// Tracks every connected Device1 by path. A single flag is not enough: this
// machine keeps its other (e.g. classic) connections, and one of those
// disconnecting must not silence the HID host.
void BlueZ_Interface::set_device_connected(const std::string& obj_path, bool connected)
{
	static std::atomic_bool accessing_connected_devices = false;

	while (accessing_connected_devices.exchange(true, std::memory_order_acquire))
	{
		accessing_connected_devices.wait(true, std::memory_order_acquire);
	}

	bool newly_connected = connected ? this->connected_devices.insert(obj_path).second : false;
	
	if (!connected && this->connected_devices.erase(obj_path) == 0) 
		return;

	this->connected_to_host.store(!this->connected_devices.empty(), std::memory_order_release);

	accessing_connected_devices.store(false, std::memory_order_release);
	accessing_connected_devices.notify_one();

	this->connected_to_host.notify_all();

	if (!connected)
	{
		std::cout << "Bluetooth device disconnected: " << obj_path << std::endl;
		return;
	}

	if (!newly_connected)
		return;

	std::cout << "Bluetooth device connected: " << obj_path << std::endl;

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

void BlueZ_Interface::subscribe_to_device(const std::string& obj_path)
{
	while (this->accessing_proxy_list.exchange(true, std::memory_order_acquire))
	{
		this->accessing_proxy_list.wait(true, std::memory_order_acquire);
	}
	
	if (this->device_proxies.contains(obj_path))	// Already watching
		return;

	this->accessing_proxy_list.store(false, std::memory_order_release);
	this->accessing_proxy_list.notify_one();

	// The proxy is kept in device_proxies: destroying it would drop the
	// PropertiesChanged subscription.
	std::unique_ptr<sdbus::IProxy> device_proxy = sdbus::createProxy(*this->dbus_connection, "org.bluez", obj_path);

	device_proxy->uponSignal("PropertiesChanged")
		.onInterface("org.freedesktop.DBus.Properties")
			.call(
				[this, obj_path](const std::string& interface, const std::map<std::string, sdbus::Variant>& changed, const std::vector<std::string>&)
				{
					if (interface == "org.bluez.Device1" && changed.contains("Connected"))
						this->set_device_connected(obj_path, changed.at("Connected").get<bool>());
				}
			);
	device_proxy->finishRegistration();

	// A brand-new host's Device1 object appears (InterfacesAdded) at the
	// moment it connects, so its "Connected = true" change is usually emitted
	// before the subscription above exists and is never delivered. Read the
	// current value now that the subscription is in place.
	bool connected = false;
	try
	{
		sdbus::Variant value = device_proxy->getProperty("Connected").onInterface("org.bluez.Device1");
		connected = value.get<bool>();
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "Could not read connection state of " << obj_path << ": " << e.getMessage() << std::endl;
	}

	while (this->accessing_proxy_list.exchange(true, std::memory_order_acquire))
	{
		this->accessing_proxy_list.wait(true, std::memory_order_acquire);
	}

	this->device_proxies.emplace(obj_path, std::move(device_proxy));

	this->accessing_proxy_list.store(false, std::memory_order_release);
	this->accessing_proxy_list.notify_one();

	if (connected)
		this->set_device_connected(obj_path, true);
}

void BlueZ_Interface::monitor_connection()
{
	if (!this->bluez_proxy) return;

	// The watcher must be a member. A local unique_ptr would be destroyed when
	// this function returns, taking the signal handlers with it.
	this->connection_watcher = sdbus::createProxy(*this->dbus_connection, "org.bluez", "/");

	// New devices, including a host connecting and pairing for the first time
	this->connection_watcher->registerSignalHandler("org.freedesktop.DBus.ObjectManager", "InterfacesAdded",
		[this](sdbus::Signal signal)
		{
			sdbus::ObjectPath obj_path;
			InterfacesMap interfaces;
			signal >> obj_path >> interfaces;

			if (interfaces.contains("org.bluez.Device1"))
				this->subscribe_to_device(obj_path);
		}
	);

	// Devices that were removed (unpaired, or temporary objects expiring)
	this->connection_watcher->registerSignalHandler("org.freedesktop.DBus.ObjectManager", "InterfacesRemoved",
		[this](sdbus::Signal signal)
		{
			sdbus::ObjectPath obj_path;
			std::vector<std::string> interfaces;
			signal >> obj_path >> interfaces;

			if (std::find(interfaces.begin(), interfaces.end(), "org.bluez.Device1") == interfaces.end())
				return;

			this->set_device_connected(obj_path, false);

			while (this->accessing_proxy_list.exchange(true, std::memory_order_acquire))
			{
				this->accessing_proxy_list.wait(true, std::memory_order_acquire);
			}

			this->device_proxies.erase(obj_path);

			this->accessing_proxy_list.store(false, std::memory_order_release);
			this->accessing_proxy_list.notify_one();
		}
	);

	this->connection_watcher->finishRegistration();

	// Already-known devices never emit InterfacesAdded again; watch them all
	// now (subscribe_to_device also picks up any that are connected already).
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
				this->subscribe_to_device(path);
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

		try	// Ensure the adapter is on and accepts pairing
		{
			this->orig_powered_state = this->bluez_proxy->Powered();
			this->orig_pairable_state = this->bluez_proxy->Pairable();
			this->orig_alias = this->bluez_proxy->Alias();

			// BR/EDR stays enabled and untouched so existing classic
			// connections and pairing on this machine keep working. The
			// adapter is deliberately NOT made discoverable (see
			// create_advertisement); only the LE advertisement is.
			this->bluez_proxy->Powered(true);
			this->bluez_proxy->Pairable(true);

			// GAP Device Name (read by the host during pairing) comes from
			// the adapter alias, so keep it in step with the advertised name.
			this->bluez_proxy->Alias(this->device_name);
		}
		catch (const sdbus::Error& e)
		{
			std::cerr << "Warning: Bluetooth adapter properties could not be set: " << e.getMessage() << std::endl;
		}

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
	// Release what we registered first, while bluetoothd is still using
	// the adapter exactly as we configured it, then restore the adapter.
	this->unregister_advertisement();
	this->unregister_gatt_application();
	this->unregister_agent();

	// Guard against being called when enable() never fully succeeded
	// (e.g. no adapter found). The proxy may legitimately be null here.
	if (this->bluez_proxy)
	{
		try
		{
			this->bluez_proxy->Alias(this->orig_alias);
			this->bluez_proxy->Pairable(this->orig_pairable_state);
			this->bluez_proxy->Powered(this->orig_powered_state);
		}
		catch (const sdbus::Error& e)
		{
			if (!already_released(e))
				std::cerr << "Warning: Could not restore adapter properties: " << e.getMessage() << std::endl;
		}
	}

	this->connection_watcher.reset();

	while (this->accessing_proxy_list.exchange(true, std::memory_order_acquire))
	{
		this->accessing_proxy_list.wait(true, std::memory_order_acquire);
	}

	this->device_proxies.clear();
	this->connected_devices.clear();

	this->accessing_proxy_list.store(false, std::memory_order_release);
	this->accessing_proxy_list.notify_all();
	
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

	for (std::size_t n = 0; ; ++n)
	{
		const Base_App_Obj* child = hid_service->get_subelement(n);

		if (child == nullptr)
			break;

		if (child->get_uuid() != "2a4d")
			continue;

		const Base_App_Obj* desc0 = child->get_subelement(0);
		if (desc0 == nullptr)
			continue;

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