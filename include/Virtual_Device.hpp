#ifndef VIRTUAL_DEVICE_HPP
#define VIRTUAL_DEVICE_HPP

#include <linux/input.h>
#include <libevdev/libevdev-uinput.h>

#include <Device.hpp>

class Virtual_Device
{
	static inline std::string virt_dev_name = "unikey-device";

	private:
		struct libevdev_uinput* virt_dev;

	public:
	// CONSTRUCTOR(S)
		Virtual_Device(std::string filepath);
		Virtual_Device(unsigned type, const BitField& enabled_codes);
		Virtual_Device(std::vector<BitField>& enabled_codes);
	
	// DESTRUCTOR
		~Virtual_Device();

		static void set_name(std::string new_name);
		void init_device(std::vector<BitField>& enabled_codes);
		void execute(struct input_event& event);
};

#endif	// VIRTUAL_DEVICE_HPP