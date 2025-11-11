#include "Virtual_Device.hpp"

#include <iostream>
#include <linux/input.h>
#include <string.h>

#include "libevdev/libevdev-uinput.h"
#include "libevdev/libevdev.h"

static bool check_if_power_button(unsigned type, unsigned code)
{
	if (type == EV_KEY && code == KEY_POWER)
		return true;
	else
		return false;
}

static bool check_if_power_button(const struct input_event& ev)
{
	return check_if_power_button(ev.type, ev.code);
}

Virtual_Device::Virtual_Device()
{
	this->init_virt_libevdev();
}

Virtual_Device::Virtual_Device(const std::string& device_name)
{
	this->device_name = device_name;
	this->init_virt_libevdev();
}

Virtual_Device::~Virtual_Device()
{
	libevdev_uinput_destroy(this->virt_dev);
	this->virt_dev = nullptr;
	libevdev_free(this->dev);
	this->dev = nullptr;
}

bool Virtual_Device::init_virt_libevdev()
{
	if (this->dev == nullptr)
	{
		this->dev = libevdev_new();
		libevdev_enable_property(this->dev, INPUT_PROP_POINTER);
		libevdev_enable_event_type(this->dev, EV_SYN);
		libevdev_enable_event_type(this->dev, EV_REL);
		for (unsigned code = 0; code < SYN_CNT; ++code)
		{
			libevdev_enable_event_code(this->dev, EV_SYN, code, NULL);
		}
		for (unsigned code = 0; code < REL_CNT; ++ code)
		{
			libevdev_enable_event_code(this->dev, EV_REL, code, NULL);
		}
		if (this->device_name.size() != 0)
			libevdev_set_name(this->dev, this->device_name.c_str());
		
		return true;
	}
	return false;
}

void Virtual_Device::create_virt_device()
{
	this->init_virt_libevdev();

	if (this->virt_dev == nullptr)
	{
		if (libevdev_uinput_create_from_device(this->dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &this->virt_dev) != 0)
		{
			std::cerr << "Creating virtual device failed: " << strerror(errno) << std::endl;
		}
	}
}

void Virtual_Device::set_device_name(const std::string& device_name)
{
	this->device_name = device_name;
	if (this->init_virt_libevdev() == false)
		libevdev_set_name(this->dev, this->device_name.c_str());
}

void Virtual_Device::enable_codes(const unsigned type, const BitField& enabled_key_field)
{
	const unsigned MAX = libevdev_event_type_get_max(type);
	
	this->init_virt_libevdev();
	libevdev_enable_event_type(this->dev, type);

	for (unsigned code = 0; code <= MAX; ++code)
	{
		if (enabled_key_field.contains(code))
		{
			libevdev_enable_event_code(this->dev, type, code, NULL);
			std::cout << "Enabled " << libevdev_event_value_get_name(type, code, 0) << std::endl;
		}
	}
}

void Virtual_Device::enable_codes(const unsigned type, const std::vector<uint64_t>& bitfield)
{
	BitField codes;
	codes.copy_bit_vector(bitfield);
	this->enable_codes(type, codes);
}

void Virtual_Device::write_event(const struct input_event& ev)
{
	if (check_if_power_button(ev))
		return;

	this->create_virt_device();
	libevdev_uinput_write_event(this->virt_dev, ev.type, ev.code, ev.value);
	libevdev_uinput_write_event(this->virt_dev, EV_SYN, SYN_REPORT, 0);
}

void Virtual_Device::write_event(const struct input_event* ev_list, const uint64_t& list_size)
{
	this->create_virt_device();
	for (uint64_t n = 0; n < list_size; ++n)
	{
		if (check_if_power_button(ev_list[n]) == false)
			libevdev_uinput_write_event(this->virt_dev, ev_list[n].type, ev_list[n].code, ev_list[n].value);
	}
}

void Virtual_Device::write_event(unsigned type, unsigned code, int value)
{
	if (check_if_power_button(type, code))
		return;

	this->create_virt_device();
	libevdev_uinput_write_event(this->virt_dev, type, code, value);
}

void Virtual_Device::clear()
{
	if (this->virt_dev != nullptr)
	{
		libevdev_uinput_destroy(this->virt_dev);
		this->virt_dev = nullptr;
	}
	if (this->dev != nullptr)
	{
		libevdev_free(this->dev);
		this->dev = nullptr;
	}
}