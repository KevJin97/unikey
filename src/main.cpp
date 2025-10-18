#include "Device.hpp"
#include <unikey.hpp>

#include <iostream>
#include <string>
#include <stdlib.h>
// #include <thread>

int main(int argc, char* argv[])
{
	std::cout << std::endl;
	// Device::set_event_processor(&print_event);
	if (argc == 2)
	{
		std::cout << "Timeout Time set to: " << Device::set_timeout_length(atoi(argv[1])) << 's' << std::endl;
	}
	std::cout << "Initializing all available input sources..." << std::endl;
	Device::initialize_devices("/dev/input");
	std::cout << "Devices have been initialized" << std::endl;
	for (;;)
	{
		std::string input;
		if (Device::return_grab_state() != true)
		{
			std::cout << "Enter Input: ";
		}
		std::cin >> input;
		if (input == "exit")
		{
			std::cout << "Exiting..." << std::endl;
			Device::trigger_exit();
			break;
		}
		else if (input == "trigger")
		{
			std::cout << "Grabbing device inputs..." << std::endl;
			Device::trigger_activation();
			std::cout << "---GRABBED---\n" << std::endl;
		}
		else
		{
			continue;
		}
	}
	std::cout << "Process 'unicode' has successfully exited" << std::endl;
	return 0;
}
