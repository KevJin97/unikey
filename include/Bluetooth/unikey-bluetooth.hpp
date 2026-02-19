#ifndef UNIKEY_BLUETOOTH_HPP
#define UNIKEY_BLUETOOTH_HPP

#include <sdbus-c++/IObject.h>
#include <sdbus-c++/Message.h>
#include <stdint.h>

extern void dbus_enable_unikey_bluetooth(sdbus::MethodCall call);
extern void dbus_disable_unikey_bluetooth();

// class BlueZ_Proxy :
// 	public org::bluez::Adapter1_proxy,
// 	public org::bluez::GattManager1_proxy,
// 	public org::bluez::LEAdvertisingManager1_proxy
// {
// 	public:
// 		BlueZ_Proxy(sdbus::IProxy& proxy) :
// 			org::bluez::Adapter1_proxy(proxy),
// 			org::bluez::GattManager1_proxy(proxy),
// 			org::bluez::LEAdvertisingManager1_proxy(proxy)
// 		{}
// };
// TODO: DYNAMICALLY ALLOCATE/GENERATE REPORT BASED OFF SETTINGS

struct HID_Mouse_Report_Settings
{
	bool mouse_btns_4_5 = false;
	bool hi_dpi = false;
	bool hi_res_vert_wheel = false;
	bool has_horiz_wheel = false;
	bool hi_res_hori_wheel = false;

	HID_Mouse_Report_Settings& operator=(const HID_Mouse_Report_Settings& settings)
	{
		this->mouse_btns_4_5 = settings.mouse_btns_4_5;
		this->hi_dpi = settings.hi_dpi;
		this->hi_res_vert_wheel = settings.hi_res_vert_wheel;
		this->has_horiz_wheel = settings.has_horiz_wheel;
		this->hi_res_hori_wheel = settings.hi_res_hori_wheel;

		return *this;
	}
};

struct HID_Key_Report
{

};

class Linux_HID_Interface
{
	protected:


	public:
		uint8_t modifier_key_mask();
		uint8_t modifier_key_mask(uint16_t code);
		
		
};

#endif	// UNIKEY_BLUETOOTH_HPP