#include "Device.hpp"
#include "shm_unikey.hpp"
#include <atomic>
#include <sys/mman.h>
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
	{
		std::cout << "Timeout Time set to: " << Device::set_timeout_length(atoi(argv[1])) << 's' << std::endl;
	}
	std::cout << "Initializing all available input sources..." << std::endl;
	Device::initialize_devices("/dev/input");
	std::cout << "Devices have been initialized" << std::endl;

	void* ptr = shm_ptr_addr("/shm_unikey", sizeof(std::atomic_bool) * 2);
	std::atomic_bool* event_signal = new(ptr) std::atomic_bool[2](false);
	struct pollfd pfd;
	pfd.fd = shm_open("/shm_unikey", O_RDWR, 0660);
	pfd.events = POLLIN;

	for (;;)
	{
		if (event_signal[0].exchange(false, std::memory_order_acq_rel))
		{
			std::cout << "Grabbing device inputs..." << std::endl;
			Device::trigger_activation();
			std::cout << "---GRABBED---\n" << std::endl;
		}
		else if (event_signal[1].exchange(false, std::memory_order_acq_rel))
		{
			std::cout << "Exiting..." << std::endl;
			Device::trigger_exit();
			break;
		}
		else
		{
			if (poll(&pfd, 1, -1) < 0)	// Handle polling error
			{
				std::cerr << "Input polling failed: " << strerror(errno) << std::endl;
				break;
			}
		}
	}
	std::cout << "Process 'unicode' has successfully exited" << std::endl;

	munmap(ptr, sizeof(std::atomic_bool) * 2);
	shm_unlink("/shm_unikey");

	return 0;
}
