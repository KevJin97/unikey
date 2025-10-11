#ifndef BLUETOOTH_CONFIGS_HPP
#define BLUETOOTH_CONFIGS_HPP

#include <stdint.h>

static inline constexpr uint8_t hid_report_descriptor[] =
{
	0x05
	0x09
	0xa1
	0x85
	0x09
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