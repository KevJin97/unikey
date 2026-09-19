#ifndef DEVICE_HPP
#define DEVICE_HPP

#include "BitField.hpp"
#include "Cyclic_Queue.hpp"

#include <cstdint>
#include <string>
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

// Snapshot of an input device's static capabilities, used to generate
// HID descriptors (e.g. touchpad/touchscreen report maps) for it.
struct Device_Capabilities
{
	unsigned id = 0;			// Same value returned by Device::return_batch_source()
	std::string name;
	BitField properties{INPUT_PROP_CNT};
	BitField key_codes{KEY_CNT};
	std::vector<std::pair<unsigned, struct input_absinfo>> absinfo;
};

class Device
{
	public:
	/*
		Event batch memory layout handed to the event processor:
		{ uint64_t count, struct input_event[EVENT_BATCH_CAPACITY + 1], uint64_t source_id }

		The extra event slot is scratch space so a full batch can still read
		(and discard) the next event without overflowing. The source ID trailer
		sits after the event array so consumers that only read
		{ count, events[count] } (e.g. the WiFi client) are unaffected.
	*/
		static inline constexpr std::size_t EVENT_BATCH_CAPACITY = 128;
		static inline constexpr std::size_t EVENT_BATCH_SOURCE_OFFSET = sizeof(uint64_t) + sizeof(struct input_event) * (EVENT_BATCH_CAPACITY + 1);
		static inline constexpr std::size_t EVENT_BATCH_BYTES = EVENT_BATCH_SOURCE_OFFSET + sizeof(uint64_t);

	private:
	// STATIC PRIVATE INTERFACE
		static void default_event_processor(const void* data, uint64_t unit_size=sizeof(struct input_event));
		static void watchdog_process();
		static void hotplug_detect();
		static void accept_event(uint64_t* p_event_count);
		static void set_batch_source(void* data, unsigned id);
	
	// STATIC MEMBER DATA
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
		static inline std::atomic_uint64_t pending_reads{0};
		static inline std::atomic_bool is_grabbed{false};
		static inline std::atomic_bool is_exit{false};

	private:
	// PRIVATE MEMBER DATA
		unsigned id;
		struct libevdev* dev = nullptr;
		bool device_is_grabbed = false;
		BitField local_key_state{KEY_CNT};
		std::thread input_monitor_thread;

	// PRIVATE CONSTRUCTOR
		Device(const std::string& filepath);
	
	// PRIVATE INTERFACE
		void input_monitor_process();

	public:
	// PUBLIC CONSTRUCTOR(S)
		Device(const Device&) = delete;	// Delete copy constructor

	// DESTRUCTOR
		~Device();
	
	// STATIC PUBLIC INTERFACE
		static void set_event_processor();
		static void set_event_processor(void (*event_processing_function)(const void*, uint64_t));
		static void initialize_devices(const std::string& directory);
		static unsigned set_timeout_length(unsigned seconds);
		static bool trigger_activation();
		static void trigger_exit();
		static void wait_for_exit();
		static bool return_grab_state();
		static BitField return_enabled_global_key_states();
		static BitField return_enabled_global_rel_states();
		static std::vector<Device_Capabilities> return_device_capabilities();
		static unsigned return_batch_source(const void* data);
	
	// PUBLIC INTERFACE
		BitField return_enabled_local_key_states() const;
		BitField return_enabled_local_rel_states() const;
		BitField return_enabled_local_properties() const;
		std::vector<std::pair<unsigned, struct input_absinfo>> return_enabled_local_absinfo() const;
	
	// OPERATOR OVERLOAD
		Device& operator= (const Device&) = delete;	// Delete copy operator
};

#endif // DEVICE_HPP