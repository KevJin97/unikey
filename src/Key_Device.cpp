#include "Key_Device.hpp"
#include "Device.hpp"
#include "libevdev/libevdev.h"
#include <climits>
#include <iostream>
#include <atomic>
#include <cstdint>
#include <functional>

#include <linux/input.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <sys/poll.h>

Key_Device::Key_Device(struct libevdev* dev)
{
	this->dev = dev;
	this->is_active = false;
	this->input_monitor_process = std::thread(std::bind(&Key_Device::input_monitor, this));

	if (Device::active_devices.fetch_add(1, std::memory_order_acq_rel) == 0)
	{
		Device::active_devices.notify_one();
	}
}

Key_Device::~Key_Device()
{
	close(libevdev_get_fd(this->dev));
	libevdev_free(this->dev);
	Device::active_devices.fetch_sub(1, std::memory_order_acq_rel);

	this->input_monitor_process.join();
}

void Key_Device::input_monitor()
{
	constexpr enum libevdev_read_flag read_flag[2] = { LIBEVDEV_READ_FLAG_NORMAL, LIBEVDEV_READ_FLAG_SYNC };
	uint8_t timeout_counter = 0;
	
	// Polling monitor for HID events
	struct pollfd pfd;
	pfd.fd = libevdev_get_fd(this->dev);
	pfd.events = POLLIN;

	/*
		The event loop will check constantly for events until the timeout_counter reaches its maximum value.
		Then it jumps into a polling state to wait for an event or an interrupt. When either one occurs, 
		it checks Device::is_grabbed status before looping around to check if Device::is_exit has occurred.
	*/

	// EVENT LOOP
	while (Device::is_exit.load(std::memory_order_acquire) == false)
	{
		struct input_event ev;
		if (timeout_counter != UINT8_MAX)	// If no inputs are detected, it will stop busy-waiting and begin polling
		{
			switch(libevdev_next_event(this->dev, read_flag[0], &ev))
			{
				case LIBEVDEV_READ_STATUS_SUCCESS:	// Update state
					timeout_counter = 0;
					switch(ev.type)
					{
						case EV_SYN:
							break;

						case EV_KEY:
							(ev.value == 0) ? 
								this->current_state.remove(ev.code) : this->current_state.insert(ev.code);
							break;

						default:
							break;
					}
					break;

				case LIBEVDEV_READ_STATUS_SYNC:	// Synchronize the stream
					while (libevdev_next_event(this->dev, read_flag[1], &ev) == LIBEVDEV_READ_STATUS_SYNC);
					this->current_state.wipe();
					timeout_counter = 0;
					break;

				case -EAGAIN:	// If no inputs, increment counter
					++timeout_counter;
					break;

				default:
					return;	// Break out of loop completely and disable device
			}

			if (this->is_active == true)	// Once it's active, begin sending data into the queue
			{
				Device::queue.push(ev);
				Device::pending_events_to_process.fetch_add(1, std::memory_order_acq_rel);
				Device::pending_events_to_process.notify_one();

				uint64_t signal = 1;
				write(Device::poll_signal_fd, &signal, sizeof(uint64_t));
			}
		}
		else	// timeout_counter == UINT8_MAX
		{
			if (poll(&pfd, 1, -1) < 0)	// Have input wait for HID event or an interrupt signal indefinitely
			{
				std::cerr << "Poll failed: " << strerror(errno) << std::endl;	// Error while polling
				break;
			}

			if ((pfd.revents & POLLIN) && libevdev_has_event_pending(this->dev))	// If polling detects an event
			{	
				++timeout_counter;	// Overflow timeout_counter to zero
				continue;	// Restart the loop to grab the event first
			}
			else
			{
				return;
			}
		}

		if (Device::is_grabbed.load(std::memory_order_acquire) == true)	// If the grab state has been triggered
		{
			if (this->is_active == false && this->current_state == BitField::zero())	// Grab inputs only if there's no inputs and set system to active
			{
				libevdev_grab(this->dev, LIBEVDEV_GRAB);
				this->is_active = true;
			}
		}
		else
		{
			if (this->is_active)	// Ungrab inputs
			{
				libevdev_grab(this->dev, LIBEVDEV_UNGRAB);
				this->is_active = false;
				Device::pending_events_to_process.notify_one();
			}
			if (this->current_state == Device::activation_cmd)
			{
				Device::is_grabbed.store(true, std::memory_order_release);
				Device::is_grabbed.notify_one();
			}
		}
	}
}