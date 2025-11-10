#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <cstdint>
#include <unistd.h>
#include <vector>
#include <atomic>
#include <thread>
#include <cstring>
#include <assert.h>
#include <fcntl.h>

#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <libevdev/libevdev.h>

#include "BitField.hpp"
#include "Cyclic_Queue.hpp"

class Device
{
	static void default_event_processor(const void* data, uint64_t unit_size=sizeof(struct input_event));
	static void watchdog_process();
	static void hotplug_detect();
	
	static inline void (*event_process)(const void*, const uint64_t) = Device::default_event_processor;
	static inline std::atomic_int8_t global_key_state[KEY_CNT] = { 0 };
	static inline unsigned timeout_length = 30000;
	static inline Cyclic_Queue global_queue;
	static inline Cyclic_Queue global_mem_bank;
	static inline Cyclic_Queue global_id_queue;
	static inline std::vector<Device*> device_objects;
	static inline std::thread watchdog_thread;
	static inline int poll_signal_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);
	static inline int event_signal_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);
	static inline std::atomic_uint32_t active_devices{0};
	static inline std::atomic_uint32_t pending_events{0};
	static inline std::atomic_uint32_t global_key_press_cnt{0};
	static inline std::atomic_bool is_grabbed{false};
	static inline std::atomic_bool is_exit{false};

	private:
		unsigned id;
		struct libevdev* dev = nullptr;
		bool device_is_grabbed = false;
		BitField local_key_state{KEY_CNT};
		std::thread input_monitor_thread;

		void input_monitor_process();

	// PRIVATE CONSTRUCTOR
		Device(const std::string& filepath);
	
	public:
	// DESTRUCTOR
		~Device();
	
	// PUBLIC INTERFACE
		static void set_event_processor(void (*event_processing_function)(const void*, uint64_t));
		static void initialize_devices(const std::string& directory);
		static unsigned set_timeout_length(unsigned seconds);
		static bool trigger_activation();
		static void trigger_exit();
		static void wait_for_exit();
		static bool return_grab_state();
		static BitField return_enabled_global_key_states();
		static BitField return_enabled_global_rel_states();

		Device(const Device&) = delete;	// Delete copy constructor
		Device& operator= (const Device&) = delete;	// Delete copy operator
		BitField return_enabled_local_key_states() const;
		BitField return_enabled_local_rel_states() const;
};

#endif // DEVICE_HPP
