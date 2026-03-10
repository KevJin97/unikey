#include "Bluetooth/unikey-bluetooth.hpp"
#include "Bluetooth/bluetooth_configs.hpp"
#include "Bluetooth/BlueZ_Interface.hpp"
#include "unikey.hpp"

#include <cstdint>
#include <cstring>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <string>

#include <sdbus-c++/Message.h>

static BlueZ_Interface* unikey_bluetooth = nullptr;
std::unique_ptr<sdbus::IObject> unikey_bluetooth_dbus_obj;

// ─── Persistent HID report state ─────────────────────────────────────────────
// These persist across calls so that key-down events from one batch and
// key-up events from a later batch produce the correct accumulated state.
static hid_mouse_report    s_mouse;
static hid_keyboard_report s_keyboard;
static hid_consumer_report s_consumer;


// ─── Input event processor ───────────────────────────────────────────────────
// Shared between dbus_enable_unikey_bluetooth and dbus_toggle_unikey_bluetooth
// so neither duplicates the event handling logic.
static void process_hid_events(const void* data, uint64_t /*unit_size*/)
{
	const uint64_t* p_count = static_cast<const uint64_t*>(data);
	const struct input_event* events = reinterpret_cast<const struct input_event*>(p_count + 1);
	uint64_t count = *p_count;

	// Relative axes accumulate per batch then reset.
	s_mouse.x      = 0;
	s_mouse.y      = 0;
	s_mouse.wheel  = 0;
	s_mouse.hwheel = 0;

	bool mouse_dirty    = false;
	bool keyboard_dirty = false;
	bool consumer_dirty = false;

	for (uint64_t n = 0; n < count; ++n)
	{
		const struct input_event& ev = events[n];

		switch (ev.type)
		{
			case EV_KEY:
			{
				// ── Mouse buttons ─────────────────────────────────────────────
				if (ev.code >= BTN_MOUSE && ev.code < BTN_MOUSE + 32)
				{
					uint32_t bit = 1u << (ev.code - BTN_MOUSE);
					if (ev.value) s_mouse.buttons |=  bit;
					else          s_mouse.buttons &= ~bit;
					mouse_dirty = true;
					break;
				}

				// ── Consumer control keys ─────────────────────────────────────
				// Checked before the keyboard map: media/brightness/volume keys
				// are not in the keyboard descriptor's usage range 0x00–0x67.
				uint16_t consumer_code = keymap_linux_to_consumer(ev.code);
				if (consumer_code)
				{
					// value 1 = press, 2 = repeat (treat as press), 0 = release
					if (ev.value)
						s_consumer.usage_code = consumer_code;
					else if (s_consumer.usage_code == consumer_code)
						s_consumer.usage_code = 0x0000;
					consumer_dirty = true;
					break;
				}

				// ── Keyboard keys ─────────────────────────────────────────────
				uint8_t hid = keymap_linux_to_hid(static_cast<uint8_t>(ev.code));
				if (hid == 0x00)
					break;

				if (hid >= 0xE0 && hid <= 0xE7)
				{
					// Modifier bitmap: LCtrl=bit0 … RMeta=bit7
					uint8_t bit = static_cast<uint8_t>(1u << (hid - 0xE0));
					if (ev.value) s_keyboard.modifiers |=  bit;
					else          s_keyboard.modifiers &= ~bit;
				}
				else
				{
					// Key presence bitmap across keys[0..12].
					// The descriptor covers usages 0x00–0x67 (104 bits = 13 bytes,
					// Var|Abs). Bit N of the bitmap = usage 0x00+N.
					// Usage 0x00 is "no event" and is never set intentionally.
					// All valid regular key codes returned by keymap_linux_to_hid
					// are in the range 0x01–0x67, so byte_idx is always 0–12.
					uint8_t byte_idx = hid / 8;
					uint8_t bit_mask = static_cast<uint8_t>(1u << (hid % 8));
					if (ev.value) s_keyboard.keys[byte_idx] |=  bit_mask;
					else          s_keyboard.keys[byte_idx] &= ~bit_mask;
				}
				keyboard_dirty = true;
				break;
			}

			case EV_REL:
				switch (ev.code)
				{
					case REL_X:      s_mouse.x      = ev.value; break;
					case REL_Y:      s_mouse.y      = ev.value; break;
					case REL_WHEEL:  s_mouse.wheel  = ev.value; break;
					case REL_HWHEEL: s_mouse.hwheel = ev.value; break;
				}
				mouse_dirty = true;
				break;
		}
	}

	if (mouse_dirty)
		unikey_bluetooth->send_hid_report(1, &s_mouse, sizeof(s_mouse));

	if (keyboard_dirty)
		unikey_bluetooth->send_hid_report(2, &s_keyboard, sizeof(s_keyboard));

	if (consumer_dirty)
		unikey_bluetooth->send_hid_report(3, &s_consumer, sizeof(s_consumer));
}

// Send all-zero keyboard and consumer reports so the host sees every key
// released before we tear down the connection.
static void release_all_keys()
{
	if (!unikey_bluetooth) return;

	std::memset(&s_keyboard, 0, sizeof(s_keyboard));
	std::memset(&s_consumer, 0, sizeof(s_consumer));

	unikey_bluetooth->send_hid_report(2, &s_keyboard, sizeof(s_keyboard));
	unikey_bluetooth->send_hid_report(3, &s_consumer, sizeof(s_consumer));
}


// ─── D-Bus interface ──────────────────────────────────────────────────────────

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
	if (unikey_bluetooth == nullptr)
	{
		unikey_bluetooth = new BlueZ_Interface("Unikey HID", unikey_dbus_connection, "io.unikey");
	}

	if (unikey_bluetooth->enable())
	{
		Device::set_event_processor(process_hid_events);
	}
	else
	{
		delete unikey_bluetooth;
		unikey_bluetooth = nullptr;
	}
}

void dbus_disable_unikey_bluetooth()
{
	release_all_keys();
	Device::set_event_processor();	// Reset to default
	delete unikey_bluetooth;
	unikey_bluetooth = nullptr;
}

void dbus_toggle_unikey_bluetooth()
{
	if (unikey_bluetooth == nullptr)
	{
		unikey_bluetooth = new BlueZ_Interface("Unikey HID", unikey_dbus_connection, "io.unikey");

		if (unikey_bluetooth->enable())
		{
			if (Device::return_grab_state())
			{
				Device::trigger_activation();
			}

			Device::set_event_processor(process_hid_events);
			return;
		}
	}
	else
	{
		if (Device::return_grab_state())
		{
			Device::trigger_activation();
		}

		release_all_keys();
		Device::set_event_processor();	// Reset to default
	}

	delete unikey_bluetooth;
	unikey_bluetooth = nullptr;
}


// ─── Linux → HID keymap ──────────────────────────────────────────────────────
/*
	Based on HID usage tables version 1.21 specified
	at https://usb.org/sites/default/files/hut1_21.pdf

	Only covers the keyboard descriptor's usage range 0x01–0x67 for regular
	keys and 0xE0–0xE7 for modifiers. Consumer control keys (media, volume,
	brightness) return 0x00 here; they are handled by keymap_linux_to_consumer
	and routed to report ID 3 instead.
*/
uint8_t keymap_linux_to_hid(uint8_t linux_key)
{
	static std::vector<uint8_t> keymap;

	if (keymap.empty())
	{
		keymap.resize(256, 0x00);
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
		keymap[KEY_F1]  = 0x3a;
		keymap[KEY_F2]  = 0x3b;
		keymap[KEY_F3]  = 0x3c;
		keymap[KEY_F4]  = 0x3d;
		keymap[KEY_F5]  = 0x3e;
		keymap[KEY_F6]  = 0x3f;
		keymap[KEY_F7]  = 0x40;
		keymap[KEY_F8]  = 0x41;
		keymap[KEY_F9]  = 0x42;
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
		keymap[KEY_POWER]      = 0x66;
		keymap[KEY_GRAVE]      = 0x35;
		keymap[KEY_ESC]        = 0x29;
		keymap[KEY_ENTER]      = 0x28;
		keymap[KEY_SPACE]      = 0x2c;
		keymap[KEY_INSERT]     = 0x49;
		keymap[KEY_DELETE]     = 0x4c;
		keymap[KEY_HOME]       = 0x4a;
		keymap[KEY_PAGEUP]     = 0x4b;
		keymap[KEY_END]        = 0x4d;
		keymap[KEY_PAGEDOWN]   = 0x4e;
		keymap[KEY_SEMICOLON]  = 0x33;
		keymap[KEY_APOSTROPHE] = 0x34;
		keymap[KEY_MINUS]      = 0x2d;
		keymap[KEY_EQUAL]      = 0x2e;
		keymap[KEY_BACKSPACE]  = 0x2a;
		keymap[KEY_TAB]        = 0x2b;
		keymap[KEY_CAPSLOCK]   = 0x39;
		keymap[KEY_COMMA]      = 0x36;
		keymap[KEY_DOT]        = 0x37;
		keymap[KEY_SLASH]      = 0x38;
		keymap[KEY_BACKSLASH]  = 0x31;
		keymap[KEY_SYSRQ]      = 0x46;
		keymap[KEY_NUMLOCK]    = 0x53;
		keymap[KEY_SCROLLLOCK] = 0x47;
		keymap[KEY_LEFTBRACE]  = 0x2f;
		keymap[KEY_RIGHTBRACE] = 0x30;
		keymap[KEY_UP]         = 0x52;
		keymap[KEY_DOWN]       = 0x51;
		keymap[KEY_LEFT]       = 0x50;
		keymap[KEY_RIGHT]      = 0x4f;

		// NOTE: KEY_MUTE, KEY_VOLUMEUP, KEY_VOLUMEDOWN are intentionally NOT
		// mapped here. Their original HID keyboard-page codes (0x7F-0x81)
		// exceed the descriptor bitmap's Usage Maximum (0x67) and would write
		// out-of-bounds into keys[]. They are handled in keymap_linux_to_consumer
		// and sent via report ID 3 (Consumer Control).

		// Modifiers — returned as 0xE0–0xE7; handled via the modifier byte
		keymap[KEY_LEFTCTRL]   = 0xe0;
		keymap[KEY_LEFTSHIFT]  = 0xe1;
		keymap[KEY_LEFTALT]    = 0xe2;
		keymap[KEY_LEFTMETA]   = 0xe3;
		keymap[KEY_RIGHTCTRL]  = 0xe4;
		keymap[KEY_RIGHTSHIFT] = 0xe5;
		keymap[KEY_RIGHTALT]   = 0xe6;
		keymap[KEY_RIGHTMETA]  = 0xe7;

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

		// Keypad miscellaneous
		keymap[KEY_KPSLASH]      = 0x54;
		keymap[KEY_KPASTERISK]   = 0x55;
		keymap[KEY_KPMINUS]      = 0x56;
		keymap[KEY_KPPLUS]       = 0x57;
		keymap[KEY_KPENTER]      = 0x58;
		keymap[KEY_KPDOT]        = 0x63;
		keymap[KEY_KPEQUAL]      = 0x67;
		keymap[KEY_KPLEFTPAREN]  = 0xb6;
		keymap[KEY_KPRIGHTPAREN] = 0xb7;
		keymap[KEY_KPPLUSMINUS]  = 0xd7;
	}
	
	return keymap[linux_key];
}


// ─── Linux → HID Consumer page keymap ────────────────────────────────────────
/*
	Maps Linux EV_KEY media/consumer codes to 16-bit HID Consumer page usage
	codes (HUT 1.21 §15). The consumer report (ID 3) is a single-usage Array
	field: send the usage on press, 0x0000 on release.
*/
uint16_t keymap_linux_to_consumer(uint16_t linux_key)
{
	switch (linux_key)
	{
		// Transport controls
		case KEY_PLAYPAUSE:    return 0x00CD;	// Play/Pause
		case KEY_STOPCD:       return 0x00B7;	// Stop
		case KEY_NEXTSONG:     return 0x00B5;	// Scan Next Track
		case KEY_PREVIOUSSONG: return 0x00B6;	// Scan Previous Track
		case KEY_FASTFORWARD:  return 0x00B3;	// Fast Forward
		case KEY_REWIND:       return 0x00B4;	// Rewind
		case KEY_EJECTCD:      return 0x00B8;	// Eject

		// Volume and audio
		case KEY_MUTE:         return 0x00E2;	// Mute
		case KEY_VOLUMEUP:     return 0x00E9;	// Volume Increment
		case KEY_VOLUMEDOWN:   return 0x00EA;	// Volume Decrement

		// Display brightness
		case KEY_BRIGHTNESSUP:   return 0x006F;	// Display Brightness Increment
		case KEY_BRIGHTNESSDOWN: return 0x0070;	// Display Brightness Decrement

		// Application launch
		case KEY_MEDIA:    return 0x0183;	// AL Media Select
		case KEY_MAIL:     return 0x018A;	// AL Email Reader
		case KEY_CALC:     return 0x0192;	// AL Calculator

		// Browser / navigation
		case KEY_HOMEPAGE: return 0x0223;	// AC Home
		case KEY_SEARCH:   return 0x0221;	// AC Search
		case KEY_BACK:     return 0x0224;	// AC Back
		case KEY_FORWARD:  return 0x0225;	// AC Forward
		case KEY_REFRESH:  return 0x0227;	// AC Refresh
		case KEY_STOP:     return 0x0226;	// AC Stop

		default: return 0x0000;
	}
}