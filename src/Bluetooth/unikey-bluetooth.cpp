#include "Bluetooth/unikey-bluetooth.hpp"
#include "Bluetooth/bluetooth_configs.hpp"
#include "Bluetooth/BlueZ_Interface.hpp"
#include "unikey.hpp"

#include <cstdint>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <string>

#include <sdbus-c++/Message.h>

static BlueZ_Interface* unikey_bluetooth = nullptr;
std::unique_ptr<sdbus::IObject> unikey_bluetooth_dbus_obj;

void register_bluetooth_dbus_cmds()
{
	unikey_bluetooth_dbus_obj = sdbus::createObject(*unikey_dbus_connection, "/io/unikey/Bluetooth");
	
	unikey_bluetooth_dbus_obj->registerMethod("io.unikey.Bluetooth.Methods",
		"SetDisplayName", "s", "", &dbus_set_bluetooth_name);

	unikey_bluetooth_dbus_obj->registerMethod("EnableBluetooth")
		.onInterface("io.unikey.Bluetooth.Methods")
			.implementedAs(&dbus_enable_unikey_bluetooth);

	unikey_bluetooth_dbus_obj->registerMethod("DisableBluetooth")
		.onInterface("io.unikey.Bluetooth.Methods")
			.implementedAs(&dbus_disable_unikey_bluetooth);
	
	unikey_bluetooth_dbus_obj->registerMethod("ToggleBluetooth")
		.onInterface("io.unikey.Bluetooth.Methods")
			.implementedAs(&dbus_toggle_unikey_bluetooth);

	unikey_bluetooth_dbus_obj->finishRegistration();
}

void dbus_set_bluetooth_name(sdbus::MethodCall call)
{
	std::string bluetooth_device_name;
	call >> bluetooth_device_name;
	call.createReply().send();

	if (bluetooth_device_name.empty())
	{
		return;
	}
	else if (unikey_bluetooth == nullptr)
	{
		unikey_bluetooth = new BlueZ_Interface(bluetooth_device_name, unikey_dbus_connection, "io.unikey");
	}
	else
	{
		unikey_bluetooth->set_device_name(bluetooth_device_name);
	}
}

void dbus_enable_unikey_bluetooth()
{
	static hid_mouse_report mouse;

	if (unikey_bluetooth == nullptr)
	{
		unikey_bluetooth = new BlueZ_Interface("Unikey HID", unikey_dbus_connection, "io.unikey");
	}

	if (unikey_bluetooth->enable())
	{
		Device::set_event_processor(
			[](const void* data, uint64_t unit_size)
			{
				const uint64_t* p_count = (const uint64_t*)data;
				const struct input_event* events = (const struct input_event*)(p_count + 1);
				uint64_t count = *p_count;

				mouse.x = 0;
				mouse.y = 0;
				mouse.wheel = 0;
				mouse.hwheel = 0;

				for (uint64_t n = 0; n < count; ++n)
				{
					switch(events[n].type)
					{
						case EV_KEY:
							if (events[n].code >= BTN_MOUSE && events[n].code < BTN_MOUSE + 32)
							{
								uint32_t bit = 1u << (events[n].code - BTN_MOUSE);
								
								if (events[n].value)
								{
									mouse.buttons |= bit;
								}
								else
								{
									mouse.buttons &= ~bit;
								}
							}
							break;

						case EV_REL:
							switch(events[n].code)
							{
								case REL_X:
									mouse.x = events[n].value;
									break;

								case REL_Y:
									mouse.y = events[n].value;
									break;

								case REL_WHEEL:
									mouse.wheel = events[n].value;
									break;

								case REL_HWHEEL:
									mouse.hwheel = events[n].value;
									break;
							}
							break;
					}
				}

				unikey_bluetooth->send_hid_report(1, &mouse, sizeof(mouse));
			}
		);
	}
	else
	{
		delete unikey_bluetooth;
		unikey_bluetooth = nullptr;
	}
}

void dbus_disable_unikey_bluetooth()
{
	Device::set_event_processor();	// Reset event_processor to default function
	delete unikey_bluetooth;
	unikey_bluetooth = nullptr;
}

void dbus_toggle_unikey_bluetooth()
{
	static hid_mouse_report mouse;

	if (unikey_bluetooth == nullptr)
	{
		unikey_bluetooth = new BlueZ_Interface("Unikey HID", unikey_dbus_connection, "io.unikey");
		if (unikey_bluetooth->enable())
		{
			if (Device::return_grab_state())
			{
				Device::trigger_activation();
			}

			Device::set_event_processor(
				[](const void* data, uint64_t unit_size)
				{
					const uint64_t* p_count = (const uint64_t*)data;
					const struct input_event* events = (const struct input_event*)(p_count + 1);
					uint64_t count = *p_count;

					mouse.x = 0;
					mouse.y = 0;
					mouse.wheel = 0;
					mouse.hwheel = 0;

					for (uint64_t n = 0; n < count; ++n)
					{
						switch(events[n].type)
						{
							case EV_KEY:
								if (events[n].code >= BTN_MOUSE && events[n].code < BTN_MOUSE + 32)
								{
									uint32_t bit = 1u << (events[n].code - BTN_MOUSE);
									
									if (events[n].value)
									{
										mouse.buttons |= bit;
									}
									else
									{
										mouse.buttons &= ~bit;
									}
								}
								break;

							case EV_REL:
								switch(events[n].code)
								{
									case REL_X:
										mouse.x = events[n].value;
										break;

									case REL_Y:
										mouse.y = events[n].value;
										break;

									case REL_WHEEL:
										mouse.wheel = events[n].value;
										break;

									case REL_HWHEEL:
										mouse.hwheel = events[n].value;
										break;
								}
								break;
						}
					}

					unikey_bluetooth->send_hid_report(1, &mouse, sizeof(mouse));
				}
			);
			
			return;
		}
	}
	else
	{
		if (Device::return_grab_state())
		{
			Device::trigger_activation();
		}
		// Don't forget to release all keys
		Device::set_event_processor();	// Reset event_processor to default function
	}

	delete unikey_bluetooth;
	unikey_bluetooth = nullptr;
}

/*
	Based on HID usage tables version 1.21 specified 
	at https://usb.org/sites/default/files/hut1_21.pdf 
*/
uint8_t keymap_linux_to_hid(uint8_t linux_key)
{
	static std::vector<uint8_t> keymap;

	if (keymap.empty())
	{
		keymap.resize(256);
		keymap[KEY_RESERVED] = 0x00;

		// Alphanumeric
		keymap[KEY_A] = 0x04;
		keymap[KEY_B] = 0x05;
		keymap[KEY_C] = 0x06;
		keymap[KEY_D] = 0x07;
		keymap[KEY_E] = 0x08;
		keymap[KEY_F] = 0x09;
		keymap[KEY_G] = 0x0a;
		keymap[KEY_H] = 0x0b;
		keymap[KEY_I] = 0x0c;
		keymap[KEY_J] = 0x0d;
		keymap[KEY_K] = 0x0e;
		keymap[KEY_L] = 0x0f;
		keymap[KEY_M] = 0x10;
		keymap[KEY_N] = 0x11;
		keymap[KEY_O] = 0x12;
		keymap[KEY_P] = 0x13;
		keymap[KEY_Q] = 0x14;
		keymap[KEY_R] = 0x15;
		keymap[KEY_S] = 0x16;
		keymap[KEY_T] = 0x17;
		keymap[KEY_U] = 0x18;
		keymap[KEY_V] = 0x19;
		keymap[KEY_W] = 0x1a;
		keymap[KEY_X] = 0x1b;
		keymap[KEY_Y] = 0x1c;
		keymap[KEY_Z] = 0x1d;
		keymap[KEY_1] = 0x1e;
		keymap[KEY_2] = 0x1f;
		keymap[KEY_3] = 0x20;
		keymap[KEY_4] = 0x21;
		keymap[KEY_5] = 0x22;
		keymap[KEY_6] = 0x23;
		keymap[KEY_7] = 0x24;
		keymap[KEY_8] = 0x25;
		keymap[KEY_9] = 0x26;
		keymap[KEY_0] = 0x27;

		// Function
		keymap[KEY_F1] = 0x3a;
		keymap[KEY_F2] = 0x3b;
		keymap[KEY_F3] = 0x3c;
		keymap[KEY_F4] = 0x3d;
		keymap[KEY_F5] = 0x3e;
		keymap[KEY_F6] = 0x3f;
		keymap[KEY_F7] = 0x40;
		keymap[KEY_F8] = 0x41;
		keymap[KEY_F9] = 0x42;
		keymap[KEY_F10] = 0x43;
		keymap[KEY_F11] = 0x44;
		keymap[KEY_F12] = 0x45;
		keymap[KEY_F13] = 0x68;
		keymap[KEY_F14] = 0x69;
		keymap[KEY_F15] = 0x6a;
		keymap[KEY_F16] = 0x6b;
		keymap[KEY_F17] = 0x6c;
		keymap[KEY_F18] = 0x6d;
		keymap[KEY_F19] = 0x6e;
		keymap[KEY_F20] = 0x6f;
		keymap[KEY_F21] = 0x70;
		keymap[KEY_F22] = 0x71;
		keymap[KEY_F23] = 0x72;
		keymap[KEY_F24] = 0x73;

		// Miscellaneous
		keymap[KEY_POWER] = 0x66;
		keymap[KEY_GRAVE] = 0x35;
		keymap[KEY_ESC] = 0x29;
		keymap[KEY_ENTER] = 0x28;
		keymap[KEY_SPACE] = 0x2c;
		keymap[KEY_INSERT] = 0x49;
		keymap[KEY_DELETE] = 0x4c;
		keymap[KEY_HOME] = 0x4a;
		keymap[KEY_PAGEUP] = 0x4b;
		keymap[KEY_END] = 0x4d;
		keymap[KEY_PAGEDOWN] = 0x4e;
		keymap[KEY_SEMICOLON] = 0x33;
		keymap[KEY_APOSTROPHE] = 0x34;
		keymap[KEY_MINUS] = 0x2d;
		keymap[KEY_EQUAL] = 0x2e;
		keymap[KEY_BACKSPACE] = 0x2a;
		keymap[KEY_TAB] = 0x2b;
		keymap[KEY_CAPSLOCK] = 0x39;
		keymap[KEY_COMMA] = 0x36;
		keymap[KEY_DOT] = 0x37;
		keymap[KEY_SLASH] = 0x38;
		keymap[KEY_BACKSLASH] = 0x31;
		keymap[KEY_SYSRQ] = 0x46;
		keymap[KEY_NUMLOCK] = 0x53;
		keymap[KEY_SCROLLLOCK] = 0x47;
		keymap[KEY_LEFTBRACE] = 0x2f;
		keymap[KEY_RIGHTBRACE] = 0x30;
		keymap[KEY_UP] = 0x52;
		keymap[KEY_DOWN] = 0x51;
		keymap[KEY_LEFT] = 0x50;
		keymap[KEY_RIGHT] = 0x4f;
		keymap[KEY_MUTE] = 0x7f;
		keymap[KEY_VOLUMEDOWN] = 0x81;
		keymap[KEY_VOLUMEUP] = 0x80;
		// keymap[KEY_PLAYPAUSE] = 0x;
		// keymap[KEY_BRIGHTNESSDOWN] = 0x;
		// keymap[KEY_BRIGHTNESSUP] = 0x;
		// keymap[KEY_COMPOSE] = 0x;

		// Modifiers
		keymap[KEY_LEFTCTRL] = 0xe0;
		keymap[KEY_LEFTSHIFT] = 0xe1;
		keymap[KEY_LEFTALT] = 0xe2;
		keymap[KEY_LEFTMETA] = 0xe3;
		keymap[KEY_RIGHTCTRL] = 0xe4;
		keymap[KEY_RIGHTSHIFT] = 0xe5;
		keymap[KEY_RIGHTALT] = 0xe6;
		keymap[KEY_RIGHTMETA] = 0xe7;

		// Keypad
		keymap[KEY_KP1] = 0x59;
		keymap[KEY_KP2] = 0x5a;
		keymap[KEY_KP3] = 0x5b;
		keymap[KEY_KP4] = 0x5c;
		keymap[KEY_KP5] = 0x5d;
		keymap[KEY_KP6] = 0x5e;
		keymap[KEY_KP7] = 0x5f;
		keymap[KEY_KP8] = 0x60;
		keymap[KEY_KP9] = 0x61;
		keymap[KEY_KP0] = 0x62;

		// Keypad Miscellaneous
		keymap[KEY_KPSLASH] = 0x54;
		keymap[KEY_KPASTERISK] = 0x55;
		keymap[KEY_KPMINUS] = 0x56;
		keymap[KEY_KPPLUS] = 0x57;
		keymap[KEY_KPENTER] = 0x58;
		keymap[KEY_KPDOT] = 0x63;
		keymap[KEY_KPEQUAL] = 0x67;
		keymap[KEY_KPLEFTPAREN] = 0xb6;
		keymap[KEY_KPRIGHTPAREN] = 0xb7;
		keymap[KEY_KPPLUSMINUS] = 0xd7;
		// keymap[KEY_KPCOMMA] = 0x;
	}
	
	return keymap[linux_key];
}
