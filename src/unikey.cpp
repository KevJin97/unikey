#include "unikey.hpp"
#include "Bluetooth/unikey-bluetooth.hpp"
#include "Core/Device.hpp"
#include "WiFi/unikey-wifi.hpp"

#include <iostream>
#include <memory>

#include <asm-generic/socket.h>
#include <grp.h>
#include <libudev.h>
#include <linux/input.h>
#include <sys/socket.h>

#include <sdbus-c++/IObject.h>
#include <sdbus-c++/Message.h>

std::unique_ptr<sdbus::IConnection> unikey_dbus_connection;
std::unique_ptr<sdbus::IObject> unikey_root_dbus_obj;
std::unique_ptr<sdbus::IObject> unikey_device_dbus_obj;

void register_to_dbus()
{
	// Initialize the D-Bus connection
	unikey_dbus_connection = sdbus::createSystemBusConnection("io.unikey");
	
	// Create an object at the root
	unikey_root_dbus_obj = sdbus::createObject(*unikey_dbus_connection, "/io/unikey");
	unikey_root_dbus_obj->addObjectManager();
	unikey_root_dbus_obj->finishRegistration();
	

	// Add additional functionality to D-Bus
	register_device_dbus_cmds();
	register_bluetooth_dbus_cmds();
	register_wifi_dbus_cmds();

	// Begin listening to D-Bus Signals
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