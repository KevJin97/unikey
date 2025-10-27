#include "Device.hpp"
#include "BitField.hpp"

#include "libevdev/libevdev.h"

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <iostream>
#include <cerrno>
#include <cstdint>
#include <ctime>
#include <fcntl.h>

#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/poll.h>

void Device::set_event_processor(void (*event_processing_function)(struct input_event*, uint64_t))
{
	Device::event_process = event_processing_function;
}

void Device::initialize_devices(const std::string &directory)
{
	if (Device::active_devices.load() == 0)
	{
		if (Device::poll_signal_fd < 0)
		{
			std::cerr << "Poll initialization failed: " << strerror(errno) << std::endl;
			return;
		}

		Device::watchdog_thread = std::thread(watchdog_process);
	}

	std::string fullpath = directory.ends_with("/") ? directory.substr(0, directory.size() - 1) : directory;

	for (const auto& entry : std::filesystem::directory_iterator(fullpath.c_str()))
	{
		if (entry.path().filename().string().starts_with("event"))
		{
			Device* p_device = new Device(fullpath + "/" + entry.path().filename().string());
			if (p_device->dev != nullptr)
			{
				Device::device_objects.push_back(p_device);
			}
		}
	}
}

unsigned Device::set_timeout_length(unsigned int seconds)
{
	constexpr unsigned MINMAX_TIME[2] = { 1, 900 };
	
	Device::is_grabbed.wait(true);
	if (seconds < MINMAX_TIME[0])	// Warning: minimum time is 1 second
		seconds = MINMAX_TIME[0];
	else if (seconds > MINMAX_TIME[1])	// Warning: maximum time is 15 minutes
		seconds = MINMAX_TIME[1];

	Device::timeout_length = seconds * 1000;
	return seconds;
}

void Device::trigger_activation()
{
	Device::is_grabbed.exchange(!Device::is_grabbed.load(std::memory_order_acquire), std::memory_order_acq_rel);

	uint64_t message = Device::active_devices.load(std::memory_order_acquire);
	write(Device::event_signal_fd, &message, sizeof(uint64_t));

	Device::is_grabbed.notify_all();
}

void Device::trigger_exit()
{
	Device::is_exit.store(true, std::memory_order_release);
	if (Device::is_grabbed.load() == false)
		Device::trigger_activation();

	uint64_t buffer = Device::active_devices.load(std::memory_order_acquire);
	write(Device::event_signal_fd, &buffer, sizeof(uint64_t));
	Device::watchdog_thread.join();
	// Notify event handler that devices are exited

	for (std::size_t n = 0; n < Device::device_objects.size(); ++n)
	{
		if (Device::device_objects[n] != nullptr)
		{
			delete Device::device_objects[n];
			Device::device_objects[n] = nullptr;
		}
	}
	Device::is_exit.notify_all();
}

bool Device::return_grab_state()
{
	return Device::is_grabbed.load(std::memory_order_acquire);
}

void Device::watchdog_process()
{
	struct pollfd pfd;
	pfd.fd = Device::poll_signal_fd;
	pfd.events = POLLIN;

	Device::active_devices.wait(0);
	while (Device::is_exit.load(std::memory_order_acquire) == false)
	{
		if (Device::pending_events.load(std::memory_order_acquire))
		{
			uint64_t event_count = 0;
			read(pfd.fd, &event_count, sizeof(uint64_t));
			void* p_data = Device::global_queue.pop();

			if (p_data != nullptr)
			{
				uint64_t* message_length = (uint64_t*)p_data;
				struct input_event* p_events = (struct input_event*)(message_length + 1);

				Device::event_process(p_events, *message_length);
				Device::pending_events.fetch_sub(1, std::memory_order_acq_rel);
				free(p_data);
			}
		}
		else if (Device::is_grabbed.load(std::memory_order_acquire))
		{
			// Only timeout watchdog if no keys are being pressed down
			if (poll(&pfd, 1, 
				(
					Device::global_key_press_cnt.load(std::memory_order_acquire)
					? -1
					: Device::timeout_length
				)) < 0)
			{
				std::cerr << "Poll failed: " << strerror(errno) << std::endl;   // Error while polling
			}

			if ((pfd.revents & POLLIN) == 0)	// If broke out of polling and no new events are waiting
			{
				Device::trigger_activation();
			}
		}
		else
		{
			Device::is_grabbed.wait(false);
		}
	}

	// Close everything out
	while (uint32_t active_devices_left = Device::active_devices.load())
	{
		Device::active_devices.wait(active_devices_left);
	}
	while (Device::global_queue.size())
	{
		free(global_queue.pop());
	}

	close(Device::event_signal_fd);
	close(Device::poll_signal_fd);
}

Device::Device(const std::string& filepath)
{
	int fd = open(filepath.c_str(), O_RDONLY | O_NONBLOCK);
	if (fd < 0)
	{
		close(fd);
		return;
	}
	if (libevdev_new_from_fd(fd, &this->dev) >= 0)
	{
		if (libevdev_has_event_type(this->dev, EV_KEY) || libevdev_has_event_type(this->dev, EV_REL) || libevdev_has_event_type(this->dev, EV_ABS))
		{
			this->input_monitor_thread = std::thread(std::bind(&Device::input_monitor_process, this));
			Device::active_devices.fetch_add(1, std::memory_order_acq_rel);
			Device::active_devices.notify_all();
			return;
		}
	}

	close(fd);
	libevdev_free(this->dev);
	this->dev = nullptr;
}

Device::~Device()
{
	this->input_monitor_thread.join();

	close(libevdev_get_fd(this->dev));
	libevdev_free(this->dev);
	this->dev = nullptr;
}

void Device::input_monitor_process()
{
	static constexpr uint64_t add_to_count = 1;
	static constexpr enum libevdev_grab_mode grab_state[2] = { LIBEVDEV_UNGRAB, LIBEVDEV_GRAB };

	//uint8_t read_event = 0;	// Counter to prevent constant checks and enter polling mode if no input detected
	unsigned key_press_cnt = 0;
	//bool process_event_mode = true;
	enum libevdev_read_flag read_flag = LIBEVDEV_READ_FLAG_NORMAL;
	/* 
		Allocate generic memory block
		Memory structure:
		{ uint64_t, struct input_event[64] }
	*/
	void* p_data = malloc(sizeof(uint64_t) + sizeof(struct input_event) * 64);
	uint64_t* p_event_count = (uint64_t*)p_data;
	struct input_event* event_queue = (struct input_event*)(p_event_count + 1);
	*p_event_count = 0;

	// Set up poll signals for grab and exit signals
	struct pollfd pfd[2];
	pfd[0].fd = libevdev_get_fd(this->dev);
	pfd[0].events = POLLIN;
	pfd[1].fd = Device::event_signal_fd;
	pfd[1].events = POLLIN;

	// Main loop
	while (Device::is_exit.load(std::memory_order_acquire) == false)
	{
		if (libevdev_has_event_pending(this->dev))
		{
			switch(libevdev_next_event(this->dev, read_flag, &event_queue[*p_event_count]))
			{
				case -EAGAIN:	// No inputs are available
					read_flag = LIBEVDEV_READ_FLAG_NORMAL;
					break;

				case LIBEVDEV_READ_STATUS_SYNC:
					read_flag = LIBEVDEV_READ_FLAG_SYNC;
					while (event_queue[*p_event_count].code == EV_SYN && event_queue[*p_event_count].value == SYN_DROPPED)
					{
						libevdev_next_event(this->dev, read_flag, &event_queue[*p_event_count]);
					}
				case LIBEVDEV_READ_STATUS_SUCCESS:
					switch(event_queue[*p_event_count].type)	// Handle events
					{
						case EV_SYN:
							if (*p_event_count && event_queue[*p_event_count].value == SYN_REPORT && this->device_is_grabbed)
							{
								Device::global_queue.push(p_data);
								write(Device::poll_signal_fd, &add_to_count, sizeof(uint64_t));	// Write to polling eventfd
								Device::pending_events.fetch_add(1, std::memory_order_acq_rel); // Notify watchdog
								Device::pending_events.notify_one();
								
								p_data = malloc(sizeof(uint64_t) + sizeof(struct input_event) * 64);	// Create new buffer
								p_event_count = (uint64_t*)p_data;
								event_queue = (struct input_event*)(p_event_count + 1);
							}
							*p_event_count = 0;	// Set event counter to zero regardless
							break;

						case EV_KEY:
							// If key is released AND there was a change in the local state
							if (event_queue[*p_event_count].value == 0 && this->local_key_state.remove(event_queue[*p_event_count].code))
							{
								Device::global_key_press_cnt.fetch_sub(1, std::memory_order_acq_rel);
								// Only register a key release when there are no keys being pressed down
								if (Device::global_key_state[event_queue[*p_event_count].code].fetch_sub(1, std::memory_order_acq_rel) == 1)
								{
									++*p_event_count;
								}
								--key_press_cnt;
							}
							else if (event_queue[*p_event_count].value == 1 && this->local_key_state.insert(event_queue[*p_event_count].code))	// If key is pressed ONLY
							{
								Device::global_key_press_cnt.fetch_add(1, std::memory_order_acq_rel);
								if (Device::global_key_state[event_queue[*p_event_count].code].fetch_add(1, std::memory_order_acq_rel) == 0)
								{
									++*p_event_count;
								}
								++key_press_cnt;
							}
							break;

						case EV_REL:
							if (event_queue[*p_event_count].value != 0)
							{
								++*p_event_count;
							}
							break;

						case EV_ABS:
							++*p_event_count;

						default:
							break;
					}
					break;

				default:
					if (*p_event_count != 0)
						for (std::size_t n = 0; n < *p_event_count; ++n)
							if (event_queue[n].code == EV_KEY && event_queue[n].value != 0)
							{
								Device::global_key_state[event_queue[n].code].fetch_sub(1, std::memory_order_acq_rel);
								Device::global_key_press_cnt.fetch_sub(1, std::memory_order_acq_rel);
								this->local_key_state.remove(event_queue[n].code);
							}
					if (this->local_key_state != BitField::zero())
						for (unsigned code = 0; code < KEY_CNT; ++code)
							if (this->local_key_state.remove(code))
							{
								Device::global_key_state[code].fetch_sub(1, std::memory_order_acq_rel);
								Device::global_key_press_cnt.fetch_sub(1, std::memory_order_acq_rel);
							}
					goto CLEAN_UP_THREAD;	// Break out of loop to deactivate device
			}
		}
		else if ((Device::is_grabbed.load(std::memory_order_acquire) != this->device_is_grabbed) && key_press_cnt == 0)	// Toggling local grab state (only grabs if no inputs are being received)
		{
			this->device_is_grabbed = !this->device_is_grabbed;
			libevdev_grab(this->dev, grab_state[this->device_is_grabbed]);
			uint64_t msg = 0;
			read(pfd[1].fd, &msg, sizeof(uint64_t));
		}
		else if (poll(pfd, 2, -1) < 0)	// Handle polling error
		{
			std::cerr << "Input polling failed: " << strerror(errno) << std::endl;
			break;
		}
	}

	CLEAN_UP_THREAD:
	free(p_data);
	if (this->device_is_grabbed)
	{
		libevdev_grab(this->dev, LIBEVDEV_UNGRAB);
		this->device_is_grabbed = false;
	}
	Device::active_devices.fetch_sub(1, std::memory_order_acq_rel);
	Device::active_devices.notify_one();
}

BitField Device::return_enabled_local_key_states() const
{
	BitField enabled_codes(KEY_CNT);
	for (unsigned code = 0; code < KEY_CNT; ++code)
	{
		if (libevdev_has_event_code(this->dev, EV_KEY, code))
		{
			enabled_codes.insert(code);
		}
	}
	return enabled_codes;
}

BitField Device::return_enabled_local_rel_states() const
{
	BitField enabled_codes(REL_CNT);
	for (unsigned code = 0; code < REL_CNT; ++code)
	{
		if (libevdev_has_event_code(this->dev, EV_REL, code))
		{
			enabled_codes.insert(code);
		}
	}
	return enabled_codes;
}

BitField Device::return_enabled_global_key_states()
{
	BitField enabled_codes(KEY_CNT);
	for (unsigned n = 0; n < Device::device_objects.size(); ++n)
	{
		if (Device::device_objects[n]->dev != nullptr)
		{
			enabled_codes |= Device::device_objects[n]->return_enabled_local_key_states();
		}
	}
	return enabled_codes;
}

BitField Device::return_enabled_global_rel_states()
{
	BitField enabled_codes(REL_CNT);
	for (unsigned n = 0; n < Device::device_objects.size(); ++n)
	{
		if (Device::device_objects[n]->dev != nullptr)
		{
			enabled_codes |= Device::device_objects[n]->return_enabled_local_rel_states();
		}
	}
	return enabled_codes;
}

void Device::wait_for_exit()
{
	while (!Device::is_exit.load(std::memory_order_acquire))
		Device::is_exit.wait(false, std::memory_order_acquire);
}