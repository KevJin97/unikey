#include "BitField.hpp"
#include "Device.hpp"
#include <unikey.hpp>

#include <iostream>
#include <string>
// #include <thread>
#include <grp.h>

int main()
{
	const int orig_groupID = change_group_permissions();
	
	std::vector<Device*> device_list;
	std::vector<BitField> enabled_types = initialize_all_devices(device_list);

	// std::thread server_process(unikey_server);
	return_to_original_group_permissions(orig_groupID);
	/*
	for (std::size_t n = 0; n < device_list.size(); ++n)
	{
		if (n != 3)
		{
			std::cout << "Disabled event" << n << std::endl;
			device_list[n]->disable_device();
		}
	}
	*/
	std::string input;

	while (input != "exit")
	{
		std::cout << "Command: ";
		std::cin >> input;

		if (input == "trigger")
		{
			std::cout << (Device::trigger_activation() ? "Devices have been activated" : "Devices have been deactivated") << std::endl;
		}
		else if (input == "monitor")
		{
			Device::begin_monitoring();
		}
	}
	// server_process.join();
	
	for (std::size_t n = 0; n < device_list.size(); ++n)
	{
		delete device_list[n];
	}

	return 0;
}
