#include "BlueZ_Interface.hpp"

#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>

#include <linux/input-event-codes.h>

// BlueZ D-Bus service and interface constants
namespace BlueZ
{
	static constexpr const char* SERVICE = "org.bluez";
	static constexpr const char* ADAPTER_INTERFACE = "org.bluez.Adapter1";
	static constexpr const char* DEVICE_INTERFACE = "org.bluez.Device1";
	static constexpr const char* GATT_MANAGER_INTERFACE = "org.bluez.GattManager1";
	static constexpr const char* LE_ADVERTISING_MANAGER_INTERFACE = "org.bluez.LEAdvertisingManager1";
	static constexpr const char* LE_ADVERTISEMENT_INTERFACE = "org.bluez.LEAdvertisement1";
	static constexpr const char* GATT_SERVICE_INTERFACE = "org.bluez.GattService1";
	static constexpr const char* GATT_CHARACTERISTIC_INTERFACE = "org.bluez.GattCharacteristic1";
	static constexpr const char* GATT_DESCRIPTOR_INTERFACE = "org.bluez.GattDescriptor1";
	static constexpr const char* AGENT_INTERFACE = "org.bluez.Agent1";
	static constexpr const char* AGENT_MANAGER_INTERFACE = "org.bluez.AgentManager1";
	static constexpr const char* PROPERTIES_INTERFACE = "org.freedesktop.DBus.Properties";
	static constexpr const char* OBJECT_MANAGER_INTERFACE = "org.freedesktop.DBus.ObjectManager";
}

// Standard BLE UUIDs
namespace BLE_UUID
{
	// Services
	static constexpr const char* HID_SERVICE = "00001812-0000-1000-8000-00805f9b34fb";
	static constexpr const char* BATTERY_SERVICE = "0000180f-0000-1000-8000-00805f9b34fb";
	static constexpr const char* DEVICE_INFO_SERVICE = "0000180a-0000-1000-8000-00805f9b34fb";
	
	// HID Characteristics
	static constexpr const char* HID_REPORT = "00002a4d-0000-1000-8000-00805f9b34fb";
	static constexpr const char* HID_REPORT_MAP = "00002a4b-0000-1000-8000-00805f9b34fb";
	static constexpr const char* HID_INFORMATION = "00002a4a-0000-1000-8000-00805f9b34fb";
	static constexpr const char* HID_CONTROL_POINT = "00002a4c-0000-1000-8000-00805f9b34fb";
	static constexpr const char* PROTOCOL_MODE = "00002a4e-0000-1000-8000-00805f9b34fb";

	// Battery Characteristics
	static constexpr const char* BATTERY_LEVEL = "00002a19-0000-1000-8000-00805f9b34fb";

	// Device Info Characteristics
	static constexpr const char* MANUFACTURER_NAME = "00002a29-0000-1000-8000-00805f9b34fb";
	static constexpr const char* MODEL_NUMBER = "00002a24-0000-1000-8000-00805f9b34fb";
	static constexpr const char* PNP_ID = "00002a50-0000-1000-8000-00805f9b34fb";

	// Descriptors
	static constexpr const char* REPORT_REFERENCE = "00002908-0000-1000-8000-00805f9b34fb";
	static constexpr const char* CCC_DESCRIPTOR = "00002902-0000-1000-8000-00805f9b34fb";
}

// HID Report Map - Combined Keyboard and Mouse
static const std::vector<uint8_t> HID_REPORT_MAP_DATA = {
	// Keyboard Report (Report ID 1)
	0x05, 0x01,		// Usage Page (Generic Desktop)
	0x09, 0x06,		// Usage (Keyboard)
	0xA1, 0x01,		// Collection (Application)
	0x85, 0x01,		//   Report ID (1)
	0x05, 0x07,		//   Usage Page (Keyboard/Keypad)
	0x19, 0xE0,		//   Usage Minimum (Left Control)
	0x29, 0xE7,		//   Usage Maximum (Right GUI)
	0x15, 0x00,		//   Logical Minimum (0)
	0x25, 0x01,		//   Logical Maximum (1)
	0x75, 0x01,		//   Report Size (1)
	0x95, 0x08,		//   Report Count (8)
	0x81, 0x02,		//   Input (Data, Variable, Absolute) - Modifier byte
	0x95, 0x01,		//   Report Count (1)
	0x75, 0x08,		//   Report Size (8)
	0x81, 0x01,		//   Input (Constant) - Reserved byte
	0x95, 0x06,		//   Report Count (6)
	0x75, 0x08,		//   Report Size (8)
	0x15, 0x00,		//   Logical Minimum (0)
	0x25, 0x65,		//   Logical Maximum (101)
	0x05, 0x07,		//   Usage Page (Keyboard/Keypad)
	0x19, 0x00,		//   Usage Minimum (0)
	0x29, 0x65,		//   Usage Maximum (101)
	0x81, 0x00,		//   Input (Data, Array) - Key arrays
	0xC0,			  // End Collection

	// Mouse Report (Report ID 2)
	0x05, 0x01,		// Usage Page (Generic Desktop)
	0x09, 0x02,		// Usage (Mouse)
	0xA1, 0x01,		// Collection (Application)
	0x85, 0x02,		//   Report ID (2)
	0x09, 0x01,		//   Usage (Pointer)
	0xA1, 0x00,		//   Collection (Physical)
	0x05, 0x09,		//	 Usage Page (Buttons)
	0x19, 0x01,		//	 Usage Minimum (Button 1)
	0x29, 0x05,		//	 Usage Maximum (Button 5)
	0x15, 0x00,		//	 Logical Minimum (0)
	0x25, 0x01,		//	 Logical Maximum (1)
	0x95, 0x05,		//	 Report Count (5)
	0x75, 0x01,		//	 Report Size (1)
	0x81, 0x02,		//	 Input (Data, Variable, Absolute) - Buttons
	0x95, 0x01,		//	 Report Count (1)
	0x75, 0x03,		//	 Report Size (3)
	0x81, 0x01,		//	 Input (Constant) - Padding
	0x05, 0x01,		//	 Usage Page (Generic Desktop)
	0x09, 0x30,		//	 Usage (X)
	0x09, 0x31,		//	 Usage (Y)
	0x09, 0x38,		//	 Usage (Wheel)
	0x15, 0x81,		//	 Logical Minimum (-127)
	0x25, 0x7F,		//	 Logical Maximum (127)
	0x75, 0x08,		//	 Report Size (8)
	0x95, 0x03,		//	 Report Count (3)
	0x81, 0x06,		//	 Input (Data, Variable, Relative) - X, Y, Wheel
	0xC0,			  //   End Collection
	0xC0,			  // End Collection

	// Consumer Control Report (Report ID 3)
	0x05, 0x0C,		// Usage Page (Consumer)
	0x09, 0x01,		// Usage (Consumer Control)
	0xA1, 0x01,		// Collection (Application)
	0x85, 0x03,		//   Report ID (3)
	0x15, 0x00,		//   Logical Minimum (0)
	0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
	0x19, 0x00,		//   Usage Minimum (0)
	0x2A, 0xFF, 0x03,  //   Usage Maximum (1023)
	0x75, 0x10,		//   Report Size (16)
	0x95, 0x01,		//   Report Count (1)
	0x81, 0x00,		//   Input (Data, Array)
	0xC0			   // End Collection
};

// Linux keycode to HID usage code lookup table
static const uint8_t LINUX_TO_HID_KEYCODE[] = {
	0x00, // KEY_RESERVED (0)
	0x29, // KEY_ESC (1)
	0x1E, // KEY_1 (2)
	0x1F, // KEY_2 (3)
	0x20, // KEY_3 (4)
	0x21, // KEY_4 (5)
	0x22, // KEY_5 (6)
	0x23, // KEY_6 (7)
	0x24, // KEY_7 (8)
	0x25, // KEY_8 (9)
	0x26, // KEY_9 (10)
	0x27, // KEY_0 (11)
	0x2D, // KEY_MINUS (12)
	0x2E, // KEY_EQUAL (13)
	0x2A, // KEY_BACKSPACE (14)
	0x2B, // KEY_TAB (15)
	0x14, // KEY_Q (16)
	0x1A, // KEY_W (17)
	0x08, // KEY_E (18)
	0x15, // KEY_R (19)
	0x17, // KEY_T (20)
	0x1C, // KEY_Y (21)
	0x18, // KEY_U (22)
	0x0C, // KEY_I (23)
	0x12, // KEY_O (24)
	0x13, // KEY_P (25)
	0x2F, // KEY_LEFTBRACE (26)
	0x30, // KEY_RIGHTBRACE (27)
	0x28, // KEY_ENTER (28)
	0xE0, // KEY_LEFTCTRL (29) - modifier
	0x04, // KEY_A (30)
	0x16, // KEY_S (31)
	0x07, // KEY_D (32)
	0x09, // KEY_F (33)
	0x0A, // KEY_G (34)
	0x0B, // KEY_H (35)
	0x0D, // KEY_J (36)
	0x0E, // KEY_K (37)
	0x0F, // KEY_L (38)
	0x33, // KEY_SEMICOLON (39)
	0x34, // KEY_APOSTROPHE (40)
	0x35, // KEY_GRAVE (41)
	0xE1, // KEY_LEFTSHIFT (42) - modifier
	0x31, // KEY_BACKSLASH (43)
	0x1D, // KEY_Z (44)
	0x1B, // KEY_X (45)
	0x06, // KEY_C (46)
	0x19, // KEY_V (47)
	0x05, // KEY_B (48)
	0x11, // KEY_N (49)
	0x10, // KEY_M (50)
	0x36, // KEY_COMMA (51)
	0x37, // KEY_DOT (52)
	0x38, // KEY_SLASH (53)
	0xE5, // KEY_RIGHTSHIFT (54) - modifier
	0x55, // KEY_KPASTERISK (55)
	0xE2, // KEY_LEFTALT (56) - modifier
	0x2C, // KEY_SPACE (57)
	0x39, // KEY_CAPSLOCK (58)
	0x3A, // KEY_F1 (59)
	0x3B, // KEY_F2 (60)
	0x3C, // KEY_F3 (61)
	0x3D, // KEY_F4 (62)
	0x3E, // KEY_F5 (63)
	0x3F, // KEY_F6 (64)
	0x40, // KEY_F7 (65)
	0x41, // KEY_F8 (66)
	0x42, // KEY_F9 (67)
	0x43, // KEY_F10 (68)
	0x53, // KEY_NUMLOCK (69)
	0x47, // KEY_SCROLLLOCK (70)
	0x5F, // KEY_KP7 (71)
	0x60, // KEY_KP8 (72)
	0x61, // KEY_KP9 (73)
	0x56, // KEY_KPMINUS (74)
	0x5C, // KEY_KP4 (75)
	0x5D, // KEY_KP5 (76)
	0x5E, // KEY_KP6 (77)
	0x57, // KEY_KPPLUS (78)
	0x59, // KEY_KP1 (79)
	0x5A, // KEY_KP2 (80)
	0x5B, // KEY_KP3 (81)
	0x62, // KEY_KP0 (82)
	0x63, // KEY_KPDOT (83)
	0x00, // (84)
	0x00, // KEY_ZENKAKUHANKAKU (85)
	0x64, // KEY_102ND (86)
	0x44, // KEY_F11 (87)
	0x45, // KEY_F12 (88)
	0x00, // KEY_RO (89)
	0x00, // KEY_KATAKANA (90)
	0x00, // KEY_HIRAGANA (91)
	0x00, // KEY_HENKAN (92)
	0x00, // KEY_KATAKANAHIRAGANA (93)
	0x00, // KEY_MUHENKAN (94)
	0x00, // KEY_KPJPCOMMA (95)
	0x58, // KEY_KPENTER (96)
	0xE4, // KEY_RIGHTCTRL (97) - modifier
	0x54, // KEY_KPSLASH (98)
	0x46, // KEY_SYSRQ (99)
	0xE6, // KEY_RIGHTALT (100) - modifier
	0x00, // KEY_LINEFEED (101)
	0x4A, // KEY_HOME (102)
	0x52, // KEY_UP (103)
	0x4B, // KEY_PAGEUP (104)
	0x50, // KEY_LEFT (105)
	0x4F, // KEY_RIGHT (106)
	0x4D, // KEY_END (107)
	0x51, // KEY_DOWN (108)
	0x4E, // KEY_PAGEDOWN (109)
	0x49, // KEY_INSERT (110)
	0x4C, // KEY_DELETE (111)
	0x00, // KEY_MACRO (112)
	0x00, // KEY_MUTE (113)
	0x00, // KEY_VOLUMEDOWN (114)
	0x00, // KEY_VOLUMEUP (115)
	0x66, // KEY_POWER (116)
	0x67, // KEY_KPEQUAL (117)
	0x00, // KEY_KPPLUSMINUS (118)
	0x48, // KEY_PAUSE (119)
	0x00, // KEY_SCALE (120)
	0x00, // KEY_KPCOMMA (121)
	0x00, // KEY_HANGEUL (122)
	0x00, // KEY_HANJA (123)
	0x00, // KEY_YEN (124)
	0xE3, // KEY_LEFTMETA (125) - modifier
	0xE7, // KEY_RIGHTMETA (126) - modifier
	0x65, // KEY_COMPOSE (127)
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

BlueZ_Interface::BlueZ_Interface()
	: device_name("Unikey BLE HID")
{
}

BlueZ_Interface::BlueZ_Interface(const std::string& device_name)
	: device_name(device_name)
{
}

BlueZ_Interface::~BlueZ_Interface()
{
	shutdown();
}

// ============================================================================
// Public Interface - Lifecycle Management
// ============================================================================

bool BlueZ_Interface::initialize()
{
	if (is_running.load(std::memory_order_acquire))
	{
		std::cerr << "BlueZ_Interface: Already initialized" << std::endl;
		return false;
	}

	try
	{
		if (!init_dbus_connection())
		{
			std::cerr << "BlueZ_Interface: Failed to initialize D-Bus connection" << std::endl;
			return false;
		}

		if (!find_bluetooth_adapter())
		{
			std::cerr << "BlueZ_Interface: No Bluetooth adapter found" << std::endl;
			return false;
		}

		setup_adapter_properties();
		create_gatt_application();
		create_advertisement_object();
		create_agent_object();

		if (!register_agent())
		{
			std::cerr << "BlueZ_Interface: Failed to register agent" << std::endl;
			return false;
		}

		if (!register_gatt_application())
		{
			std::cerr << "BlueZ_Interface: Failed to register GATT application" << std::endl;
			return false;
		}

		is_running.store(true, std::memory_order_release);
		active_instance = this;

		// Start the event loop in a separate thread
		event_loop_thread = std::thread(&BlueZ_Interface::event_loop_process, this);

		std::cout << "BlueZ_Interface: Initialized successfully" << std::endl;
		return true;
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "BlueZ_Interface: D-Bus error during initialization: " 
				  << e.getName() << " - " << e.getMessage() << std::endl;
		return false;
	}
	catch (const std::exception& e)
	{
		std::cerr << "BlueZ_Interface: Exception during initialization: " << e.what() << std::endl;
		return false;
	}
}

bool BlueZ_Interface::start_advertising()
{
	if (!is_running.load(std::memory_order_acquire))
	{
		std::cerr << "BlueZ_Interface: Not initialized" << std::endl;
		return false;
	}

	if (current_state.load(std::memory_order_acquire) == State::ADVERTISING)
	{
		return true; // Already advertising
	}

	try
	{
		if (!register_advertisement())
		{
			std::cerr << "BlueZ_Interface: Failed to register advertisement" << std::endl;
			return false;
		}

		current_state.store(State::ADVERTISING, std::memory_order_release);
		if (state_change_callback)
		{
			state_change_callback(State::ADVERTISING);
		}

		std::cout << "BlueZ_Interface: Started advertising as '" << device_name << "'" << std::endl;
		return true;
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "BlueZ_Interface: Failed to start advertising: " 
				  << e.getName() << " - " << e.getMessage() << std::endl;
		return false;
	}
}

bool BlueZ_Interface::stop_advertising()
{
	if (!is_running.load(std::memory_order_acquire))
	{
		return true;
	}

	if (current_state.load(std::memory_order_acquire) != State::ADVERTISING)
	{
		return true;
	}

	try
	{
		advertising_manager_proxy->callMethod("UnregisterAdvertisement")
			.onInterface(BlueZ::LE_ADVERTISING_MANAGER_INTERFACE)
			.withArguments(sdbus::ObjectPath("/io/unikey/ble/advertisement"));

		current_state.store(State::DISCONNECTED, std::memory_order_release);
		if (state_change_callback)
		{
			state_change_callback(State::DISCONNECTED);
		}

		std::cout << "BlueZ_Interface: Stopped advertising" << std::endl;
		return true;
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "BlueZ_Interface: Failed to stop advertising: " 
				  << e.getName() << " - " << e.getMessage() << std::endl;
		return false;
	}
}

void BlueZ_Interface::shutdown()
{
	if (!is_running.load(std::memory_order_acquire))
	{
		return;
	}

	is_running.store(false, std::memory_order_release);

	try
	{
		stop_advertising();

		if (gatt_manager_proxy)
		{
			gatt_manager_proxy->callMethod("UnregisterApplication")
				.onInterface(BlueZ::GATT_MANAGER_INTERFACE)
				.withArguments(sdbus::ObjectPath("/io/unikey/ble"));
		}

		if (agent_manager_proxy)
		{
			agent_manager_proxy->callMethod("UnregisterAgent")
				.onInterface(BlueZ::AGENT_MANAGER_INTERFACE)
				.withArguments(sdbus::ObjectPath("/io/unikey/ble/agent"));
		}
	}
	catch (const sdbus::Error& e)
	{
		// Ignore errors during shutdown
	}

	if (dbus_connection)
	{
		dbus_connection->leaveEventLoop();
	}

	if (event_loop_thread.joinable())
	{
		event_loop_thread.join();
	}

	if (active_instance == this)
	{
		active_instance = nullptr;
	}

	current_state.store(State::DISCONNECTED, std::memory_order_release);
	std::cout << "BlueZ_Interface: Shutdown complete" << std::endl;
}

// ============================================================================
// Public Interface - State Queries
// ============================================================================

BlueZ_Interface::State BlueZ_Interface::get_state() const
{
	return current_state.load(std::memory_order_acquire);
}

bool BlueZ_Interface::is_connected() const
{
	return current_state.load(std::memory_order_acquire) == State::CONNECTED;
}

bool BlueZ_Interface::is_advertising() const
{
	return current_state.load(std::memory_order_acquire) == State::ADVERTISING;
}

std::string BlueZ_Interface::get_connected_device() const
{
	return connected_device_path;
}

// ============================================================================
// Public Interface - Configuration
// ============================================================================

void BlueZ_Interface::set_device_name(const std::string& name)
{
	device_name = name;

	if (is_running.load(std::memory_order_acquire) && adapter_proxy)
	{
		try
		{
			adapter_proxy->callMethod("Set")
				.onInterface(BlueZ::PROPERTIES_INTERFACE)
				.withArguments(std::string(BlueZ::ADAPTER_INTERFACE), 
							  std::string("Alias"),
							  sdbus::Variant(name));
		}
		catch (const sdbus::Error& e)
		{
			std::cerr << "BlueZ_Interface: Failed to update device name: " << e.getMessage() << std::endl;
		}
	}
}

std::string BlueZ_Interface::get_device_name() const
{
	return device_name;
}

void BlueZ_Interface::set_battery_level(uint8_t level)
{
	battery_level.store(std::min(level, uint8_t(100)), std::memory_order_release);
	// TODO: Send notification if notifications are enabled
}

void BlueZ_Interface::set_state_change_callback(std::function<void(State)> callback)
{
	state_change_callback = std::move(callback);
}

void BlueZ_Interface::enable_key_codes(const BitField& key_codes)
{
	enabled_key_codes = key_codes;
}

// ============================================================================
// Public Interface - HID Report Transmission
// ============================================================================

void BlueZ_Interface::send_keyboard_report(const ble_keyboard_report_t& report)
{
	if (!is_connected())
	{
		return;
	}

	std::lock_guard<std::mutex> lock(report_mutex);
	keyboard_state = report;

	// Build report data with report ID
	std::vector<uint8_t> report_data;
	report_data.reserve(9);
	report_data.push_back(HID_REPORT::KEYBOARD);
	report_data.push_back(report.modifiers);
	report_data.push_back(report.reserved);
	for (int i = 0; i < 6; ++i)
	{
		report_data.push_back(report.keycodes[i]);
	}

	// TODO: Send via notification when characteristic supports it
	// For now, the report is stored and will be read when the host requests it
}

void BlueZ_Interface::send_mouse_report(const ble_mouse_report_t& report)
{
	if (!is_connected())
	{
		return;
	}

	std::lock_guard<std::mutex> lock(report_mutex);
	mouse_state = report;

	// Build report data with report ID
	std::vector<uint8_t> report_data;
	report_data.reserve(5);
	report_data.push_back(HID_REPORT::MOUSE);
	report_data.push_back(report.buttons);
	report_data.push_back(static_cast<uint8_t>(report.x));
	report_data.push_back(static_cast<uint8_t>(report.y));
	report_data.push_back(static_cast<uint8_t>(report.wheel));

	// TODO: Send via notification when characteristic supports it
}

void BlueZ_Interface::send_input_event(const struct input_event& ev)
{
	if (!is_connected())
	{
		return;
	}

	std::lock_guard<std::mutex> lock(report_mutex);

	if (ev.type == EV_KEY)
	{
		uint16_t code = ev.code;
		int32_t value = ev.value;

		// Check for mouse buttons
		if (code >= BTN_LEFT && code <= BTN_TASK)
		{
			uint8_t button_bit = 0;
			switch (code)
			{
				case BTN_LEFT:   button_bit = 0x01; break;
				case BTN_RIGHT:  button_bit = 0x02; break;
				case BTN_MIDDLE: button_bit = 0x04; break;
				case BTN_SIDE:   button_bit = 0x08; break;
				case BTN_EXTRA:  button_bit = 0x10; break;
				default: return;
			}

			if (value) // Press
				mouse_state.buttons |= button_bit;
			else // Release
				mouse_state.buttons &= ~button_bit;

			send_mouse_report(mouse_state);
			return;
		}

		// Handle keyboard keys
		if (is_modifier_key(code))
		{
			uint8_t modifier_bit = linux_key_to_hid_modifier(code);
			if (value) // Press
				keyboard_state.modifiers |= modifier_bit;
			else // Release
				keyboard_state.modifiers &= ~modifier_bit;
		}
		else
		{
			uint8_t hid_code = linux_key_to_hid_usage(code);
			if (hid_code != 0)
			{
				if (value) // Press - add to keycodes array
				{
					for (int i = 0; i < 6; ++i)
					{
						if (keyboard_state.keycodes[i] == 0)
						{
							keyboard_state.keycodes[i] = hid_code;
							break;
						}
					}
				}
				else // Release - remove from keycodes array
				{
					for (int i = 0; i < 6; ++i)
					{
						if (keyboard_state.keycodes[i] == hid_code)
						{
							keyboard_state.keycodes[i] = 0;
							// Shift remaining keys
							for (int j = i; j < 5; ++j)
							{
								keyboard_state.keycodes[j] = keyboard_state.keycodes[j + 1];
							}
							keyboard_state.keycodes[5] = 0;
							break;
						}
					}
				}
			}
		}
		send_keyboard_report(keyboard_state);
	}
	else if (ev.type == EV_REL)
	{
		switch (ev.code)
		{
			case REL_X:
				mouse_state.x = static_cast<int8_t>(std::clamp(ev.value, -127, 127));
				break;
			case REL_Y:
				mouse_state.y = static_cast<int8_t>(std::clamp(ev.value, -127, 127));
				break;
			case REL_WHEEL:
			case REL_WHEEL_HI_RES:
				mouse_state.wheel = static_cast<int8_t>(std::clamp(ev.value, -127, 127));
				break;
			default:
				return;
		}
		send_mouse_report(mouse_state);

		// Reset relative values after sending
		mouse_state.x = 0;
		mouse_state.y = 0;
		mouse_state.wheel = 0;
	}
}

void BlueZ_Interface::send_input_events(const struct input_event* events, uint64_t count)
{
	for (uint64_t i = 0; i < count; ++i)
	{
		if (events[i].type != EV_SYN)
		{
			send_input_event(events[i]);
		}
	}
}

// ============================================================================
// Public Interface - Pairing
// ============================================================================

bool BlueZ_Interface::remove_paired_device(const std::string& device_address)
{
	// TODO: Implement device removal via BlueZ D-Bus API
	return false;
}

std::vector<std::string> BlueZ_Interface::get_paired_devices() const
{
	std::vector<std::string> devices;
	// TODO: Query paired devices via BlueZ D-Bus API
	return devices;
}

// ============================================================================
// Static Event Processor
// ============================================================================

void BlueZ_Interface::bluetooth_event_processor(const void* data, uint64_t unit_size)
{
	if (active_instance == nullptr || data == nullptr)
	{
		return;
	}

	const uint64_t* p_count = static_cast<const uint64_t*>(data);
	const struct input_event* events = reinterpret_cast<const struct input_event*>(p_count + 1);

	active_instance->send_input_events(events, *p_count);
}

BlueZ_Interface* BlueZ_Interface::get_active_instance()
{
	return active_instance;
}

// ============================================================================
// Private Helper Methods - D-Bus Initialization
// ============================================================================

bool BlueZ_Interface::init_dbus_connection()
{
	try
	{
		dbus_connection = sdbus::createSystemBusConnection("io.unikey.ble");
		return true;
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "BlueZ_Interface: Failed to create D-Bus connection: " << e.getMessage() << std::endl;
		return false;
	}
}

bool BlueZ_Interface::find_bluetooth_adapter()
{
	try
	{
		auto object_manager = sdbus::createProxy(*dbus_connection, BlueZ::SERVICE, "/");
		
		std::map<sdbus::ObjectPath, std::map<std::string, std::map<std::string, sdbus::Variant>>> managed_objects;
		object_manager->callMethod("GetManagedObjects")
			.onInterface(BlueZ::OBJECT_MANAGER_INTERFACE)
			.storeResultsTo(managed_objects);

		for (const auto& [path, interfaces] : managed_objects)
		{
			if (interfaces.find(BlueZ::ADAPTER_INTERFACE) != interfaces.end())
			{
				adapter_path = path;
				adapter_proxy = sdbus::createProxy(*dbus_connection, BlueZ::SERVICE, path);
				gatt_manager_proxy = sdbus::createProxy(*dbus_connection, BlueZ::SERVICE, path);
				advertising_manager_proxy = sdbus::createProxy(*dbus_connection, BlueZ::SERVICE, path);
				agent_manager_proxy = sdbus::createProxy(*dbus_connection, BlueZ::SERVICE, "/org/bluez");
				
				std::cout << "BlueZ_Interface: Found adapter at " << adapter_path << std::endl;
				return true;
			}
		}
		return false;
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "BlueZ_Interface: Error finding adapter: " << e.getMessage() << std::endl;
		return false;
	}
}

void BlueZ_Interface::setup_adapter_properties()
{
	try
	{
		// Power on the adapter
		adapter_proxy->callMethod("Set")
			.onInterface(BlueZ::PROPERTIES_INTERFACE)
			.withArguments(std::string(BlueZ::ADAPTER_INTERFACE),
						  std::string("Powered"),
						  sdbus::Variant(true));

		// Set discoverable
		adapter_proxy->callMethod("Set")
			.onInterface(BlueZ::PROPERTIES_INTERFACE)
			.withArguments(std::string(BlueZ::ADAPTER_INTERFACE),
						  std::string("Discoverable"),
						  sdbus::Variant(true));

		// Set pairable
		adapter_proxy->callMethod("Set")
			.onInterface(BlueZ::PROPERTIES_INTERFACE)
			.withArguments(std::string(BlueZ::ADAPTER_INTERFACE),
						  std::string("Pairable"),
						  sdbus::Variant(true));

		// Set alias
		adapter_proxy->callMethod("Set")
			.onInterface(BlueZ::PROPERTIES_INTERFACE)
			.withArguments(std::string(BlueZ::ADAPTER_INTERFACE),
						  std::string("Alias"),
						  sdbus::Variant(device_name));

		std::cout << "BlueZ_Interface: Adapter configured" << std::endl;
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "BlueZ_Interface: Warning - could not configure adapter: " << e.getMessage() << std::endl;
	}
}

// ============================================================================
// Private Helper Methods - GATT Service Creation
// ============================================================================

void BlueZ_Interface::create_gatt_application()
{
	// Create GATT Application object
	gatt_application = sdbus::createObject(*dbus_connection, "/io/unikey/ble");
	gatt_application->addObjectManager();

	create_hid_service();
	create_battery_service();
	create_device_info_service();

	gatt_application->finishRegistration();
}

void BlueZ_Interface::create_hid_service()
{
	// HID Service
	hid_service = sdbus::createObject(*dbus_connection, "/io/unikey/ble/hid");
	
	hid_service->registerProperty("UUID")
		.onInterface(BlueZ::GATT_SERVICE_INTERFACE)
		.withGetter([](){ return std::string(BLE_UUID::HID_SERVICE); });
	
	hid_service->registerProperty("Primary")
		.onInterface(BlueZ::GATT_SERVICE_INTERFACE)
		.withGetter([](){ return true; });

	hid_service->finishRegistration();

	// Report Map Characteristic
	report_map_char = sdbus::createObject(*dbus_connection, "/io/unikey/ble/hid/report_map");
	
	report_map_char->registerProperty("UUID")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withGetter([](){ return std::string(BLE_UUID::HID_REPORT_MAP); });
	
	report_map_char->registerProperty("Service")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withGetter([](){ return sdbus::ObjectPath("/io/unikey/ble/hid"); });
	
	report_map_char->registerProperty("Flags")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withGetter([](){ return std::vector<std::string>{"read"}; });

	report_map_char->registerMethod("ReadValue")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withInputParamNames("options")
		.withOutputParamNames("value")
		.implementedAs([this](const std::map<std::string, sdbus::Variant>&) {
			return this->read_report_map();
		});

	report_map_char->finishRegistration();

	// HID Information Characteristic
	hid_info_char = sdbus::createObject(*dbus_connection, "/io/unikey/ble/hid/hid_info");
	
	hid_info_char->registerProperty("UUID")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withGetter([](){ return std::string(BLE_UUID::HID_INFORMATION); });
	
	hid_info_char->registerProperty("Service")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withGetter([](){ return sdbus::ObjectPath("/io/unikey/ble/hid"); });
	
	hid_info_char->registerProperty("Flags")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withGetter([](){ return std::vector<std::string>{"read"}; });

	hid_info_char->registerMethod("ReadValue")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withInputParamNames("options")
		.withOutputParamNames("value")
		.implementedAs([this](const std::map<std::string, sdbus::Variant>&) {
			return this->read_hid_info();
		});

	hid_info_char->finishRegistration();

	// HID Control Point Characteristic
	hid_control_point_char = sdbus::createObject(*dbus_connection, "/io/unikey/ble/hid/control_point");
	
	hid_control_point_char->registerProperty("UUID")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withGetter([](){ return std::string(BLE_UUID::HID_CONTROL_POINT); });
	
	hid_control_point_char->registerProperty("Service")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withGetter([](){ return sdbus::ObjectPath("/io/unikey/ble/hid"); });
	
	hid_control_point_char->registerProperty("Flags")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withGetter([](){ return std::vector<std::string>{"write-without-response"}; });

	hid_control_point_char->registerMethod("WriteValue")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withInputParamNames("value", "options")
		.implementedAs([this](const std::vector<uint8_t>& value, const std::map<std::string, sdbus::Variant>&) {
			this->write_hid_control_point(value);
		});

	hid_control_point_char->finishRegistration();

	// Keyboard Report Characteristic
	keyboard_report_char = sdbus::createObject(*dbus_connection, "/io/unikey/ble/hid/keyboard_report");
	
	keyboard_report_char->registerProperty("UUID")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withGetter([](){ return std::string(BLE_UUID::HID_REPORT); });
	
	keyboard_report_char->registerProperty("Service")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withGetter([](){ return sdbus::ObjectPath("/io/unikey/ble/hid"); });
	
	keyboard_report_char->registerProperty("Flags")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withGetter([](){ return std::vector<std::string>{"read", "notify"}; });

	keyboard_report_char->registerMethod("ReadValue")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withInputParamNames("options")
		.withOutputParamNames("value")
		.implementedAs([this](const std::map<std::string, sdbus::Variant>&) {
			return this->read_keyboard_report();
		});

	keyboard_report_char->registerMethod("StartNotify")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.implementedAs([this]() { this->start_keyboard_notify(); });

	keyboard_report_char->registerMethod("StopNotify")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.implementedAs([this]() { this->stop_keyboard_notify(); });

	keyboard_report_char->finishRegistration();

	// Mouse Report Characteristic
	mouse_report_char = sdbus::createObject(*dbus_connection, "/io/unikey/ble/hid/mouse_report");
	
	mouse_report_char->registerProperty("UUID")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withGetter([](){ return std::string(BLE_UUID::HID_REPORT); });
	
	mouse_report_char->registerProperty("Service")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withGetter([](){ return sdbus::ObjectPath("/io/unikey/ble/hid"); });
	
	mouse_report_char->registerProperty("Flags")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withGetter([](){ return std::vector<std::string>{"read", "notify"}; });

	mouse_report_char->registerMethod("ReadValue")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withInputParamNames("options")
		.withOutputParamNames("value")
		.implementedAs([this](const std::map<std::string, sdbus::Variant>&) {
			return this->read_mouse_report();
		});

	mouse_report_char->registerMethod("StartNotify")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.implementedAs([this]() { this->start_mouse_notify(); });

	mouse_report_char->registerMethod("StopNotify")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.implementedAs([this]() { this->stop_mouse_notify(); });

	mouse_report_char->finishRegistration();
}

void BlueZ_Interface::create_battery_service()
{
	battery_service = sdbus::createObject(*dbus_connection, "/io/unikey/ble/battery");
	
	battery_service->registerProperty("UUID")
		.onInterface(BlueZ::GATT_SERVICE_INTERFACE)
		.withGetter([](){ return std::string(BLE_UUID::BATTERY_SERVICE); });
	
	battery_service->registerProperty("Primary")
		.onInterface(BlueZ::GATT_SERVICE_INTERFACE)
		.withGetter([](){ return true; });

	battery_service->finishRegistration();

	// Battery Level Characteristic
	battery_level_char = sdbus::createObject(*dbus_connection, "/io/unikey/ble/battery/level");
	
	battery_level_char->registerProperty("UUID")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withGetter([](){ return std::string(BLE_UUID::BATTERY_LEVEL); });
	
	battery_level_char->registerProperty("Service")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withGetter([](){ return sdbus::ObjectPath("/io/unikey/ble/battery"); });
	
	battery_level_char->registerProperty("Flags")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withGetter([](){ return std::vector<std::string>{"read", "notify"}; });

	battery_level_char->registerMethod("ReadValue")
		.onInterface(BlueZ::GATT_CHARACTERISTIC_INTERFACE)
		.withInputParamNames("options")
		.withOutputParamNames("value")
		.implementedAs([this](const std::map<std::string, sdbus::Variant>&) {
			return this->read_battery_level();
		});

	battery_level_char->finishRegistration();
}

void BlueZ_Interface::create_device_info_service()
{
	device_info_service = sdbus::createObject(*dbus_connection, "/io/unikey/ble/device_info");
	
	device_info_service->registerProperty("UUID")
		.onInterface(BlueZ::GATT_SERVICE_INTERFACE)
		.withGetter([](){ return std::string(BLE_UUID::DEVICE_INFO_SERVICE); });
	
	device_info_service->registerProperty("Primary")
		.onInterface(BlueZ::GATT_SERVICE_INTERFACE)
		.withGetter([](){ return true; });

	device_info_service->finishRegistration();
}

// ============================================================================
// Private Helper Methods - Advertisement
// ============================================================================

void BlueZ_Interface::create_advertisement_object()
{
	advertisement = sdbus::createObject(*dbus_connection, "/io/unikey/ble/advertisement");

	advertisement->registerProperty("Type")
		.onInterface(BlueZ::LE_ADVERTISEMENT_INTERFACE)
		.withGetter([](){ return std::string("peripheral"); });

	advertisement->registerProperty("LocalName")
		.onInterface(BlueZ::LE_ADVERTISEMENT_INTERFACE)
		.withGetter([this](){ return this->device_name; });

	advertisement->registerProperty("ServiceUUIDs")
		.onInterface(BlueZ::LE_ADVERTISEMENT_INTERFACE)
		.withGetter([](){
			return std::vector<std::string>{
				BLE_UUID::HID_SERVICE,
				BLE_UUID::BATTERY_SERVICE,
				BLE_UUID::DEVICE_INFO_SERVICE
			};
		});

	advertisement->registerProperty("Appearance")
		.onInterface(BlueZ::LE_ADVERTISEMENT_INTERFACE)
		.withGetter([](){ return uint16_t(0x03C1); }); // Keyboard appearance

	advertisement->registerProperty("Discoverable")
		.onInterface(BlueZ::LE_ADVERTISEMENT_INTERFACE)
		.withGetter([](){ return true; });

	advertisement->registerMethod("Release")
		.onInterface(BlueZ::LE_ADVERTISEMENT_INTERFACE)
		.implementedAs([this](){ this->handle_advertisement_release(); });

	advertisement->finishRegistration();
}

bool BlueZ_Interface::register_advertisement()
{
	try
	{
		std::map<std::string, sdbus::Variant> options;
		advertising_manager_proxy->callMethod("RegisterAdvertisement")
			.onInterface(BlueZ::LE_ADVERTISING_MANAGER_INTERFACE)
			.withArguments(sdbus::ObjectPath("/io/unikey/ble/advertisement"), options);
		return true;
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "BlueZ_Interface: Failed to register advertisement: " << e.getMessage() << std::endl;
		return false;
	}
}

void BlueZ_Interface::handle_advertisement_release()
{
	std::cout << "BlueZ_Interface: Advertisement released by BlueZ" << std::endl;
	if (current_state.load(std::memory_order_acquire) == State::ADVERTISING)
	{
		current_state.store(State::DISCONNECTED, std::memory_order_release);
		if (state_change_callback)
		{
			state_change_callback(State::DISCONNECTED);
		}
	}
}

// ============================================================================
// Private Helper Methods - Agent (Pairing)
// ============================================================================

void BlueZ_Interface::create_agent_object()
{
	agent = sdbus::createObject(*dbus_connection, "/io/unikey/ble/agent");

	agent->registerMethod("Release")
		.onInterface(BlueZ::AGENT_INTERFACE)
		.implementedAs([this](){ this->handle_agent_release(); });

	// FIX: Use std::placeholders::_1 to indicate the call parameter will be provided at invocation time
	// The previous code was binding a local stack variable which caused segfault
	agent->registerMethod(BlueZ::AGENT_INTERFACE, "RequestPinCode", "o", "s", 
		std::bind(&BlueZ_Interface::handle_request_pin_code, this, std::placeholders::_1));
	
	agent->registerMethod(BlueZ::AGENT_INTERFACE, "DisplayPinCode", "os", "", 
		std::bind(&BlueZ_Interface::handle_display_pin_code, this, std::placeholders::_1));
	
	agent->registerMethod(BlueZ::AGENT_INTERFACE, "RequestPasskey", "o", "u", 
		std::bind(&BlueZ_Interface::handle_request_passkey, this, std::placeholders::_1));
	
	agent->registerMethod(BlueZ::AGENT_INTERFACE, "DisplayPasskey", "oqu", "", 
		std::bind(&BlueZ_Interface::handle_display_passkey, this, std::placeholders::_1));
	
	agent->registerMethod(BlueZ::AGENT_INTERFACE, "RequestConfirmation", "ou", "", 
		std::bind(&BlueZ_Interface::handle_request_confirmation, this, std::placeholders::_1));
	
	agent->registerMethod(BlueZ::AGENT_INTERFACE, "RequestAuthorization", "o", "", 
		std::bind(&BlueZ_Interface::handle_request_authorization, this, std::placeholders::_1));
	
	agent->registerMethod(BlueZ::AGENT_INTERFACE, "AuthorizeService", "os", "", 
		std::bind(&BlueZ_Interface::handle_authorize_service, this, std::placeholders::_1));

	agent->registerMethod("Cancel")
		.onInterface(BlueZ::AGENT_INTERFACE)
		.implementedAs([this](){ this->handle_cancel(); });

	agent->finishRegistration();
}

bool BlueZ_Interface::register_agent()
{
	try
	{
		agent_manager_proxy->callMethod("RegisterAgent")
			.onInterface(BlueZ::AGENT_MANAGER_INTERFACE)
			.withArguments(sdbus::ObjectPath("/io/unikey/ble/agent"), std::string("NoInputNoOutput"));

		agent_manager_proxy->callMethod("RequestDefaultAgent")
			.onInterface(BlueZ::AGENT_MANAGER_INTERFACE)
			.withArguments(sdbus::ObjectPath("/io/unikey/ble/agent"));

		return true;
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "BlueZ_Interface: Failed to register agent: " << e.getMessage() << std::endl;
		return false;
	}
}

bool BlueZ_Interface::register_gatt_application()
{
	try
	{
		std::map<std::string, sdbus::Variant> options;
		gatt_manager_proxy->callMethod("RegisterApplication")
			.onInterface(BlueZ::GATT_MANAGER_INTERFACE)
			.withArguments(sdbus::ObjectPath("/io/unikey/ble"), options);
		return true;
	}
	catch (const sdbus::Error& e)
	{
		std::cerr << "BlueZ_Interface: Failed to register GATT application: " << e.getMessage() << std::endl;
		return false;
	}
}

void BlueZ_Interface::handle_agent_release()
{
	std::cout << "BlueZ_Interface: Agent released" << std::endl;
}

void BlueZ_Interface::handle_request_pin_code(sdbus::MethodCall call)
{
	// Auto-accept with default PIN for NoInputNoOutput capability
	auto reply = call.createReply();
	reply << std::string("0000");
	reply.send();
}

void BlueZ_Interface::handle_display_pin_code(sdbus::MethodCall call)
{
	sdbus::ObjectPath device;
	std::string pin;
	call >> device >> pin;
	std::cout << "BlueZ_Interface: PIN Code for " << device << ": " << pin << std::endl;
	call.createReply().send();
}

void BlueZ_Interface::handle_request_passkey(sdbus::MethodCall call)
{
	auto reply = call.createReply();
	reply << uint32_t(0);
	reply.send();
}

void BlueZ_Interface::handle_display_passkey(sdbus::MethodCall call)
{
	sdbus::ObjectPath device;
	uint32_t passkey;
	uint16_t entered;
	call >> device >> passkey >> entered;
	std::cout << "BlueZ_Interface: Passkey for " << device << ": " << passkey << std::endl;
	call.createReply().send();
}

void BlueZ_Interface::handle_request_confirmation(sdbus::MethodCall call)
{
	// Auto-confirm for NoInputNoOutput capability
	call.createReply().send();
}

void BlueZ_Interface::handle_request_authorization(sdbus::MethodCall call)
{
	// Auto-authorize for NoInputNoOutput capability
	call.createReply().send();
}

void BlueZ_Interface::handle_authorize_service(sdbus::MethodCall call)
{
	// Auto-authorize service
	call.createReply().send();
}

void BlueZ_Interface::handle_cancel()
{
	std::cout << "BlueZ_Interface: Pairing cancelled" << std::endl;
}

// ============================================================================
// Private Helper Methods - Event Loop
// ============================================================================

void BlueZ_Interface::event_loop_process()
{
	dbus_connection->enterEventLoop();
}

void BlueZ_Interface::handle_device_connected(const std::string& device_path)
{
	connected_device_path = device_path;
	current_state.store(State::CONNECTED, std::memory_order_release);
	
	if (state_change_callback)
	{
		state_change_callback(State::CONNECTED);
	}

	std::cout << "BlueZ_Interface: Device connected: " << device_path << std::endl;
}

void BlueZ_Interface::handle_device_disconnected()
{
	connected_device_path.clear();
	
	// Clear HID states
	{
		std::lock_guard<std::mutex> lock(report_mutex);
		keyboard_state = ble_keyboard_report_t{};
		mouse_state = ble_mouse_report_t{};
	}

	current_state.store(State::DISCONNECTED, std::memory_order_release);
	
	if (state_change_callback)
	{
		state_change_callback(State::DISCONNECTED);
	}

	std::cout << "BlueZ_Interface: Device disconnected" << std::endl;
}

// ============================================================================
// Private Helper Methods - GATT Read/Write Handlers
// ============================================================================

std::vector<uint8_t> BlueZ_Interface::read_keyboard_report()
{
	std::lock_guard<std::mutex> lock(report_mutex);
	std::vector<uint8_t> report;
	report.reserve(8);
	report.push_back(keyboard_state.modifiers);
	report.push_back(keyboard_state.reserved);
	for (int i = 0; i < 6; ++i)
	{
		report.push_back(keyboard_state.keycodes[i]);
	}
	return report;
}

std::vector<uint8_t> BlueZ_Interface::read_mouse_report()
{
	std::lock_guard<std::mutex> lock(report_mutex);
	std::vector<uint8_t> report;
	report.reserve(4);
	report.push_back(mouse_state.buttons);
	report.push_back(static_cast<uint8_t>(mouse_state.x));
	report.push_back(static_cast<uint8_t>(mouse_state.y));
	report.push_back(static_cast<uint8_t>(mouse_state.wheel));
	return report;
}

std::vector<uint8_t> BlueZ_Interface::read_report_map()
{
	return HID_REPORT_MAP_DATA;
}

std::vector<uint8_t> BlueZ_Interface::read_hid_info()
{
	// HID Information: bcdHID (1.11), bCountryCode (0), Flags (normally connectable)
	return { 0x11, 0x01, 0x00, 0x02 };
}

std::vector<uint8_t> BlueZ_Interface::read_battery_level()
{
	return { battery_level.load(std::memory_order_acquire) };
}

void BlueZ_Interface::write_hid_control_point(const std::vector<uint8_t>& value)
{
	if (!value.empty())
	{
		if (value[0] == 0) // Suspend
		{
			std::cout << "BlueZ_Interface: HID suspend requested" << std::endl;
		}
		else if (value[0] == 1) // Exit suspend
		{
			std::cout << "BlueZ_Interface: HID exit suspend requested" << std::endl;
		}
	}
}

void BlueZ_Interface::start_keyboard_notify()
{
	notifications_enabled.store(true, std::memory_order_release);
	std::cout << "BlueZ_Interface: Keyboard notifications enabled" << std::endl;
}

void BlueZ_Interface::stop_keyboard_notify()
{
	notifications_enabled.store(false, std::memory_order_release);
	std::cout << "BlueZ_Interface: Keyboard notifications disabled" << std::endl;
}

void BlueZ_Interface::start_mouse_notify()
{
	std::cout << "BlueZ_Interface: Mouse notifications enabled" << std::endl;
}

void BlueZ_Interface::stop_mouse_notify()
{
	std::cout << "BlueZ_Interface: Mouse notifications disabled" << std::endl;
}

// ============================================================================
// Static Helper Methods - Key Code Conversion
// ============================================================================

uint8_t BlueZ_Interface::linux_key_to_hid_usage(uint16_t linux_code)
{
	if (linux_code < sizeof(LINUX_TO_HID_KEYCODE))
	{
		return LINUX_TO_HID_KEYCODE[linux_code];
	}
	return 0x00;
}

uint8_t BlueZ_Interface::linux_key_to_hid_modifier(uint16_t linux_code)
{
	switch (linux_code)
	{
		case KEY_LEFTCTRL:   return 0x01;
		case KEY_LEFTSHIFT:  return 0x02;
		case KEY_LEFTALT:	return 0x04;
		case KEY_LEFTMETA:   return 0x08;
		case KEY_RIGHTCTRL:  return 0x10;
		case KEY_RIGHTSHIFT: return 0x20;
		case KEY_RIGHTALT:   return 0x40;
		case KEY_RIGHTMETA:  return 0x80;
		default:			 return 0x00;
	}
}

bool BlueZ_Interface::is_modifier_key(uint16_t linux_code)
{
	switch (linux_code)
	{
		case KEY_LEFTCTRL:
		case KEY_LEFTSHIFT:
		case KEY_LEFTALT:
		case KEY_LEFTMETA:
		case KEY_RIGHTCTRL:
		case KEY_RIGHTSHIFT:
		case KEY_RIGHTALT:
		case KEY_RIGHTMETA:
			return true;
		default:
			return false;
	}
}
