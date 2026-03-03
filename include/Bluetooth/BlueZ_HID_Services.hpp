#ifndef BLUEZ_HID_SERVICES_HPP
#define BLUEZ_HID_SERVICES_HPP

#include "Bluetooth/bluetooth_configs.hpp"
#include "Bluetooth/Gatt/Base_App_Obj.hpp"
#include "Bluetooth/Gatt/Characteristic.hpp"
#include "Bluetooth/Gatt/Descriptor.hpp"
#include "Bluetooth/Gatt/Service.hpp"

#include <iostream>
#include <string>

// Battery Level Service
class BatteryLevelChar : public Characteristic
{
	private:
		uint8_t battery_level = 100;

	public:
		BatteryLevelChar() : Characteristic("2a19", { "read", "notify" }) {}

		ByteArray on_read_value(OptionsMap) const override { return { this->battery_level }; }
		void on_start_notify() const override { std::cout << "Battery Notify Start" << std::endl; }
};

class BatteryService : public Service
{
	public:
		BatteryService() : Service("180f", true)
		{
			this->add_subelement(new BatteryLevelChar);
		}
};


// Device Information Service
class ReadChar : public Characteristic
{
	private:
		ByteArray values;
	
	public:
		ReadChar(const std::string& uuid, const std::string& value)
			: Characteristic(uuid, { "read" }), values(value.begin(), value.end()) {}
		
		ByteArray on_read_value(OptionsMap) const override
		{
			return this->values;
		}

		void on_write_value(ByteArray values, OptionsMap) override
		{
			this->values = values;
		}
};

class DeviceInfoService : public Service
{
	public:
		DeviceInfoService() : Service("180a", true)
		{
			this->add_subelement(new ReadChar("2a29", "Unikey"));
			this->add_subelement(new ReadChar("2a24", "HID"));
			this->add_subelement(new ReadChar("2a28", "1.0.0"));
		}
};


// HID Service
class HIDChar : public Characteristic
{
	private:
		ByteArray values;
	
	public:
		HIDChar(const std::string& uuid, const ByteArray& values, const std::vector<std::string>& flags)
		: Characteristic(uuid, flags)
		{
			this->values = values;
		}

		ByteArray on_read_value(OptionsMap) const override
		{
			return this->values;
		}

		void on_write_value(ByteArray values, OptionsMap) override
		{
			this->values = values;
		}
};

class RefDesc : public Descriptor
{
	private:
		ByteArray values;
	
	public:
		RefDesc(const ByteArray& values) : Descriptor("2908", { "read" })
		{
			this->values = values;
		}

		ByteArray on_read_value(OptionsMap) const override
		{
			return this->values;
		}
};

class ReportChar : public Characteristic
{
	private:
		ByteArray values = { 0, 0 };
	
	public:
		ReportChar(uint8_t id, bool has_output=false) : Characteristic("2a4d", { "secure-read", "notify" })
		{
			this->add_subelement(new RefDesc({ id, 0x01 }));	// Input
			if (has_output)
			{
				this->add_subelement(new RefDesc({ id, 0x02 }));	// Output
			}
		}

		ByteArray on_read_value(OptionsMap) const override
		{
			return this->values;
		}

		void on_write_value(ByteArray values, OptionsMap) override
		{
			this->values = values;
		}
};

class HIDService : public Service
{
	public:
		HIDService() : Service("1812", true)
		{
			this->add_subelement(new HIDChar("2a4e", ByteArray{ 1 }, { "read", "write-without-response" }));
			this->add_subelement(new HIDChar("2a4a", ByteArray{ 1, 1, 0, 2 }, { "read" }));
			this->add_subelement(new HIDChar("2a4b", corsair_hid_report_desc, { "read" }));	// This takes the HID report descriptor
			this->add_subelement(new ReportChar(1));
			this->add_subelement(new ReportChar(2, true));
			this->add_subelement(new ReportChar(3));
		}
};

#endif	// BLUEZ_HID_SERVICES_HPP