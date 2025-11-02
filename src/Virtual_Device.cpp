#include "Virtual_Device.hpp"

#include <iostream>
#include <string.h>

#include "libevdev/libevdev-uinput.h"
#include "libevdev/libevdev.h"

Virtual_Device::Virtual_Device()
{
	this->dev = libevdev_new();
	libevdev_enable_event_type(this->dev, EV_SYN);
	for (unsigned code = 0; code < SYN_CNT; ++code)
	{
		libevdev_enable_event_code(dev, EV_SYN, code, NULL);
	}
}

Virtual_Device::Virtual_Device(const std::string& device_name)
{
	this->dev = libevdev_new();
	libevdev_enable_event_type(this->dev, EV_SYN);
	for (unsigned code = 0; code < SYN_CNT; ++code)
	{
		libevdev_enable_event_code(this->dev, EV_SYN, code, NULL);
	}

	libevdev_set_name(this->dev, "Unikey HID Device");
}

Virtual_Device::~Virtual_Device()
{
	libevdev_uinput_destroy(this->virt_dev);
	this->virt_dev = nullptr;
	libevdev_free(this->dev);
	this->dev = nullptr;
}

void Virtual_Device::create_virt_device()
{
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
	libevdev_set_name(this->dev, "Unikey HID Device");
}

void Virtual_Device::enable_codes(const unsigned type, const BitField& enabled_key_field)
{
	const unsigned MAX = libevdev_event_type_get_max(type);
	libevdev_enable_event_type(this->dev, type);

	for (unsigned code = 0; code <= MAX; ++code)
	{
		if (enabled_key_field.contains(code))
		{
			libevdev_enable_event_code(this->dev, type, code, NULL);
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
	this->create_virt_device();
	libevdev_uinput_write_event(this->virt_dev, ev.type, ev.code, ev.value);
	libevdev_uinput_write_event(this->virt_dev, EV_SYN, SYN_REPORT, 0);
}

void Virtual_Device::write_event(const struct input_event* ev_list, const std::size_t list_size)
{
	this->create_virt_device();
	for (std::size_t n = 0; n < list_size; ++n)
	{
		libevdev_uinput_write_event(this->virt_dev, ev_list[n].type, ev_list[n].code, ev_list[n].value);
	}
}

void Virtual_Device::write_event(unsigned type, unsigned code, int value)
{
	this->create_virt_device();
	libevdev_uinput_write_event(this->virt_dev, type, code, value);
}