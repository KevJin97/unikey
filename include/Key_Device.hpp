#ifndef KEY_DEVICE_HPP
#define KEY_DEVICE_HPP

#include "Device.hpp"

class Key_Device : public Device
{
	private:
		BitField current_state;
		
	protected:
		void input_monitor() override;

	public:
		Key_Device(struct libevdev* dev);
		~Key_Device() override;
};

#endif // KEY_DEVICE_HPP