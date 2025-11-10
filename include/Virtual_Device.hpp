#ifndef VIRTUAL_DEVICE_HPP
#define VIRTUAL_DEVICE_HPP

#include <string>

#include "libevdev/libevdev.h"
#include "libevdev/libevdev-uinput.h"
#include "BitField.hpp"

#include <linux/input.h>

class Virtual_Device
{
	private:
		std::string device_name;
		struct libevdev* dev = nullptr;
		struct libevdev_uinput* virt_dev = nullptr;

		bool init_virt_libevdev();
		void create_virt_device();
	
	public:
		Virtual_Device();
		Virtual_Device(const std::string& device_name);

		~Virtual_Device();

		void set_device_name(const std::string& device_name);
		void enable_codes(const unsigned type, const BitField& enabled_key_field);
		void enable_codes(const unsigned type, const std::vector<uint64_t>& bitfield);
		void write_event(const struct input_event& ev);
		void write_event(const struct input_event* ev_list, const uint64_t& list_size);
		void write_event(unsigned type=EV_SYN, unsigned code=SYN_REPORT, int value=0);
		void clear();
};

#endif	// VIRTUAL_DEVICE_HPP