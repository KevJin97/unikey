#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <chrono>
#include <cstdint>
#include <linux/input-event-codes.h>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <thread>
#include <cstring>
#include <assert.h>
#include <fcntl.h>

#include <linux/input.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <libevdev/libevdev.h>

#include "BitField.hpp"
#include "Shared_Cyclic_Queue.hpp"

// Currently supporting event types for signals only
const BitField default_activation_key({ KEY_LEFTMETA, KEY_LEFTSHIFT, KEY_SPACE });

unsigned event_count(unsigned event_type);

class Device
{
	static inline void (*p_event_processor)(const struct input_event&) = NULL;
	static inline BitField activation_cmd = default_activation_key;	// This command initiates the communication
	static inline std::chrono::duration<int64_t> period = std::chrono::seconds(30);

	// SHARED DATA
	static inline Shared_Cyclic_Queue<input_event> queue;
	static inline std::atomic_uint32_t pending_ev_num{0};
	static inline std::atomic_uint32_t available_devices{0};
	static inline std::atomic_bool is_grabbed{false};
	static inline std::atomic_bool is_monitoring{false};

	static inline std::condition_variable cv;
	static inline std::mutex global_lock;

	static inline std::thread grab_state_timeout;
	static void watchdog();

	private:
		unsigned event_num;
		std::mutex wait_lock;
		struct libevdev* dev;
		std::vector<BitField> enabled_events;
		BitField* p_key_state;
		BitField* p_cursor_state;
		std::thread data_handler;
		std::atomic_bool device_status;	// Enabled or disabled

	// PRIVATE INTERFACE
		void monitor_data();
		void synchronize(struct input_event& ev);

	public:
	// CONSTRUCTOR(S)
		Device(unsigned event_num);

	// DESTRUCTOR
		~Device();

	// PUBLIC INTERFACE
		static void set_event_processor(void (**func_ptr)(const struct input_event&));
		static void set_timeout_length(std::chrono::seconds secs);
		static void set_activation_cmd();
		
		static void begin_monitoring();
		static void stop_monitoring();
		static bool trigger_activation();	// Manually trigger
		
		BitField return_enabled_event_codes(unsigned type);
		void process_event(const struct input_event& ev);
		void wait(std::atomic_bool& sig);
		void disable_device();
		void enable_device();
};


// IMPLEMENTATION

inline void Device::watchdog()
{
	// When Device::available_devices hits 0, wake the watchdog loop so that it can break the loop
	std::thread cleanup(
		[]
		{ 
			unsigned old;
			while ((old = Device::available_devices.load(std::memory_order_acquire)) != 0)
			{
				Device::available_devices.wait(old, std::memory_order_acquire);
			}
			Device::is_grabbed.store(true, std::memory_order_release);
			Device::is_grabbed.notify_all();
		}
	);
	cleanup.detach();

	// Watchdog Loop
	while (Device::available_devices.load(std::memory_order_acquire) != 0)
	{
		if (Device::is_grabbed.load(std::memory_order_acquire))
		{
			if (Device::pending_ev_num.load(std::memory_order_acquire) == 0)
			{
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
					Device::trigger_activation();
				}
			}
			else
			{
				while (Device::pending_ev_num.load(std::memory_order_acquire) != 0)
				{
					Device::p_event_processor(Device::queue.pop());
					Device::pending_ev_num.fetch_sub(1, std::memory_order_acq_rel);
				}
			}
		}
		else
		{
			Device::is_grabbed.wait(false, std::memory_order_acquire);
		}
	}
}

#endif // DEVICE_HPP
