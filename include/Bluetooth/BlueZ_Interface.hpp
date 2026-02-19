#ifndef BLUEZ_INTERFACE_HPP
#define BLUEZ_INTERFACE_HPP

#include "Bluetooth/Gatt/Application.hpp"
#include "Bluetooth/Gatt/Base_App_Obj.hpp"
#include "Bluetooth/BlueZ_HID_Services.hpp"
#include "Bluetooth/org_bluez_proxy.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IObject.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/Types.h>

/*
	Combined proxy for the BlueZ adapter object, which exposes
	Adapter1, GattManager1, and LEAdvertisingManager1 on the same path.
*/
class BlueZ_Adapter_Proxy : public sdbus::ProxyInterfaces<
	org::bluez::Adapter1_proxy,
	org::bluez::GattManager1_proxy,
	org::bluez::LEAdvertisingManager1_proxy>
{
	public:
		BlueZ_Adapter_Proxy(sdbus::IConnection& connection, const std::string& path)
		: sdbus::ProxyInterfaces<
			org::bluez::Adapter1_proxy,
			org::bluez::GattManager1_proxy,
			org::bluez::LEAdvertisingManager1_proxy>(connection, "org.bluez", path)
		{
			this->registerProxy();
		}

		~BlueZ_Adapter_Proxy()
		{
			this->unregisterProxy();
		}
};

class BlueZ_Interface
{
	private:
		// D-Bus connection (non-owning, shared with the rest of the application)
		sdbus::IConnection* connection = nullptr;

		// Combined proxy for adapter/gatt/advertising
		std::unique_ptr<BlueZ_Adapter_Proxy> bluez_proxy;

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

	public:
		BlueZ_Interface() = default;
		BlueZ_Interface(const BlueZ_Interface&) = delete;

		~BlueZ_Interface();

		// Connection lifecycle
		bool enable(sdbus::IConnection& connection);
		void disable();
		bool connection_status() const;
		void wait_until_connected();

		// Data transmission
		void send_hid_report(uint8_t report_id, const void* data, uint64_t data_size) const;
		void send_hid_report(uint8_t report_id, const std::vector<uint8_t>& report) const;

		BlueZ_Interface& operator=(const BlueZ_Interface&) = delete;
};

#endif	// BLUEZ_INTERFACE_HPP