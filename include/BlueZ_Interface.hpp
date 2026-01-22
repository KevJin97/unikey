#ifndef BLUEZ_INTERFACE_HPP
#define BLUEZ_INTERFACE_HPP

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <sdbus-c++/sdbus-c++.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IObject.h>
#include <sdbus-c++/IProxy.h>

#include <linux/input.h>

#include "BitField.hpp"

// HID Report IDs
namespace HID_REPORT
{
	static constexpr uint8_t KEYBOARD = 0x01;
	static constexpr uint8_t MOUSE	= 0x02;
	static constexpr uint8_t CONSUMER = 0x03;
}

// BLE HID Report structures
struct ble_keyboard_report_t
{
	uint8_t modifiers = 0;	  // Modifier keys (Ctrl, Shift, Alt, GUI)
	uint8_t reserved = 0;	   // Reserved byte
	uint8_t keycodes[6] = {0};  // Up to 6 simultaneous key presses
};

struct ble_mouse_report_t
{
	uint8_t buttons = 0;		// Mouse buttons (left, right, middle)
	int8_t x = 0;			   // X movement (-127 to 127)
	int8_t y = 0;			   // Y movement (-127 to 127)
	int8_t wheel = 0;		   // Scroll wheel (-127 to 127)
};

class BlueZ_Interface
{
	public:
		// Connection state enumeration
		enum class State
		{
			DISCONNECTED,
			ADVERTISING,
			CONNECTING,
			CONNECTED,
			PAIRING,
			ERROR
		};

	private:
		// D-Bus connection and objects
		std::unique_ptr<sdbus::IConnection> dbus_connection;
		std::unique_ptr<sdbus::IObject> gatt_application;
		std::unique_ptr<sdbus::IObject> hid_service;
		std::unique_ptr<sdbus::IObject> keyboard_report_char;
		std::unique_ptr<sdbus::IObject> mouse_report_char;
		std::unique_ptr<sdbus::IObject> report_map_char;
		std::unique_ptr<sdbus::IObject> hid_info_char;
		std::unique_ptr<sdbus::IObject> hid_control_point_char;
		std::unique_ptr<sdbus::IObject> battery_service;
		std::unique_ptr<sdbus::IObject> battery_level_char;
		std::unique_ptr<sdbus::IObject> device_info_service;
		std::unique_ptr<sdbus::IObject> advertisement;
		std::unique_ptr<sdbus::IObject> agent;

		// D-Bus proxies for BlueZ interfaces
		std::unique_ptr<sdbus::IProxy> adapter_proxy;
		std::unique_ptr<sdbus::IProxy> gatt_manager_proxy;
		std::unique_ptr<sdbus::IProxy> advertising_manager_proxy;
		std::unique_ptr<sdbus::IProxy> agent_manager_proxy;

		// State management
		std::atomic<State> current_state{State::DISCONNECTED};
		std::atomic_bool is_running{false};
		std::atomic_bool notifications_enabled{false};
		std::atomic_uint8_t battery_level{100};

		// Device configuration
		std::string device_name;
		std::string adapter_path;
		std::string connected_device_path;
		
		// HID state tracking
		ble_keyboard_report_t keyboard_state;
		ble_mouse_report_t mouse_state;
		BitField enabled_key_codes{KEY_CNT};
		std::mutex report_mutex;

		// Event loop thread
		std::thread event_loop_thread;

		// Callback for connection state changes
		std::function<void(State)> state_change_callback;

		// Private helper methods
		bool init_dbus_connection();
		bool find_bluetooth_adapter();
		bool register_gatt_application();
		bool register_advertisement();
		bool register_agent();
		void create_hid_service();
		void create_battery_service();
		void create_device_info_service();
		void create_gatt_application();
		void create_advertisement_object();
		void create_agent_object();
		void setup_adapter_properties();
		void event_loop_process();
		void handle_device_connected(const std::string& device_path);
		void handle_device_disconnected();

		// D-Bus method handlers for GATT characteristics
		std::vector<uint8_t> read_keyboard_report();
		std::vector<uint8_t> read_mouse_report();
		std::vector<uint8_t> read_report_map();
		std::vector<uint8_t> read_hid_info();
		std::vector<uint8_t> read_battery_level();
		void write_hid_control_point(const std::vector<uint8_t>& value);
		void start_keyboard_notify();
		void stop_keyboard_notify();
		void start_mouse_notify();
		void stop_mouse_notify();

		// D-Bus method handlers for Advertisement
		void handle_advertisement_release();

		// D-Bus method handlers for Agent
		void handle_agent_release();
		void handle_request_pin_code(sdbus::MethodCall call);
		void handle_display_pin_code(sdbus::MethodCall call);
		void handle_request_passkey(sdbus::MethodCall call);
		void handle_display_passkey(sdbus::MethodCall call);
		void handle_request_confirmation(sdbus::MethodCall call);
		void handle_request_authorization(sdbus::MethodCall call);
		void handle_authorize_service(sdbus::MethodCall call);
		void handle_cancel();

		// Linux input code to HID usage code conversion
		static uint8_t linux_key_to_hid_usage(uint16_t linux_code);
		static uint8_t linux_key_to_hid_modifier(uint16_t linux_code);
		static bool is_modifier_key(uint16_t linux_code);

	public:
		// Constructors and destructor
		BlueZ_Interface();
		explicit BlueZ_Interface(const std::string& device_name);
		~BlueZ_Interface();

		// Delete copy constructor and assignment
		BlueZ_Interface(const BlueZ_Interface&) = delete;
		BlueZ_Interface& operator=(const BlueZ_Interface&) = delete;

		// Public interface - Lifecycle management
		bool initialize();
		bool start_advertising();
		bool stop_advertising();
		void shutdown();

		// Public interface - State queries
		State get_state() const;
		bool is_connected() const;
		bool is_advertising() const;
		std::string get_connected_device() const;

		// Public interface - Configuration
		void set_device_name(const std::string& name);
		std::string get_device_name() const;
		void set_battery_level(uint8_t level);
		void set_state_change_callback(std::function<void(State)> callback);
		void enable_key_codes(const BitField& key_codes);

		// Public interface - HID report transmission
		void send_keyboard_report(const ble_keyboard_report_t& report);
		void send_mouse_report(const ble_mouse_report_t& report);
		void send_input_event(const struct input_event& ev);
		void send_input_events(const struct input_event* events, uint64_t count);

		// Public interface - Pairing
		bool remove_paired_device(const std::string& device_address);
		std::vector<std::string> get_paired_devices() const;

		// Static event processor compatible with Device::set_event_processor
		static void bluetooth_event_processor(const void* data, uint64_t unit_size);
		static BlueZ_Interface* get_active_instance();

	private:
		static inline BlueZ_Interface* active_instance = nullptr;
};

#endif // BLUEZ_INTERFACE_HPP
