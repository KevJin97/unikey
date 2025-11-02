#include <cstdint>
#include <iostream>
#include <stdint.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring> // For memset
#include <vector>

#include "Virtual_Device.hpp"
#include "WiFi_Server.hpp"

int main()
{
	Virtual_Device virt_unikey("Unikey HID Device");

	uint64_t* p_data = nullptr;
	WiFi_Server dev_server(42069);
	for (unsigned msg_cnt = 0;; msg_cnt = 0)
	{
		dev_server.begin_listening().wait_for_connection();
		std::cout << "Device Connected" << std::endl;
		p_data = (uint64_t*)dev_server.read_sent_data();	// Get enabled EV_KEY codes
		virt_unikey.enable_codes(EV_KEY, std::vector<uint64_t>(1 + p_data, *p_data + p_data));
		free(p_data);
		
		p_data = (uint64_t*)dev_server.read_sent_data();	// Get Enabled EV_REL codes
		virt_unikey.enable_codes(EV_REL, std::vector<uint64_t>(1 + p_data, *p_data + p_data));	// Enables 
		free(p_data);

		std::cout << "Client has been connected" << std::endl;
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
		std::cout << "Client has been disconnected" << std::endl;
	}

	return 0;
}
