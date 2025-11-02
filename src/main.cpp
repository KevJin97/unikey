#include "Device.hpp"
// #include <sys/mman.h>
#include <unikey.hpp>

#include <iostream>
#include <string>
#include <stdlib.h>
#include <sys/poll.h>
// #include <thread>

int main()
{
	int old_gid = change_group_permissions();
	std::cout << std::endl;
	//messenger_wifi.connect_to_server("100.79.165.10");
	//messenger_wifi.wait_until_connected();
	//uint64_t hi = 67;
	//messenger_wifi.send_unformatted_data(&hi, sizeof(uint64_t));

	std::cout << "Initializing all available input sources..." << std::endl;
	Device::initialize_devices("/dev/input");
	std::cout << "Devices have been initialized..." << std::endl;
	
	std::cout << "Begin unikey..." << std::endl;
	
	register_to_dbus();
	Device::wait_for_exit();
	unikey_dbus_connection->leaveEventLoop();
	
	std::cout << "Process 'unikey' has exited successfully" << std::endl;

	return_to_original_group_permissions(old_gid);
	return 0;
}
