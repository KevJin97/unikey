#include "Bluetooth/BlueZ_Interface.hpp"

#include <iostream>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IObject.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Types.h>

std::string BlueZ_Interface::find_adapter() const
{
	auto proxy = sdbus::createProxy(*this->connection, "org.bluez", "/");
	ManagedObjectsMap objects;

	try
	{
		proxy->callMethod("GetManagedObjects")
			.onInterface("org.freedesktop.DBus.ObjectManager")
				.storeResultsTo(objects);
	}
	catch (...)
	{
		return "";
	}

	for (const auto& [path, interfaces] : objects)
	{
		if (interfaces.count("org.bluez.GattManager1"))
		{
			return path;
		}
	}
	return "";
}

void BlueZ_Interface::create_advertisement()
{
	this->ad_path = this->app_path + "/advertisement0";
	this->ad_object = sdbus::createObject(*this->connection, this->ad_path);

	// org.bluez.LEAdvertisement1 properties
	this->ad_object->registerProperty("Type")
		.onInterface("org.bluez.LEAdvertisement1")
			.withGetter([](){ return std::string("peripheral"); });

	this->ad_object->registerProperty("ServiceUUIDs")
		.onInterface("org.bluez.LEAdvertisement1")
			.withGetter([]()
			{
				return std::vector<std::string>{ "1812", "180f", "180a" };
			});

	this->ad_object->registerProperty("LocalName")
		.onInterface("org.bluez.LEAdvertisement1")
			.withGetter([](){ return std::string("Unikey"); });

	// Appearance: 0x03C1 = Keyboard (HID subtype)
	this->ad_object->registerProperty("Appearance")
		.onInterface("org.bluez.LEAdvertisement1")
			.withGetter([](){ return uint16_t(0x03C1); });

	this->ad_object->registerProperty("Discoverable")
		.onInterface("org.bluez.LEAdvertisement1")
			.withGetter([](){ return true; });

	this->ad_object->registerProperty("Includes")
		.onInterface("org.bluez.LEAdvertisement1")
			.withGetter([]()
			{
				return std::vector<std::string>{ "tx-power" };
			});

	// Release method (called by BlueZ when advertisement is removed)
	this->ad_object->registerMethod("Release")
		.onInterface("org.bluez.LEAdvertisement1")
			.implementedAs([this]()
			{
				std::cout << "Advertisement released by BlueZ" << std::endl;
				this->advertising.store(false, std::memory_order_release);
			});

	this->ad_object->finishRegistration();
}

void BlueZ_Interface::register_advertisement()
{
	if (!this->ad_manager_proxy)
		return;

	OptionsMap options;

	this->ad_manager_proxy->callMethodAsync("RegisterAdvertisement")
		.onInterface("org.bluez.LEAdvertisingManager1")
			.withArguments(sdbus::ObjectPath(this->ad_path), options)
				.uponReplyInvoke(
					[this](const sdbus::Error* error)
					{
						if (error)
						{
							std::cerr << "Advertisement registration failed: "
								<< error->getMessage() << std::endl;
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

void BlueZ_Interface::unregister_advertisement()
{
	if (!this->ad_manager_proxy || !this->advertising.load(std::memory_order_acquire))
		return;

	try
	{
		this->ad_manager_proxy->callMethod("UnregisterAdvertisement")
			.onInterface("org.bluez.LEAdvertisingManager1")
				.withArguments(sdbus::ObjectPath(this->ad_path));
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "Failed to unregister advertisement: " << e.getMessage() << std::endl;
	}

	this->advertising.store(false, std::memory_order_release);
}

void BlueZ_Interface::register_gatt_application()
{
	if (!this->gatt_manager_proxy)
		return;

	OptionsMap options;

	std::cout << "Registering GATT app at " << this->app->get_full_path() << std::endl;

	this->gatt_manager_proxy->callMethodAsync("RegisterApplication")
		.onInterface("org.bluez.GattManager1")
			.withArguments(sdbus::ObjectPath(this->app->get_full_path()), options)
				.uponReplyInvoke(
					[this](const sdbus::Error* error)
					{
						if (error)
						{
							std::cerr << "GATT registration failed: "
								<< error->getMessage() << std::endl;
						}
						else
						{
							std::cout << "GATT application registered successfully" << std::endl;
							this->registered.store(true, std::memory_order_release);
							this->registered.notify_all();
						}
					}
				);
}

void BlueZ_Interface::unregister_gatt_application()
{
	if (!this->gatt_manager_proxy || !this->registered.load(std::memory_order_acquire))
		return;

	try
	{
		this->gatt_manager_proxy->callMethod("UnregisterApplication")
			.onInterface("org.bluez.GattManager1")
				.withArguments(sdbus::ObjectPath(this->app->get_full_path()));
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "Failed to unregister GATT application: " << e.getMessage() << std::endl;
	}

	this->registered.store(false, std::memory_order_release);
}

void BlueZ_Interface::monitor_connection()
{
	/*
		Monitor for device connections by watching for property changes on
		BlueZ device objects. When a device's "Connected" property changes
		to true, a BLE host has connected to us.
	*/
	if (!this->adapter_proxy)
		return;

	auto watcher = sdbus::createProxy(*this->connection, "org.bluez", "/");

	watcher->registerSignalHandler("org.freedesktop.DBus.ObjectManager", "InterfacesAdded",
		[this](sdbus::Signal signal)
		{
			sdbus::ObjectPath obj_path;
			InterfacesMap interfaces;
			signal >> obj_path >> interfaces;

			if (interfaces.count("org.bluez.Device1"))
			{
				auto device_proxy = sdbus::createProxy(*this->connection, "org.bluez", obj_path);

				device_proxy->uponSignal("PropertiesChanged")
					.onInterface("org.freedesktop.DBus.Properties")
						.call(
							[this, obj_path](const std::string& interface,
								const std::map<std::string, sdbus::Variant>& changed,
								const std::vector<std::string>&)
							{
								if (interface == "org.bluez.Device1" && changed.count("Connected"))
								{
									bool is_connected = changed.at("Connected").get<bool>();
									this->connected_to_host.store(is_connected, std::memory_order_release);
									this->connected_to_host.notify_all();

									if (is_connected)
										std::cout << "BLE device connected: " << obj_path << std::endl;
									else
										std::cout << "BLE device disconnected: " << obj_path << std::endl;
								}
							}
						);
				device_proxy->finishRegistration();
			}
		}
	);
	watcher->finishRegistration();
}

bool BlueZ_Interface::enable(sdbus::IConnection& connection)
{
	if (this->registered.load(std::memory_order_acquire))
		return true;

	try
	{
		// Use the shared D-Bus connection
		this->connection = &connection;

		// Find the Bluetooth adapter
		this->adapter_path = this->find_adapter();
		if (this->adapter_path.empty())
		{
			std::cerr << "No Bluetooth adapter found" << std::endl;
			return false;
		}
		std::cout << "Found adapter: " << this->adapter_path << std::endl;

		// Set adapter properties for HID
		this->adapter_proxy = sdbus::createProxy(*this->connection, "org.bluez", this->adapter_path);

		// Ensure adapter is powered on and discoverable
		try
		{
			this->adapter_proxy->callMethod("Set")
				.onInterface("org.freedesktop.DBus.Properties")
					.withArguments(std::string("org.bluez.Adapter1"),
						std::string("Powered"), sdbus::Variant(true));

			this->adapter_proxy->callMethod("Set")
				.onInterface("org.freedesktop.DBus.Properties")
					.withArguments(std::string("org.bluez.Adapter1"),
						std::string("Discoverable"), sdbus::Variant(true));
		}
		catch (const sdbus::Error& e)
		{
			std::cerr << "Warning: Could not set adapter properties: " << e.getMessage() << std::endl;
		}

		// Build the GATT application tree
		this->app_path = "/io/unikey/Bluetooth";
		this->app = new Application(this->app_path, this->connection);

		this->hid_service = new HIDService;
		this->dev_info_service = new DeviceInfoService;
		this->battery_service = new BatteryService;

		this->app->add_subelement(this->hid_service);
		this->app->add_subelement(this->dev_info_service);
		this->app->add_subelement(this->battery_service);

		// Get references to report characteristics for sending data
		// HIDService subelements: [0]=HIDChar(2a4e), [1]=HIDChar(2a4a), [2]=HIDChar(2a4b), [3]=ReportChar(1), [4]=ReportChar(2)
		// These are accessed through the base class subelements vector
		// ReportChar pointers need to be set after the tree is built
		// (They are subelements index 3 and 4 of the HIDService)

		// Create proxies for BlueZ managers
		this->gatt_manager_proxy = sdbus::createProxy(*this->connection, "org.bluez", this->adapter_path);
		this->ad_manager_proxy = sdbus::createProxy(*this->connection, "org.bluez", this->adapter_path);

		// Register GATT application
		this->register_gatt_application();

		// Create and register advertisement
		this->create_advertisement();
		this->register_advertisement();

		// Start monitoring for device connections
		this->monitor_connection();

		std::cout << "BlueZ interface enabled" << std::endl;
		return true;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Failed to enable BlueZ interface: " << e.what() << std::endl;
		this->disable();
		return false;
	}
}

void BlueZ_Interface::disable()
{
	this->unregister_advertisement();
	this->unregister_gatt_application();

	this->ad_object.reset();
	this->gatt_manager_proxy.reset();
	this->ad_manager_proxy.reset();
	this->adapter_proxy.reset();

	if (this->app != nullptr)
	{
		delete this->app;
		this->app = nullptr;
	}

	this->hid_service = nullptr;
	this->dev_info_service = nullptr;
	this->battery_service = nullptr;
	this->report_char_1 = nullptr;
	this->report_char_2 = nullptr;

	this->connection = nullptr;

	this->connected_to_host.store(false, std::memory_order_release);
	this->advertising.store(false, std::memory_order_release);
	this->registered.store(false, std::memory_order_release);

	std::cout << "BlueZ interface disabled" << std::endl;
}

BlueZ_Interface::~BlueZ_Interface()
{
	this->disable();
}

bool BlueZ_Interface::connection_status() const
{
	return this->connected_to_host.load(std::memory_order_acquire);
}

void BlueZ_Interface::wait_until_connected()
{
	this->connected_to_host.wait(false, std::memory_order_acquire);
}

void BlueZ_Interface::send_hid_report(uint8_t report_id, const void* data, uint64_t data_size) const
{
	if (!this->connected_to_host.load(std::memory_order_acquire))
		return;

	if (data == nullptr || data_size == 0)
		return;

	std::vector<uint8_t> report(static_cast<const uint8_t*>(data),
		static_cast<const uint8_t*>(data) + data_size);

	this->send_hid_report(report_id, report);
}

void BlueZ_Interface::send_hid_report(uint8_t report_id, const std::vector<uint8_t>& report) const
{
	if (!this->connected_to_host.load(std::memory_order_acquire))
		return;

	if (report.empty())
		return;

	/*
		Send HID report by emitting a PropertiesChanged signal on the
		appropriate Report characteristic. BlueZ picks this up and sends
		it as a GATT notification to the connected host.

		The HID service structure has ReportChar at subelement indices 3 and 4,
		corresponding to report IDs 1 and 2.
	*/

	// Determine which characteristic to notify on based on report_id
	Characteristic* target = nullptr;

	if (this->hid_service != nullptr)
	{
		// Access through the public accessor
		// Index 3 = ReportChar(id=1), Index 4 = ReportChar(id=2)
		if (report_id == 1 && (*this->hid_service)().size() > 3)
			target = static_cast<Characteristic*>((*this->hid_service)[3]);
		else if (report_id == 2 && (*this->hid_service)().size() > 4)
			target = static_cast<Characteristic*>((*this->hid_service)[4]);
	}

	if (target == nullptr)
	{
		std::cerr << "No report characteristic found for report ID " 
			<< static_cast<int>(report_id) << std::endl;
		return;
	}

	// Emit PropertiesChanged to trigger GATT notification
	std::map<std::string, sdbus::Variant> changed;
	changed["Value"] = sdbus::Variant(report);

	target->emit_properties_changed("org.bluez.GattCharacteristic1", changed);
}