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
		unikey_bluetooth = new BlueZ_Interface("Unikey HID", nullptr, "io.unikey.bluetooth");
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