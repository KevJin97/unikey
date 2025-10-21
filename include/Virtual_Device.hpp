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
		struct libevdev* dev = nullptr;
		struct libevdev_uinput* virt_dev = nullptr;

		void create_virt_device();
	
	public:
		Virtual_Device();
		Virtual_Device(const std::string& device_name);

		~Virtual_Device();

		void set_device_name(const std::string& device_name);
		void enable_codes(unsigned type, const BitField& enabled_key_field);
		void enable_codes(unsigned type, const std::vector<uint64_t>& bitfield);
		void write_event(struct input_event& ev);
		void write_event(struct input_event* ev_list, std::size_t list_size);
		void write_event(unsigned type, unsigned code, int value);
};

#endif	// VIRTUAL_DEVICE_HPP