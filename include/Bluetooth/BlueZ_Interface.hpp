#ifndef BLUEZ_INTERFACE_HPP
#define BLUEZ_INTERFACE_HPP

#include "Bluetooth/Gatt/Application.hpp"
#include "Bluetooth/Gatt/Base_App_Obj.hpp"
#include "Bluetooth/BlueZ_HID_Services.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IObject.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Types.h>

class BlueZ_Interface
{
	private:
		// D-Bus connection (non-owning, shared with the rest of the application)
		sdbus::IConnection* connection = nullptr;
		std::unique_ptr<sdbus::IProxy> gatt_manager_proxy;
		std::unique_ptr<sdbus::IProxy> ad_manager_proxy;
		std::unique_ptr<sdbus::IProxy> adapter_proxy;

		// GATT Application tree
		Application* app = nullptr;
		HIDService* hid_service = nullptr;
		DeviceInfoService* dev_info_service = nullptr;
		BatteryService* battery_service = nullptr;

		// Advertisement D-Bus object
		std::unique_ptr<sdbus::IObject> ad_object;

		// State
		std::string adapter_path;
		std::string app_path;
		std::string ad_path;
		std::atomic_bool connected_to_host = false;
		std::atomic_bool advertising = false;
		std::atomic_bool registered = false;

		// Internal helpers
		std::string find_adapter() const;
		void create_advertisement();
		void register_advertisement();
		void unregister_advertisement();
		void register_gatt_application();
		void unregister_gatt_application();
		void monitor_connection();

		// Pointers to report characteristics for sending HID data
		ReportChar* report_char_1 = nullptr;
		ReportChar* report_char_2 = nullptr;

	public:
		BlueZ_Interface() = default;
		BlueZ_Interface(const BlueZ_Interface&) = delete;

		~BlueZ_Interface();

		// Connection lifecycle (mirrors WiFi_Client)
		bool enable(sdbus::IConnection& connection);
		void disable();
		bool connection_status() const;
		void wait_until_connected();

		// Data transmission (mirrors WiFi_Client::send_formatted_data)
		void send_hid_report(uint8_t report_id, const void* data, uint64_t data_size) const;
		void send_hid_report(uint8_t report_id, const std::vector<uint8_t>& report) const;

		BlueZ_Interface& operator=(const BlueZ_Interface&) = delete;
};

#endif	// BLUEZ_INTERFACE_HPP