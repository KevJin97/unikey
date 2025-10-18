#include "Device.hpp"
#include <unikey.hpp>

#include <iostream>
#include <string>
// #include <thread>

int main(int argc, char* argv[])
{
	// Device::set_event_processor(&print_event);
	Device::set_timeout_length(5);
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
			std::cout << "Devices have been grabbed" << std::endl;
		}
		else
		{
			continue;
		}
	}
	std::cout << "Process 'unicode' has successfully exited" << std::endl;
	return 0;
}
