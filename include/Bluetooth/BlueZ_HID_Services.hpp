#ifndef BLUEZ_HID_SERVICES_HPP
#define BLUEZ_HID_SERVICES_HPP

#include "Bluetooth/bluetooth_configs.hpp"
#include "Bluetooth/Gatt/Base_App_Obj.hpp"
#include "Bluetooth/Gatt/Characteristic.hpp"
#include "Bluetooth/Gatt/Descriptor.hpp"
#include "Bluetooth/Gatt/Service.hpp"

#include <iostream>
#include <string>


// ─── Shared Descriptors ──────────────────────────────────────────────────────
// Defined first so they can be used by all characteristic classes below.

// Report Reference Descriptor (UUID: 0x2908)
// Returns [ReportID, ReportType] where type 0x01=Input, 0x02=Output
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

// NOTE: CCCDesc (0x2902) is intentionally NOT defined or used here.
// When using the BlueZ GATT Application API, BlueZ automatically creates and
// manages the CCC handle for any characteristic that declares "notify" or
// "indicate" in its Flags property. If the application also exports a 0x2902
// descriptor object, a duplicate CCC appears in the ATT handle table.
// The host writes 0x0100 to the application-exported CCC (which BlueZ ignores
// for notification purposes) and BlueZ's internal CCC stays at 0x0000, causing
// emitPropertiesChangedSignal to silently not send any ATT notifications.


// ─── Battery Service (UUID: 0x180F) ──────────────────────────────────────────

// Battery Level Characteristic (UUID: 0x2A19)
// Flags: ["read", "notify"]
// No descriptors — BlueZ auto-creates the CCC.
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
			this->add_subelement(new BatteryLevelChar);	// char0
		}
};


// ─── Device Information Service (UUID: 0x180A) ───────────────────────────────

// Generic read-only string characteristic for device info fields
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

// PnP ID Characteristic (UUID: 0x2A50)
// Returns 7 bytes: [VendorIDSource(1), VendorID(2 LE), ProductID(2 LE), Version(2 LE)]
class PnPIDChar : public Characteristic
{
	private:
		ByteArray values;

	public:
		// vendor_id_source: 0x01=Bluetooth SIG, 0x02=USB IF
		PnPIDChar(uint8_t vendor_id_source, uint16_t vendor_id, uint16_t product_id, uint16_t version)
			: Characteristic("2a50", { "read" })
		{
			this->values = {
				vendor_id_source,
				(uint8_t)(vendor_id & 0xFF),  (uint8_t)(vendor_id >> 8),
				(uint8_t)(product_id & 0xFF), (uint8_t)(product_id >> 8),
				(uint8_t)(version & 0xFF),    (uint8_t)(version >> 8)
			};
		}

		ByteArray on_read_value(OptionsMap) const override
		{
			return this->values;
		}
};

class DeviceInfoService : public Service
{
	public:
		DeviceInfoService() : Service("180a", true)
		{
			this->add_subelement(new ReadChar("2a29", "Unikey"));				// char0: Manufacturer Name
			this->add_subelement(new ReadChar("2a24", "HID"));					// char1: Model Number
			this->add_subelement(new ReadChar("2a25", "000000"));				// char2: Serial Number
			this->add_subelement(new ReadChar("2a26", "1.0.0"));				// char3: Firmware Revision
			this->add_subelement(new PnPIDChar(0x01, 0x05AC, 0x0239, 0x0001));	// char4: PnP ID
		}
};


// ─── HID Service (UUID: 0x1812) ──────────────────────────────────────────────

// Generic HID characteristic with a fixed value and configurable flags.
// Used for Protocol Mode, HID Information, Report Map, and HID Control Point.
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

// Input Report Characteristic (UUID: 0x2A4D)
// Flags: ["read", "notify"]
// Descriptors: desc0 = Report Reference [id, 0x01=Input]
// NOTE: No CCCDesc here. BlueZ auto-creates the CCC for "notify" characteristics.
//       Adding CCCDesc manually would create a duplicate CCC, causing the host to
//       enable the wrong one and BlueZ to never send ATT notifications.
class InputReportChar : public Characteristic
{
	private:
		ByteArray values = { 0x00, 0x00 };
	
	public:
		InputReportChar(uint8_t report_id) : Characteristic("2a4d", { "read", "notify" })
		{
			this->add_subelement(new RefDesc({ report_id, 0x01 }));	// desc0: Report Reference (Input)
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

// Output Report Characteristic (UUID: 0x2A4D)
// Flags: ["read", "write", "write-without-response"]
// Descriptors: desc0 = Report Reference [id, 0x02=Output]
// Used to receive LED status (Num/Caps/Scroll Lock) from the host.
class OutputReportChar : public Characteristic
{
	private:
		ByteArray values = { 0x00 };
	
	public:
		OutputReportChar(uint8_t report_id) : Characteristic("2a4d", { "read", "write", "write-without-response" })
		{
			this->add_subelement(new RefDesc({ report_id, 0x02 }));	// desc0: Report Reference (Output)
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
			// char0: Protocol Mode (0x2A4E) — boot or report protocol
			this->add_subelement(new HIDChar("2a4e", ByteArray{ 0x01 }, { "read", "write-without-response" }));

			// char1: HID Information (0x2A4A) — [bcdHID=1.11, bCountryCode=0, Flags=NormallyConnectable]
			this->add_subelement(new HIDChar("2a4a", ByteArray{ 0x11, 0x01, 0x00, 0x02 }, { "read" }));

			// char2: Report Map (0x2A4B) — HID report descriptor bytes
			this->add_subelement(new HIDChar("2a4b", corsair_hid_report_desc, { "read" }));

			// char3: HID Control Point (0x2A4C) — 0x00=Suspend, 0x01=ExitSuspend
			this->add_subelement(new HIDChar("2a4c", ByteArray{}, { "write-without-response" }));

			// char4: Input Report (mouse, report ID 1)
			this->add_subelement(new InputReportChar(1));

			// char5: Output Report (keyboard LEDs, report ID 2)
			this->add_subelement(new OutputReportChar(2));

			// char6: Input Report (keyboard, report ID 2)
			this->add_subelement(new InputReportChar(2));

			// char7: Input Report (consumer control, report ID 3)
			this->add_subelement(new InputReportChar(3));
		}
};

#endif	// BLUEZ_HID_SERVICES_HPP