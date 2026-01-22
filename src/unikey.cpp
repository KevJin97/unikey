#include "unikey.hpp"
#include "BitField.hpp"
#include "Device.hpp"
#include "Virtual_Device.hpp"
#include "WiFi_Server.hpp"

#include <atomic>
#include <iostream>

#include <grp.h>
#include <asm-generic/socket.h>
#include <libudev.h>
#include <linux/input.h>
#include <memory>
#include <sdbus-c++/IObject.h>
#include <sys/socket.h>

#include <sdbus-c++/Message.h>

std::unique_ptr<sdbus::IConnection> unikey_dbus_connection;
std::unique_ptr<sdbus::IObject> unikey_device_dbus_obj;
std::unique_ptr<sdbus::IObject> unikey_wifi_dbus_obj;

void register_to_dbus()
{
	unikey_dbus_connection = sdbus::createSystemBusConnection("io.unikey");
	
	register_device_dbus_cmds();
	register_wifi_dbus_cmds();
	
	unikey_dbus_connection->enterEventLoopAsync();
}

void register_device_dbus_cmds()
{
	unikey_device_dbus_obj = sdbus::createObject(*unikey_dbus_connection, "/io/unikey/Device");

	unikey_device_dbus_obj->registerMethod("io.unikey.Device.Methods",
		"SetTimeout", "u", "", &dbus_set_timeout_cmd);

	unikey_device_dbus_obj->registerMethod("Trigger")
		.onInterface("io.unikey.Device.Methods")
			.implementedAs(&dbus_trigger_cmd);

	unikey_device_dbus_obj->registerMethod("Exit")
		.onInterface("io.unikey.Device.Methods")
			.implementedAs(&Device::trigger_exit);

	unikey_device_dbus_obj->finishRegistration();
}

void register_wifi_dbus_cmds()
{
	unikey_wifi_dbus_obj = sdbus::createObject(*unikey_dbus_connection, "/io/unikey/WiFi");

	unikey_wifi_dbus_obj->registerMethod("io.unikey.WiFi.Methods",
		"ConnectTo", "s", "", &dbus_connect_to_ip);
	
	unikey_wifi_dbus_obj->registerMethod("ToggleServer")
		.onInterface("io.unikey.WiFi.Methods")
			.implementedAs(&dbus_toggle_unikey_server);
	
	unikey_wifi_dbus_obj->finishRegistration();
}

void dbus_trigger_cmd()
{
	std::cout << (Device::trigger_activation() ? "\n---GRABBED---" : "\n--UNGRABBED--") << std::endl;
}

void dbus_set_timeout_cmd(sdbus::MethodCall call)
{
	uint32_t seconds;
	call >> seconds;
	Device::set_timeout_length(seconds);
	call.createReply().send();
}

void dbus_connect_to_ip(sdbus::MethodCall call)
{
	std::string ip_addr_str;
	call >> ip_addr_str;
	call.createReply().send();
	
	messenger_wifi.set_server_addr(ip_addr_str.c_str());
	messenger_wifi.connect_to_server();
	Device::set_event_processor(send_formatted_data_wifi);

	std::thread send_init_virtual_device_data([&]
	{
		messenger_wifi.wait_until_connected();
		messenger_wifi.send_unformatted_data(Device::return_enabled_global_key_states().return_vector().data(),
			sizeof(uint64_t), Device::return_enabled_global_key_states().return_vector().size());
	});
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

void send_formatted_data_wifi(const void* data, uint64_t unit_size)
{
	messenger_wifi.send_formatted_data(data, unit_size);
}

int change_group_permissions()
{
	// Set Permissions
	auto grp = getgrnam("input");
	if (grp == NULL)
	{
		std::cerr << "getgrnam(\"input\") failed" << std::endl;
		return -1;
	}
	int oldgid = getgid();
	if (setgid(grp->gr_gid) < 0)
	{
		std::cerr << "Failed to change group to input" << std::endl;
		return -1;
	}
	
	return oldgid;
}

int return_to_original_group_permissions(int gid)
{
	if (setgid(gid) < 0)
	{
		std::cerr << "Could not return Group ID back to original" << std::endl;
		return -1;
	}
	return 0;
}

/*
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

void broadcast_service()
{
	int broadcast_socket = socket(AF_INET, SOCK_DGRAM, 0);
	int broadcast_enable = 1;
	setsockopt(broadcast_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));

	struct sockaddr_in broadcast_address;
	broadcast_address.sin_family = AF_INET;
	broadcast_address.sin_port = htons(8888);
	broadcast_address.sin_addr.s_addr = inet_addr("255.255.255.255");
	const char* msg = "Unikey Server";

	for (;;)
	{
		if (broadcast_service_status.load(std::memory_order_acquire) == false)
		{
			broadcast_service_status.wait(false, std::memory_order_acquire);
		}
		sendto(broadcast_socket, msg, strlen(msg), 0, (struct sockaddr*)&broadcast_address, sizeof(broadcast_address));
		sleep(5);
	}
	close(broadcast_socket);
}
*/