#ifndef BLUEZ_INTERFACE_HPP
#define BLUEZ_INTERFACE_HPP

#include "Bluetooth/Gatt/Application.hpp"
#include "Bluetooth/org_bluez_proxy.hpp"

#include <memory>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IObject.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/ProxyInterfaces.h>

class BlueZ_Interface;

class BlueZ_Adapter_Proxy : public 
	sdbus::ProxyInterfaces<
		org::bluez::Adapter1_proxy,
		org::bluez::GattManager1_proxy,
		org::bluez::LEAdvertisingManager1_proxy
	>
{
	friend BlueZ_Interface;

	public:
		BlueZ_Adapter_Proxy(sdbus::IConnection& connection, const std::string& path) : sdbus::ProxyInterfaces<
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
	// MEMBER DATA
		std::unique_ptr<sdbus::IConnection> new_connection;
		sdbus::IConnection* dbus_connection = nullptr;
		std::string device_name = "BlueZ Interface";
		std::string path_name;
		Application* gatt_app = nullptr;
		std::unique_ptr<BlueZ_Adapter_Proxy> bluez_proxy;
		bool orig_powered_state = false;
		bool orig_discover_state = false;
		std::unique_ptr<sdbus::IObject> ad_object;
		std::atomic_bool connected_to_host = false;
		std::atomic_bool advertising = false;
		std::atomic_bool registered = false;

	// PRIVATE INTERFACE
		std::vector<std::string> find_adapter() const;
		void create_advertisement();
		void register_advertisement();
		void register_gatt_application();
		void unregister_advertisement();
		void unregister_gatt_application();
		void monitor_connection();

	public:
	// PUBLIC CONSTRUCTOR(S)
		BlueZ_Interface(const BlueZ_Interface&) = delete;
		BlueZ_Interface(sdbus::IConnection* dbus_connection=nullptr, const std::string& connection_name="bluez.interface");
		explicit BlueZ_Interface(const std::unique_ptr<sdbus::IConnection>& dbus_connection, const std::string& connection_name="bluez.interface");
		BlueZ_Interface(const std::string& device_name, sdbus::IConnection* dbus_connection=nullptr, const std::string& connection_name="bluez.interface");
		explicit BlueZ_Interface(const std::string& device_name, const std::unique_ptr<sdbus::IConnection>& dbus_connection, const std::string& connection_name="bluez.interface");

	// DESTRUCTOR
		~BlueZ_Interface();

	// PUBLIC INTERFACE
		void set_path_name(const std::string& path_name);
		void set_device_name(const std::string& device_name);
		bool enable();
		bool connection_status() const;
		void wait_until_connected();
		void disable();
		void send_hid_report(uint8_t report_id, const void* data, uint64_t data_size) const;
		void send_hid_report(uint8_t report_id, const std::vector<uint8_t>& report) const;

	// OPERATOR OVERLOAD(S)
		BlueZ_Interface& operator=(const BlueZ_Interface&) = delete;
};

#endif	// BLUEZ_INTERFACE_HPP