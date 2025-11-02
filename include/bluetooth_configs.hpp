#ifndef BLUETOOTH_CONFIGS_HPP
#define BLUETOOTH_CONFIGS_HPP

#include <stdint.h>

const uint8_t hid_report_mouse[] =
{
	0x05, 0x01,	// Usage Page (Generic Desktop)
	0x09, 0x02,	// Usage (Mouse)
	0xA1, 0x01,	// Collection (Application)
	0x09, 0x01,		// Usage (Pointer)
	0xA1, 0x00,		// Collection (Physical)
	0x05, 0x09,			// Usage Page (Buttons)
	0x19, 0x01,			// Usage Minimum (1)
	0x29, 0x03,			// Usage Maximum (3)
	0x15, 0x00,			// Logical Minimum (0)
	0x25, 0x01,			// Logical Maximum (1)
	0x95, 0x03,			// Report Count (3)
	0x75, 0x01,			// Report Size (1)
	0x81, 0x02,			// Input (Data, Var, Abs) - Buttons
	0x95, 0x01,			// Report Count (1)
	0x75, 0x05,			// Report Size (5)
	0x81, 0x03,			// Input (Const, Var, Abs) - Padding
	0x05, 0x01,			// Usage Page (Generic Desktop)
	0x09, 0x30,			// Usage (X)
	0x09, 0x31,			// Usage (Y)
	0x09, 0x38,			// Usage (Wheel)
	0x15, 0x81,			// Logical Minimum (-127)
	0x25, 0x7F,			// Logical Maximum (127)
	0x75, 0x08,			// Report Size (8)
	0x95, 0x03,			// Report Count (3)
	0x81, 0x06,			// Input (Data, Var, Rel) - X, Y, Wheel
	0xC0,			// End Collection
	0xC0		// End Collection
};
const uint8_t hid_report_keyboard[] =
{
    0x05, 0x01,	// Usage Page (Generic Desktop)
    0x09, 0x06,	// Usage (Keyboard)
    0xA1, 0x01,	// Collection (Application)
    0x05, 0x07,		// Usage Page (Keyboard/Keypad)
    0x19, 0xE0,		// Usage Minimum (Keyboard LeftControl)
    0x29, 0xE7,		// Usage Maximum (Keyboard Right GUI)
    0x15, 0x00,		// Logical Minimum (0)
    0x25, 0x01,		// Logical Maximum (1)
    0x75, 0x01,		// Report Size (1)
    0x95, 0x08,		// Report Count (8)
    0x81, 0x02,		// Input (Data,Var,Abs)      ; Modifier byte
    0x95, 0x01,		// Report Count (1)
    0x75, 0x08,		// Report Size (8)
    0x81, 0x03,		// Input (Cnst,Var,Abs)      ; Reserved byte
    0x95, 0x05,		// Report Count (5)
    0x75, 0x01,		// Report Size (1)
    0x05, 0x08,		// Usage Page (LEDs)
    0x19, 0x01,		// Usage Minimum (Num Lock)
    0x29, 0x05,		// Usage Maximum (Kana)
    0x91, 0x02,		// Output (Data,Var,Abs)     ; LED report
    0x95, 0x01,		// Report Count (1)
    0x75, 0x03,		// Report Size (3)
    0x91, 0x03,		// Output (Cnst,Var,Abs)     ; LED report padding
    0x95, 0x06,		// Report Count (6)
    0x75, 0x08,		// Report Size (8)
    0x15, 0x00,		// Logical Minimum (0)
    0x25, 0x65,		// Logical Maximum (101)
    0x05, 0x07,		// Usage Page (Keyboard/Keypad)
    0x19, 0x00,		// Usage Minimum (Reserved (no event indicated))
    0x29, 0x65,		// Usage Maximum (Keyboard Application)
    0x81, 0x00,		// Input (Data,Ary,Abs)      ; Key arrays (6 bytes)
    0xC0		// End Collection
};
const uint8_t hid_report_touchpad[] =
{
    0x05, 0x0D,			// Usage Page (Digitizer)
    0x09, 0x05,			// Usage (Touch Pad)
    0xA1, 0x01,			// Collection (Application)
    0x05, 0x0D,				// Usage Page (Digitizer)
    0x09, 0x22,				// Usage (Finger)
    0xA1, 0x02,				// Collection (Logical)
    0x09, 0x42,					// Usage (Tip Switch)
    0x15, 0x00,					// Logical Minimum (0)
    0x25, 0x01,					// Logical Maximum (1)
    0x75, 0x01,					// Report Size (1)
    0x95, 0x01,					// Report Count (1)
    0x81, 0x02,					// Input (Data,Var,Abs)
    0x09, 0x51,					// Usage (Contact Identifier)
    0x75, 0x07,					// Report Size (7)
    0x95, 0x01,					// Report Count (1)
    0x81, 0x03,					// Input (Cnst,Var,Abs)
    0x05, 0x01,					// Usage Page (Generic Desktop)
    0x09, 0x30,					// Usage (X)
    0x15, 0x00,					// Logical Minimum (0)
    0x26, 0xFF, 0x0F,			// Logical Maximum (4095)
    0x75, 0x10,					// Report Size (16)
    0x95, 0x01,					// Report Count (1)
    0x81, 0x02,					// Input (Data,Var,Abs)
    0x09, 0x31,					// Usage (Y)
    0x26, 0xFF, 0x0F,			// Logical Maximum (4095)
    0x81, 0x02,					// Input (Data,Var,Abs)
    0xC0,					// End Collection
    // Second contact (finger)
    0x09, 0x22,				// Usage (Finger)
    0xA1, 0x02,				// Collection (Logical)
    0x09, 0x42,					// Usage (Tip Switch)
    0x15, 0x00,					// Logical Minimum (0)
    0x25, 0x01,					// Logical Maximum (1)
    0x75, 0x01,					// Report Size (1)
    0x95, 0x01,					// Report Count (1)
    0x81, 0x02,					// Input (Data,Var,Abs)
    0x09, 0x51,					// Usage (Contact Identifier)
    0x75, 0x07,					// Report Size (7)
    0x95, 0x01,					// Report Count (1)
    0x81, 0x03,					// Input (Cnst,Var,Abs)
    0x05, 0x01,					// Usage Page (Generic Desktop)
    0x09, 0x30,					// Usage (X)
    0x15, 0x00,					// Logical Minimum (0)
    0x26, 0xFF, 0x0F,			// Logical Maximum (4095)
    0x75, 0x10,					// Report Size (16)
    0x95, 0x01,					// Report Count (1)
    0x81, 0x02,					// Input (Data,Var,Abs)
    0x09, 0x31,					// Usage (Y)
    0x26, 0xFF, 0x0F,			// Logical Maximum (4095)
    0x81, 0x02,					// Input (Data,Var,Abs)
    0xC0,					// End Collection
    // Button
    0x05, 0x09,				// Usage Page (Button)
    0x19, 0x01,				// Usage Minimum (Button 1)
    0x29, 0x01,				// Usage Maximum (Button 1)
    0x15, 0x00,				// Logical Minimum (0)
    0x25, 0x01,				// Logical Maximum (1)
    0x75, 0x01,				// Report Size (1)
    0x95, 0x01,				// Report Count (1)
    0x81, 0x02,				// Input (Data,Var,Abs)
    0x75, 0x07,				// Report Size (7)
    0x95, 0x01,				// Report Count (1)
    0x81, 0x03,				// Input (Cnst,Ary,Abs)
    0xC0				// End Collection
};

struct hid_mouse_report_t
{
	uint8_t&& report_id;
	uint8_t buttons = 0;
	int8_t coor[2] = { 0 };
	int8_t wheel = 0;
};

struct hid_keyboard_report_t
{
	uint8_t&& report_id;
	uint8_t modifiers = 0;
	uint8_t reserved = 0;
	uint8_t key[6] = { 0 };
};

#endif	// BLUETOOTH_CONFIGS_HPP