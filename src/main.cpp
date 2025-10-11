#include "Device.hpp"
#include "libevdev/libevdev.h"
#include <unikey.hpp>

#include <iostream>
#include <string>
// #include <thread>

int main(int argc, char* argv[])
{
	Device::set_event_processor(&print_event);
	Device::initialize_devices("/dev/input");

	for (;;)
	{
		std::string input;
		std::cin >> input;
		if (input == "c")
		{
			Device::trigger_exit();
			break;
		}
		if (input == "t")
		{
			Device::trigger_activation();
		}
	}

	return 0;
}
