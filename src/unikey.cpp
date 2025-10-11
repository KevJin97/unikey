#include "unikey.hpp"
#include "BitField.hpp"

#include <asm-generic/socket.h>
#include <atomic>
#include <iostream>
#include <sys/socket.h>
#include <vector>
#include <filesystem>
#include <grp.h>

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