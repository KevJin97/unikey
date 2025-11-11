#include <cstdint>
#include <iostream>
#include <linux/input.h>
#include <stdint.h>
#include <iostream>
//#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>

#include "unikey.hpp"
#include "Virtual_Device.hpp"
#include "WiFi_Server.hpp"

#define EVER ;;

int main()
{
	int old_gid = change_group_permissions();

	Virtual_Device virt_unikey("Unikey HID Device");
	uint64_t* p_data = nullptr;
	WiFi_Server dev_server(42069);

	for(EVER)
	{
		dev_server.begin_listening().wait_for_connection();
		std::cout << "Device Connected" << std::endl;
		p_data = (uint64_t*)dev_server.read_sent_data_packet();	// Get enabled EV_KEY codes
		virt_unikey.enable_codes(EV_KEY, std::vector<uint64_t>(1 + p_data, 1 + *p_data + p_data));
		free(p_data);
		p_data = (uint64_t*)malloc(sizeof(uint64_t) + sizeof(struct input_event) * 64);
		
		while (dev_server.is_connected_to_client())
		{
			if (dev_server.read_sent_data(p_data) == nullptr)
			{
				dev_server.close_connection();
			}
			else if (*p_data == 0)
			{
				dev_server.close_connection();
				goto BREAK_LOOP;
			}
			else
			{
				virt_unikey.write_event((struct input_event*)(1 + p_data), *p_data);	// Write received inputs
				virt_unikey.write_event();	// Sends SYN_REPORT as default arguments
			}
		}

		free(p_data);
		p_data = nullptr;
		virt_unikey.clear();
		std::cout << "Device has been disconnected" << std::endl;
	}

	BREAK_LOOP:
	std::cout << "Shutting down unikey server..." << std::endl;
	free(p_data);
	virt_unikey.clear();
	return_to_original_group_permissions(old_gid);

	return 0;
}
