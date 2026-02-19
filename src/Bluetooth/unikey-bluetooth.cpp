#include "Bluetooth/unikey-bluetooth.hpp"
#include "Bluetooth/BlueZ_Interface.hpp"
#include "unikey.hpp"

#include <cstdint>

static BlueZ_Interface* unikey_bluetooth = nullptr;

void dbus_enable_unikey_bluetooth(sdbus::MethodCall call)
{
	if (unikey_bluetooth == nullptr)
	{
		unikey_bluetooth = new BlueZ_Interface;
	}
	if (unikey_bluetooth->enable(*unikey_dbus_connection))
	{
		Device::set_event_processor(
			[](const void* data, uint64_t unit_size)
			{
				unikey_bluetooth->send_hid_report(1, data, unit_size);
			}
		);
	}

	call.createReply().send();
}

void dbus_disable_unikey_bluetooth()
{
	Device::set_event_processor();	// Reset event_processor to default function
	delete unikey_bluetooth;
	unikey_bluetooth = nullptr;
}
