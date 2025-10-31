#include "unikey.hpp"
#include "Device.hpp"

#include <iostream>

#include <grp.h>
#include <asm-generic/socket.h>
#include <libudev.h>
#include <sys/socket.h>

#include <sdbus-c++/Message.h>

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