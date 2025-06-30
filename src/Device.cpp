#include "Device.hpp"
#include "BitField.hpp"
//#include "Shared_Cyclic_Queue.hpp"
#include <iostream>
#include "libevdev/libevdev.h"

#include <atomic>
#include <cerrno>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <mutex>
#include <string>
#include <functional>

Device::Device(unsigned event_num)
{
	this->event_num = event_num;
	this->dev = NULL;
	this->p_state = NULL;

	int fd = open(("/dev/input/event" + std::to_string(event_num)).c_str(), O_RDONLY);
	if (libevdev_new_from_fd(fd, &this->dev) < 0)
	{
		// Error while creating device
		this->device_status.store(false, std::memory_order_release);
		libevdev_free(this->dev);
		this->dev = NULL;
	}
	else
	{
		for (unsigned code = 0, type = EV_SYN; code < SYN_CNT; ++code)
		{
			if (libevdev_has_event_code(this->dev, type, code))
			{
				this->enabled_events[type].insert(code);
			}
		}
		for (unsigned type = 1; type < EV_CNT; ++type)	// type == 0 is EV_SYN. No max is defined for this in libevdev
		{
			if (libevdev_has_event_type(this->dev, type))
			{
				for (unsigned code = 0; code <= libevdev_event_type_get_max(type); ++code)
				{
					if (libevdev_has_event_code(this->dev, type, code))
					{
						this->enabled_events[type].insert(code);
					}
				}
			}
		}

		if (libevdev_has_event_type(this->dev, EV_KEY)) // To keep track of EV_KEY inputs	
		{
			this->p_state = (void*)new BitField;
			this->device_category = KEYBOARD;
			this->enable_device();
		}
		else
		{
			this->device_category = UNKOWN;
			this->disable_device();
		}
	}

	if (Device::available_devices.load(std::memory_order_acquire) == 0)	// If watchdog loop doesn't exist, initialize it
	{
		Device::available_devices.fetch_add(1, std::memory_order_acq_rel);
		Device::grab_state_timeout = std::thread(&Device::watchdog);
	}
}

Device::~Device()
{
	this->disable_device();
	Device::available_devices.fetch_sub(1, std::memory_order_acq_rel);
	Device::available_devices.notify_one();

	if (this->p_state != NULL)
	{
		switch(this->device_category)
		{
			case KEYBOARD:
				delete (BitField*)this->p_state;
				break;

			default:
				break;
		}
	}

	if (this->dev != NULL)
	{
		close(libevdev_get_fd(this->dev));
		libevdev_free(this->dev);
	}

	if (Device::available_devices.load(std::memory_order_acquire) == 0)	// Disable watchdog if last device is deconstructed
	{
		if (Device::is_grabbed.load(std::memory_order_acquire) == true)
		{
			Device::trigger_activation();
		}
		else
		{
			Device::is_grabbed.store(true, std::memory_order_release);
			Device::is_grabbed.notify_all();
		}

		Device::grab_state_timeout.join();
	}
}

void Device::set_event_processor(void (**func_ptr)(const struct input_event &))
{
	Device::p_event_processor = *func_ptr;
}

void Device::set_timeout_length(std::chrono::seconds secs)
{
	Device::period = secs;
}

void Device::set_activation_cmd()
{

}

void Device::begin_monitoring()
{
	Device::is_monitoring.store(true, std::memory_order_release);
	Device::is_monitoring.notify_all();
}

void Device::stop_monitoring()
{
	Device::is_monitoring.store(false, std::memory_order_seq_cst);
}

bool Device::trigger_activation()
{
	std::cout << "Triggered" << std::endl;
	if (Device::is_monitoring.load(std::memory_order_acquire) == false)
	{
		Device::begin_monitoring();
	}
	Device::is_grabbed.store(!Device::is_grabbed.load(std::memory_order_acquire), std::memory_order_release);
	Device::is_grabbed.notify_all();
	Device::cv.notify_all();

	return Device::is_grabbed.load(std::memory_order_seq_cst);
}

BitField Device::return_enabled_event_codes(unsigned int type)
{
	return this->enabled_events[type];
}

void Device::process_event(Device_Type dev_type)
{
	//Device::queue.push();
	switch(this->device_category)
	{
		case KEYBOARD:
			Device::shared_key_state |= *(BitField*)this->p_state;
			break;

		default:
			break;
	}
	Device::pending_ev_num.fetch_add(1,std::memory_order_acq_rel);
	Device::pending_ev_num.notify_all();
}

void Device::enable_device()
{
	this->device_status.store(true, std::memory_order_release);
	switch(this->device_category)
	{
		case KEYBOARD:
			this->data_handler = std::thread(std::bind(&Device::keyboard_monitor, this));
			break;

		default:
			break;
	}
}

void Device::disable_device()
{
	bool prev_status = this->device_status.load(std::memory_order_acquire);
	this->device_status.store(false, std::memory_order_release);
	if (prev_status == true)
	{
		this->data_handler.join();
	}
}

void Device::keyboard_monitor()
{
	const libevdev_read_flag flags[2] = { LIBEVDEV_READ_FLAG_NORMAL, LIBEVDEV_READ_FLAG_SYNC };
	BitField* state = (BitField*)this->p_state;
	bool action_pending = false;
	bool old_state = false;

	while (this->device_status.load(std::memory_order_acquire))
	{
		if (old_state != Device::is_grabbed.load(std::memory_order_acquire))	// If Device::trigger_activation() was called, then begin grab attempts
		{
			old_state = Device::is_grabbed.load(std::memory_order_acquire);
			action_pending = true;
		}

		struct input_event ev;
		switch (libevdev_next_event(this->dev, flags[0], &ev))	// Read device state
		{
			case LIBEVDEV_READ_STATUS_SUCCESS:	// Successful read of device state
				switch(ev.type)
				{
					case EV_SYN:	// If input source has been successfully grabbed, begin processing events
						if ((action_pending == false) && Device::is_grabbed.load(std::memory_order_acquire) && (ev.value == SYN_REPORT))
						{
							this->process_event(KEYBOARD);
						}
						break;

					case EV_KEY:	// Keep track of which keys have been pressed
						(ev.value == 0) ? state->remove(ev.code) : state->insert(ev.code);
						(ev.value == 0) ? std::cout << "Released: " << libevdev_event_code_get_name(ev.type, ev.code) << std::endl : std::cout << "Pressed: " << libevdev_event_code_get_name(ev.type, ev.code) << std::endl;
						break;

					default:
						break;
				}
				break;

			case LIBEVDEV_READ_STATUS_SYNC:	// If out-of-sync has been detected
				while (libevdev_next_event(this->dev, flags[1], &ev) == LIBEVDEV_READ_STATUS_SYNC)
				{
					switch(ev.type)
					{
						case EV_SYN:
							if ((action_pending == false) && Device::is_grabbed.load(std::memory_order_acquire) && (ev.value == SYN_REPORT))
							{
								this->process_event(KEYBOARD);
							}
							break;

						case EV_KEY:
							(ev.value == 0) ? state->remove(ev.code) : state->insert(ev.code);
							break;

						default:
							break;
					}
				}
				break;

			case -EAGAIN:
				break;

			default:	// This should never happen. End loop if it does.
				this->device_status.store(false, std::memory_order_release);
				break;
		}

		if (*state == Device::activation_cmd)	// Attempt to grab device handle
		{
			Device::trigger_activation();	// Toggle Device::is_grabbed and notify all those waiting on this
		}
		else if (action_pending && (Device::is_grabbed.load(std::memory_order_acquire) == true) && (libevdev_has_event_pending(this->dev) == 0))
		{
			// Only grab keyboard if no inputs are pressed.
			if (*state == no_input)
			{
				libevdev_grab(this->dev, LIBEVDEV_GRAB);
				action_pending = false;
				std::cout << "Grab Successful" << std::endl;
			}
		}
		else if (action_pending && (Device::is_grabbed.load(std::memory_order_acquire) == false))
		{
			// Ungrabbing can be performed without restriction
			libevdev_grab(this->dev, LIBEVDEV_UNGRAB);
			action_pending = false;
			std::cout << "Ungrabbed Successfully" << std::endl;
		}
	}
}

void Device::watchdog()
{
	std::cout << "Watchdog started successfully" << std::endl;
	// Watchdog Loop
	while (Device::available_devices.load(std::memory_order_acquire) != 0)
	{
		if (Device::is_grabbed.load(std::memory_order_acquire))
		{
			if (Device::pending_ev_num.load(std::memory_order_acquire) == 0)
			{
				std::cout << "Watchdog: Timer started" << std::endl;
				{
					std::unique_lock<std::mutex> lock(Device::global_lock);
					Device::cv.wait_for(lock, Device::period,
					[]
					{
						return (Device::is_grabbed.load(std::memory_order_acquire) == false) | (Device::pending_ev_num.load(std::memory_order_acquire) != 0);
					});
				}
				if (Device::pending_ev_num.load(std::memory_order_acquire) == 0 && Device::is_grabbed.load(std::memory_order_acquire) == true)
				{
					std::cout << "Watchdog: Releasing inputs" << std::endl;
					Device::trigger_activation();
				}
			}
			else
			{
				std::cout << "Watchdog: Processing inputs" << std::endl;
				while (Device::pending_ev_num.load(std::memory_order_acquire) != 0)
				{
					//Device::p_event_processor(Device::queue.pop());
					Device::pending_ev_num.fetch_sub(1, std::memory_order_acq_rel);
				}
			}
		}
		else
		{
			std::cout << "Watchdog: Waiting..." << std::endl;
			Device::is_grabbed.wait(false, std::memory_order_acquire);
			std::cout << "Watchdog: Triggered" << std::endl;
		}
	}
}
