#include "Device.hpp"
#include "BitField.hpp"
#include "Shared_Cyclic_Queue.hpp"

#include "libevdev/libevdev.h"

#include <atomic>
#include <cerrno>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <mutex>
#include <string>
#include <functional>

#define EVER ;;

Device::Device(unsigned event_num)
{
	this->event_num = event_num;
	this->enabled_events = std::vector<BitField>(EV_CNT);
	this->p_key_state = NULL;
	this->p_cursor_state = NULL;
	this->dev = NULL;

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
		for (unsigned type = 0; type < EV_CNT; ++type)
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
		
		if (libevdev_has_event_type(this->dev, EV_KEY))
		{
			this->p_key_state = new BitField;
		}
		if (libevdev_has_event_type(this->dev, EV_REL))
		{
			this->p_cursor_state = new BitField;
		}
		
		this->enable_device();
	}

	Device::available_devices.fetch_add(1, std::memory_order_acq_rel);

	if (Device::available_devices.load(std::memory_order_acquire) == 1)	// If watchdog loop doesn't exist, initialize it
	{
		Device::grab_state_timeout = std::thread(&Device::watchdog);
	}

	Device::available_devices.notify_one();
}

Device::~Device()
{
	this->disable_device();
	Device::available_devices.fetch_sub(1, std::memory_order_acq_rel);
	Device::available_devices.notify_one();

	if (this->p_key_state != NULL)
	{
		delete this->p_key_state;
	}
	if (this->p_cursor_state != NULL)
	{
		delete this->p_cursor_state;
	}
	if (this->dev != NULL)
	{
		close(libevdev_get_fd(this->dev));
		libevdev_free(this->dev);
	}
	if (Device::available_devices.load(std::memory_order_acquire) == 0)
	{
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

void Device::process_event(const struct input_event& ev)
{
	//Device::queue.push(ev);
	Device::pending_ev_num.fetch_add(1,std::memory_order_acq_rel);
	 Device::pending_ev_num.notify_one();
}

void Device::wait(std::atomic_bool& sig)
{
	std::unique_lock<std::mutex> lock(this->wait_lock);
	Device::cv.wait(lock, [&sig, this]{ return sig.load(std::memory_order_acquire) || (this->device_status.load(std::memory_order_acquire) == false); });
}

void Device::enable_device()
{
	this->device_status.store(true, std::memory_order_release);
	this->data_handler = std::thread(std::bind(&Device::monitor_data, this));
}

void Device::disable_device()
{
	bool prev_status = this->device_status.load(std::memory_order_acquire);
	this->device_status.store(false, std::memory_order_release);
	if (prev_status == true)
	{
		Device::cv.notify_all();
		this->data_handler.join();
	}
}

void Device::monitor_data()
{
	const unsigned flags[2] = { LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING, LIBEVDEV_READ_FLAG_SYNC };
	bool triggered = false;

	while (this->device_status.load(std::memory_order_acquire))
	{
		if (Device::is_monitoring.load(std::memory_order_acquire))	// If monitoring is enabled
		{
			if (Device::is_grabbed.load(std::memory_order_acquire))	// If inputs are grabbed
			{
				libevdev_grab(this->dev, LIBEVDEV_GRAB);
				while (Device::is_grabbed.load(std::memory_order_acquire))
				{
					struct input_event ev;
					switch(libevdev_next_event(this->dev, flags[0], &ev))
					{
						case LIBEVDEV_READ_STATUS_SUCCESS:
							switch(ev.type)
							{
								case EV_KEY:
								(ev.value == 0) ? this->p_key_state->remove(ev.code) : this->p_key_state->insert(ev.code);
								
								if (*this->p_key_state == Device::activation_cmd)	// Trigger activation
								{
									Device::trigger_activation();
								}
								case EV_REL:
								case EV_SYN:	// Do the same thing for REL and SYN events
									this->process_event(ev);
								
								default:
									break;
							}
							break;

						case LIBEVDEV_READ_STATUS_SYNC:
							this->synchronize(ev);
							this->p_key_state->clear();
							break;

						case -EAGAIN:
							break;

						default:
							this->device_status.store(false, std::memory_order_release);
							return;
					}
				}
				libevdev_grab(this->dev, LIBEVDEV_UNGRAB);
			}
			else if (this->p_key_state != NULL)	// If a keyboard input is present
			{
				struct input_event ev;
				switch(libevdev_next_event(this->dev, flags[0], &ev))
				{
					case LIBEVDEV_READ_STATUS_SUCCESS:
						if (ev.type == EV_KEY)
						{
							// Keep track of keyboard inputs and trigger activation if combination is entered
							(ev.value == 0) ? this->p_key_state->remove(ev.code) : this->p_key_state->insert(ev.code);
							
							if (*this->p_key_state == Device::activation_cmd)	// Trigger activation
							{
								this->p_key_state->clear();
								Device::trigger_activation();
							}
						}
						break;

					case LIBEVDEV_READ_STATUS_SYNC:
						this->synchronize(ev);
						this->p_key_state->clear();
						break;

					case -EAGAIN:
						break;

					default:
						this->device_status.store(false, std::memory_order_release);
						return;
				}
			}
			else
			{
				// When this->device_status is false or is_grabbed is true, notifying Device::cv will satisfy predicate
				this->wait(Device::is_grabbed);
			}
		}
		else
		{
			// When this->device_status is false or is_monitoring is true, notifying Device::cv will satisfy predicate
			this->wait(Device::is_monitoring);
		}
	}
}

void Device::synchronize(struct input_event& ev)
{
	while (libevdev_next_event(this->dev, LIBEVDEV_READ_FLAG_SYNC, &ev) == LIBEVDEV_READ_STATUS_SYNC);
	
	if (p_key_state != NULL)
	{
		this->p_key_state->clear();
	}
}

#undef EVER