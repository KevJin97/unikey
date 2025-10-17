#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <vector>
#include <atomic>
#include <thread>
#include <cstring>
#include <assert.h>
#include <fcntl.h>

#include <iostream>

#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <sys/eventfd.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <libevdev/libevdev.h>

#include "BitField.hpp"
#include "Cyclic_Queue.hpp"

// TODO: implement shared memory to modify is_grabbed and is_exit values externally

inline void print_event(struct input_event* ev, uint64_t length)
{
	for (uint64_t n = 0; n < length; ++n)
	{
		std::cout << libevdev_event_code_get_name(ev[n].type, ev[n].code) << ',' << ev[n].value << ((n % 4 == 3) ? "\n" : "\t\t");
	}
	std::cout << std::endl;
}

class Device
{
	static inline void (*event_process)(struct input_event*, uint64_t) = print_event;
	static inline Cyclic_Queue global_queue;
	static inline std::atomic_int8_t global_key_state[KEY_CNT] = { 0 };
	static inline std::vector<Device*> device_objects;
	static inline std::thread watchdog_thread;
	static inline unsigned timeout_length = 30000;
	static inline int poll_signal_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);
	static inline int event_signal_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	static inline std::atomic_uint32_t active_devices{0};
	static inline std::atomic_uint32_t pending_events{0};
	static inline std::atomic_bool is_grabbed{false};
	static inline std::atomic_bool is_exit{false};

	static void watchdog_process();

	private:
		struct libevdev* dev = nullptr;
		bool device_is_grabbed = false;
		BitField local_key_state{KEY_CNT};
		std::thread input_monitor_thread;
		unsigned total_pressed = 0;

		void input_monitor_process();

	// PRIVATE CONSTRUCTOR
		Device(const std::string& filepath);
	
	public:
	// DESTRUCTOR
		~Device();
	
	// PUBLIC INTERFACE
		static void set_event_processor(void (*event_processing_function)(struct input_event*, uint64_t));
		static void initialize_devices(const std::string& directory);
		static void set_timeout_length(unsigned seconds);
		static void trigger_activation();
		static void trigger_exit();

		Device(const Device&) = delete;	// Delete copy constructor
		Device& operator= (const Device&) = delete;	// Delete copy operator
};

#endif // DEVICE_HPP
