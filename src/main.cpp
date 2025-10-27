#include "Device.hpp"
// #include <sys/mman.h>
#include <unikey.hpp>

#include <iostream>
#include <string>
#include <stdlib.h>
#include <sys/poll.h>
// #include <thread>

int main(int argc, char* argv[])
{

	std::cout << std::endl;
	// Device::set_event_processor(&print_event);
	if (argc == 2)
		std::cout << "Timeout Time set to: " << Device::set_timeout_length(atoi(argv[1])) << 's' << std::endl;

	std::cout << "Initializing all available input sources..." << std::endl;
	Device::initialize_devices("/dev/input");
	std::cout << "Devices have been initialized" << std::endl;
	std::cout << "Begin unikey" << std::endl;
	
	//register_to_dbus();
	std::unique_ptr<sdbus::IConnection> unikey_dbus_connection
		= sdbus::createSystemBusConnection("io.unikey.Device");
	std::unique_ptr<sdbus::IObject> unikey_dbus_obj
		= sdbus::createObject(*unikey_dbus_connection, "/io/unikey/Device");
	unikey_dbus_obj->registerMethod("Trigger")
		.onInterface("io.unikey.Device.Methods")
			.implementedAs(&Device::trigger_activation);
	unikey_dbus_obj->registerMethod("Exit")
		.onInterface("io.unikey.Device.Methods")
			.implementedAs(&Device::trigger_exit);
	unikey_dbus_obj->finishRegistration();
	unikey_dbus_connection->enterEventLoopAsync();

	Device::wait_for_exit();
	
	std::cout << "Process 'unicode' has successfully exited" << std::endl;

	return 0;
}
