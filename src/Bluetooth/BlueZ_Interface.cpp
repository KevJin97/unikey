#include "Bluetooth/BlueZ_Interface.hpp"
#include "Bluetooth/BlueZ_HID_Services.hpp"
#include "Bluetooth/Gatt/Application.hpp"
#include "Bluetooth/Gatt/Base_App_Obj.hpp"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
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

	this->ad_object->registerProperty("Type")
		.onInterface("org.bluez.LEAdvertisement1")
			.withGetter(
				[]()
				{
					return std::string("peripheral");
				}
			);
	
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
	
	this->ad_object->registerProperty("Appearance")
		.onInterface("org.bluez.LEAdvertisement1")
			.withGetter(
				[]()
				{
					return uint16_t(0x03c1);
				}
			);

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

void BlueZ_Interface::monitor_connection()
{
	if (!this->bluez_proxy) return;

	std::unique_ptr<sdbus::IProxy> watcher = sdbus::createProxy(*this->dbus_connection, "org.bluez", "/");
	watcher->registerSignalHandler("org.freedesktop.DBus.ObjectManager", "InterfacesAdded", 
		[this](sdbus::Signal signal)
		{
			sdbus::ObjectPath obj_path;
			InterfacesMap interfaces;
			signal >> obj_path >> interfaces;

			if (interfaces.contains("org.bluez.Device1"))
			{
				std::unique_ptr<sdbus::IProxy> device_proxy = sdbus::createProxy(*this->dbus_connection, "org.bluez", obj_path);
				
				device_proxy->uponSignal("PropertiesChanged")
					.onInterface("org.freedesktop.DBus.Properties")
						.call(
							[this, obj_path](const std::string& interface, const std::map<std::string, sdbus::Variant>& changed, const std::vector<std::string>&)
							{
								if (interface == "org.bluez.Device1" && changed.contains("Connected"))
								{
									this->connected_to_host.store(changed.at("Connected").get<bool>(), std::memory_order_release);
									this->connected_to_host.notify_all();

									if (this->connected_to_host.load(std::memory_order_acquire))
									{
										std::cout << "BLE device connected: " << obj_path << std::endl;

										try
										{
											// Allow devices to reconnect without re-pairing
											sdbus::createProxy(*this->dbus_connection, "org.bluez", obj_path)->setProperty("Trusted")
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
							}
						);
				
				device_proxy->finishRegistration();
			}
		}
	);
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

			this->bluez_proxy->Powered(true);
			this->bluez_proxy->Discoverable(true);
			this->bluez_proxy->Pairable(true);
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
	// Set configurations back to original values
	this->bluez_proxy->Powered(this->orig_powered_state);
	this->bluez_proxy->Discoverable(this->orig_discover_state);
	this->bluez_proxy->Pairable(this->orig_pairable_state);

	this->unregister_advertisement();
	this->unregister_gatt_application();
	this->unregister_agent();

	this->ad_object.reset();
	this->bluez_proxy.reset();

	if (this->gatt_app != nullptr)
	{
		delete this->gatt_app;
		this->gatt_app = nullptr;
	}

	this->new_connection.reset();
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

	Characteristic* target = nullptr;

	if (this->gatt_app->get_subelement(0) != nullptr)
	{
		target = (Characteristic*)this->gatt_app->get_subelement(0)->get_subelement(report_id + 2);
	}

	if (target == nullptr)
	{
		std::cerr << "No report characteristic found for report ID " << (int)report_id << std::endl;
		return;
	}

	//PropertiesMap changed;
	//changed["Value"] = sdbus::Variant(report);
	target->emit_properties_changed("org.bluez.GattCharacteristic1", { std::pair<std::string, sdbus::Variant>("Value", report) });
}