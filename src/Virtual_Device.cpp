#include "BitField.hpp"
#include "Device.hpp"
#include "libevdev/libevdev.h"
#include <fcntl.h>
#include <libevdev/libevdev-uinput.h>
#include <Virtual_Device.hpp>
#include <linux/input.h>


Virtual_Device::Virtual_Device(std::string filepath)
{
	struct libevdev* device = NULL;
	int fd = open(filepath.c_str(), O_RDONLY | O_NONBLOCK);
	if (libevdev_new_from_fd(fd, &device) != 0)
	{
		// Error handling
	}
	libevdev_uinput_create_from_device(device, fd, &this->virt_dev);
	
	// Clean-up
	libevdev_free(device);
	close(fd);
}

Virtual_Device::Virtual_Device(std::vector<BitField>& enabled_codes)
{
	this->init_device(enabled_codes);
}

Virtual_Device::~Virtual_Device()
{
	libevdev_uinput_destroy(this->virt_dev);
}

void Virtual_Device::set_name(std::string new_name)
{
	Virtual_Device::virt_dev_name = new_name;
}

void Virtual_Device::init_device(std::vector<BitField>& enabled_codes)
{
	if (this->virt_dev != NULL)
	{
		libevdev_uinput_destroy(this->virt_dev);
	}

	struct libevdev* dev = libevdev_new();
	libevdev_set_name(dev, Virtual_Device::virt_dev_name.c_str());
	void* p_data;

	for (std::size_t type = 0; type < EV_CNT; ++type)
	{
		for (std::size_t code = 0; code < event_count(type); ++code)
		{
			if (enabled_codes[type].contains(code))
			{
				switch (type)
				{
					case EV_ABS:
						//p_data; // = (void*)input_absinfo_value;
						break;
					case EV_REP:
						//p_data; // = (void*)integer_value;
						break;

					default:
						p_data = NULL;
				}
				libevdev_enable_event_code(dev, type, code, p_data);
			}
		}
	}
	if (libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &this->virt_dev) != 0)
	{
		// Error Handling
	}

	// Clean-up
	libevdev_free(dev);
}

void Virtual_Device::execute(struct input_event& event)
{
	libevdev_uinput_write_event(this->virt_dev, event.type, event.code, event.value);
}