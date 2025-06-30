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
inline const BitField default_activation_key({ KEY_LEFTMETA, KEY_LEFTSHIFT, KEY_SPACE });
inline const BitField no_input;

enum Device_Type
{
	UNKOWN,
	KEYBOARD,
	MOUSE,
	TOUCHPAD
};

class Device
{
	static inline void (*p_event_processor)(const struct input_event&) = NULL;
	static inline BitField activation_cmd = default_activation_key;	// This command initiates the communication
	static inline std::chrono::duration<int64_t> period = std::chrono::seconds(30);

	// SHARED DATA
	static inline Shared_Cyclic_Queue<input_event> queue;
	static inline BitField shared_key_state;
	static inline struct rel_data shared_rel_state;
	static inline struct abs_data shared_abs_state;
	static inline std::atomic_uint32_t pending_ev_num{0};
	static inline std::atomic_uint32_t available_devices{0};
	static inline std::atomic_bool is_grabbed{false};
	static inline std::atomic_bool is_monitoring{false};

	static inline std::condition_variable cv;
	static inline std::mutex global_lock;

	static inline std::thread grab_state_timeout;
	static void watchdog();

	private:
	// PRIVATE MEMBERS
		unsigned event_num;
		struct libevdev* dev;
		BitField enabled_events[EV_CNT];
		void* p_state;
		Device_Type device_category;
		std::thread data_handler;
		std::atomic_bool device_status;	// Enabled or disabled

		void keyboard_monitor();
		void mouse_monitor();
		void touchpad_monitor();

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
		static bool trigger_activation();	// Global Trigger
		
		BitField return_enabled_event_codes(unsigned type);
		void process_event(Device_Type dev_type);
		void disable_device();
		void enable_device();
};

#endif // DEVICE_HPP
