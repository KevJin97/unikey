#include "WiFi/unikey-wifi.hpp"
#include "WiFi/Virtual_Device.hpp"
#include "WiFi/WiFi_Client.hpp"
#include "WiFi/WiFi_Server.hpp"
#include "Core/Device.hpp"
#include "unikey.hpp"

#include <iostream>
#include <thread>

#include <sdbus-c++/IObject.h>
#include <sdbus-c++/Message.h>

std::unique_ptr<sdbus::IObject> unikey_wifi_dbus_obj;
static WiFi_Client* messenger_wifi = nullptr;

void register_wifi_dbus_cmds()
{
	unikey_wifi_dbus_obj = sdbus::createObject(*unikey_dbus_connection, "/io/unikey/WiFi");

	unikey_wifi_dbus_obj->registerMethod("io.unikey.WiFi.Methods",
		"WiFiConnectTo", "s", "", &dbus_connect_to_ip);
	
	unikey_wifi_dbus_obj->registerMethod("ToggleWiFiServer")
		.onInterface("io.unikey.WiFi.Methods")
			.implementedAs(&dbus_toggle_unikey_server);
	
	unikey_wifi_dbus_obj->finishRegistration();
}

void dbus_connect_to_ip(sdbus::MethodCall call)
{
	std::string ip_addr_str;
	call >> ip_addr_str;
	call.createReply().send();
	
	if (messenger_wifi != nullptr)
	{
		delete messenger_wifi;
		messenger_wifi = nullptr;
	}

	messenger_wifi = new WiFi_Client;
	messenger_wifi->set_server_addr(ip_addr_str.c_str());
	messenger_wifi->connect_to_server();

	Device::set_event_processor(
		[](const void* data, uint64_t unit_size)
		{
			messenger_wifi->send_formatted_data(data, unit_size);
		}
	);

	std::thread send_init_virtual_device_data(
		[&]()
		{
			messenger_wifi->wait_until_connected();

			messenger_wifi->send_unformatted_data(
				Device::return_enabled_global_key_states().return_vector().data(),
				sizeof(uint64_t),
				Device::return_enabled_global_key_states().return_vector().size()
			);
		}
	);
	send_init_virtual_device_data.detach();
}

void dbus_toggle_unikey_server()
{
	static std::atomic_bool unikey_server_status = false;
	static WiFi_Server dev_server(42069);
	auto launch_server = [&]
	{
		std::thread server_event_loop([&]
		{
			Virtual_Device virt_unikey("Unikey HID Device");
			uint64_t* p_data = nullptr;

			while(unikey_server_status.load(std::memory_order_acquire) == false)
			{
				dev_server.begin_listening().wait_for_connection();
				std::cout << "Device Connected" << std::endl;
				p_data = (uint64_t*)dev_server.read_sent_data();	// Get enabled EV_KEY codes
				virt_unikey.enable_codes(EV_KEY, std::vector<uint64_t>(1 + p_data, *p_data + p_data));
				free(p_data);
				
				p_data = (uint64_t*)dev_server.read_sent_data();	// Get Enabled EV_REL codes
				virt_unikey.enable_codes(EV_REL, std::vector<uint64_t>(1 + p_data, *p_data + p_data));	// Enables 
				free(p_data);

				while (dev_server.is_connected_to_client())
				{
					if ((p_data = (uint64_t*)dev_server.read_sent_data()) != nullptr)
					{
						virt_unikey.write_event((struct input_event*)(1 + p_data), *p_data);	// Write received inputs
						virt_unikey.write_event();	// Sends SYN_REPORT as default arguments
						free(p_data);
					}
				}
				virt_unikey.clear();
				virt_unikey.set_device_name("Unikey HID Device");
			}
		});
		server_event_loop.detach();
	};

	if (unikey_server_status.load(std::memory_order_acquire) == false)
	{
		std::cout << "Server launched" << std::endl;
		unikey_server_status.store(true, std::memory_order_release);
		launch_server();
	}
	else
	{
		std::cout << "Closing server" << std::endl;

		unikey_server_status.store(false, std::memory_order_release);
		dev_server.close_connection();
	}
}