#include "Bluetooth/BlueZ_HID_Services.hpp"
#include "Bluetooth/Gatt/Application.hpp"
#include "Bluetooth/Gatt/Base_App_Obj.hpp"

#include <iostream>
#include <memory>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Types.h>

std::string find_adapter(const std::unique_ptr<sdbus::IConnection>& g_connection)
{
	auto proxy = sdbus::createProxy(*g_connection, "org.bluez", "/");
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

int main()
{
	try
	{
		std::unique_ptr<sdbus::IConnection> g_connection = sdbus::createSystemBusConnection("io.unikey");
		std::string adapter = find_adapter(g_connection);

		if (adapter.empty())
		{
			std::cerr << "Adapter not found" << std::endl;
			return 1;
		}
		std::cout << "Found adapter: " << adapter << std::endl;

		Application app("/io/unikey/Bluetooth", g_connection.get());
		app.add_subelement(new HIDService);
		app.add_subelement(new DeviceInfoService);
		app.add_subelement(new BatteryService);

		auto gattManager = sdbus::createProxy(*g_connection, "org.bluez", adapter);
		OptionsMap options;

		std::cout << "Registering GATT app at " << app.get_full_path() << " (Async)..." << std::endl;
		gattManager->callMethodAsync("RegisterApplication")
			.onInterface("org.bluez.GattManager1")
				.withArguments(sdbus::ObjectPath(app.get_full_path()), options)
					.uponReplyInvoke(
						[](const sdbus::Error* error)
						{
							if (error)
							{
								std::cerr << "Registration Failed: " << error->getMessage() << std::endl;
							}
							else
							{
								std::cout << "Registration Successful!" << std::endl;
							}
						}
					);
		
		std::cout << "Entering event loop..." << std::endl;
		g_connection->enterEventLoop();
	}

	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}