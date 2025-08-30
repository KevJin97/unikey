#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <vector>
#include <atomic>
#include <thread>
#include <cstring>
#include <assert.h>
#include <fcntl.h>

#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <sys/eventfd.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <libevdev/libevdev.h>

#include "BitField.hpp"
#include "Shared_Cyclic_Queue.hpp"

class Device
{
	protected:
		static inline void (*p_event_processor)(const struct input_event&) = nullptr;
		static inline BitField activation_cmd{{ KEY_LEFTMETA, KEY_LEFTSHIFT, KEY_SPACE }};	// This command initiates the communication
		static inline unsigned period = 30;

	// SHARED DATA
		static inline int poll_signal_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
		static inline std::atomic_uint32_t active_devices{0};
		static inline Shared_Cyclic_Queue<input_event> queue;
		static inline BitField shared_key_state{KEY_CNT};
		static struct rel_data shared_rel_state;
		static struct abs_data shared_abs_state;
		static inline std::atomic_uint32_t pending_events_to_process{0};
		static inline std::atomic_bool is_grabbed{true};
		static inline std::atomic_bool is_record{false};
		static inline std::atomic_bool is_exit{false};

		static inline std::thread watchdog_thread;
		static void watchdog_process();

	protected:
	// PROTECTED MEMBERS
		struct libevdev* dev;
		bool is_active;
		std::thread input_monitor_process;

		virtual void input_monitor() = 0;

	public:
	// CONSTRUCTOR(S)
		Device();

	// DESTRUCTOR
		virtual ~Device() = 0;

	// PUBLIC INTERFACE
		static Device* create_device(const std::string& event_path);
		static void set_event_processor(void (**func_ptr)(const struct input_event&));
		static void init_watchdog();
		static void set_timeout_length(unsigned secs);
		static void set_activation_cmd();
		static void trigger_activation();	// Global Trigger
		static void wait_to_exit();
		static void exit();
		
		BitField return_enabled_event_codes(unsigned type);
		void toggle_device();
};

#endif // DEVICE_HPP
