#include "Device.hpp"
#include "Key_Device.hpp"
#include "Abs_Device.hpp"
#include "Rel_Device.hpp"
#include "BitField.hpp"
#include "Shared_Cyclic_Queue.hpp"

#include "libevdev/libevdev.h"

#include <atomic>
#include <iostream>
#include <cerrno>
#include <cstdint>
#include <ctime>
#include <fcntl.h>

#include <poll.h>
#include <sys/poll.h>

Device::Device()
{
	this->dev = nullptr;
	this->is_active = false;
}

Device* Device::create_device(const std::string& event_path)
{
	int fd = open(event_path.c_str(), O_RDONLY | O_NONBLOCK);
	struct libevdev* dev;

	if (fd < 0)
	{
		close(fd);
		return nullptr;
	}
	else if (libevdev_new_from_fd(fd, &dev) >= 0)
	{
		if (libevdev_has_event_type(dev, EV_ABS))
		{
			return new Abs_Device(dev);
		}
		else if (libevdev_has_event_type(dev, EV_REL))
		{
			return new Rel_Device(dev);
		}
		else if (libevdev_has_event_type(dev, EV_KEY))
		{
			return new Key_Device(dev);
		}
		else
		{
			close(fd);
			libevdev_free(dev);
			return nullptr;
		}
	}
	else
	{
		libevdev_free(dev);
		return nullptr;
	}
}

void Device::set_event_processor(void (**func_ptr)(const struct input_event &))
{
	Device::p_event_processor = *func_ptr;
}

void Device::init_watchdog()
{
	if (Device::is_grabbed.load(std::memory_order_acquire))
	{
		Device::is_grabbed.store(false, std::memory_order_release);
		Device::watchdog_thread = std::thread(watchdog_process);
		Device::is_grabbed.notify_all();
	}
	else
	{
		// Error: Already initialized
	}
}

void Device::set_timeout_length(unsigned secs)
{
	Device::period = secs;
}

void Device::set_activation_cmd()
{
	Device::is_record.store(true, std::memory_order_release);
	Device::is_grabbed.store(true, std::memory_order_release);
	Device::is_grabbed.notify_all();
}

void Device::trigger_activation()
{
	Device::is_grabbed.store(!Device::is_grabbed.load(std::memory_order_acquire), std::memory_order_release);
	Device::is_grabbed.notify_all();
}

void Device::wait_to_exit()
{
	struct pollfd pfd;
	pfd.fd = Device::poll_signal_fd;
	pfd.events = POLLIN;

	Device::is_exit.wait(false, std::memory_order_acquire);	// Wait until exit signal is triggered

	uint64_t signal = 1;
	while (write(Device::poll_signal_fd, &signal, sizeof(uint64_t)) < 0);	// While write to signaler fails
	
	Device::watchdog_thread.join();	// Wait for Watchdog thread to complete
	close(Device::poll_signal_fd);
}

void Device::exit()
{
	Device::is_exit.store(true, std::memory_order_release);

	if (Device::is_grabbed.load(std::memory_order_acquire) == true)
	{
		Device::is_grabbed.store(false, std::memory_order_release);
	}
	else
	{
		Device::is_grabbed.store(true, std::memory_order_acquire);
		Device::is_exit.notify_all();
		
		for (uint16_t n = 0; n < UINT16_MAX && Device::active_devices.load(std::memory_order_acquire) != 0; ++n)
		{
			Device::is_grabbed.notify_all();	// For those waiting on Device::is_grabbed, notification is spammed
		}
	}
}

BitField Device::return_enabled_event_codes(unsigned int type)
{
	BitField enabled;

	for (unsigned n = 0; n < libevdev_event_type_get_max(type); ++n)
	{
		if (libevdev_has_event_code(this->dev, type, n))
		{
			enabled.insert(n);
		}
	}
	
	return enabled;
}

void Device::toggle_device()
{

}

void Device::watchdog_process()
{
	struct pollfd pfd;
	pfd.fd = Device::poll_signal_fd;
	pfd.events = POLLIN;

	// Don't do anything until at least one device is initialized
	Device::active_devices.wait(0, std::memory_order_acquire);

	// Watchdog Loop
	while (Device::is_exit.load(std::memory_order_acquire) == false)
	{
		if (Device::pending_events_to_process.load(std::memory_order_acquire) != 0)	// If there are events to process
		{
			Device::p_event_processor(Device::queue.pop());
			Device::pending_events_to_process.fetch_sub(1, std::memory_order_acq_rel);
		}
		else if (Device::is_grabbed.load(std::memory_order_acquire) == true)	// If it's in a grab state but no pending events
		{
			if (poll(&pfd, 1, Device::period * 1000) < 0)	// Either detects a signal or it times out
			{
				std::cerr << "Poll failed: " << strerror(errno) << std::endl;	// Error while polling
				break;
			}

			if (((pfd.revents & POLLIN) == 0) && (Device::pending_events_to_process.load(std::memory_order_acquire) == 0))
			{
				Device::is_grabbed.store(false, std::memory_order_release);
			}
		}
		else	// Wait for a grab state to be triggered
		{
			Device::is_grabbed.wait(false, std::memory_order_acquire);
		}
	}
}