#include "Bluetooth/unikey-bluetooth.hpp"
#include "Bluetooth/BlueZ_Interface.hpp"
#include "unikey.hpp"

#include <cstdint>
#include <string>

#include <sdbus-c++/Message.h>

static BlueZ_Interface* unikey_bluetooth = nullptr;

void dbus_set_bluetooth_name(sdbus::MethodCall call)
{
	std::string bluetooth_device_name;
	call >> bluetooth_device_name;
	call.createReply().send();

	if (unikey_bluetooth != nullptr)
	{
		unikey_bluetooth->set_device_name(bluetooth_device_name);
	}
}

void dbus_enable_unikey_bluetooth()
{
	if (unikey_bluetooth == nullptr)
	{
		unikey_bluetooth = new BlueZ_Interface("Unikey HID", unikey_dbus_connection, "io.unikey");
	}
	if (unikey_bluetooth->enable())
	{
		Device::set_event_processor(
			[](const void* data, uint64_t unit_size)
			{
				unikey_bluetooth->send_hid_report(1, data, unit_size);
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
