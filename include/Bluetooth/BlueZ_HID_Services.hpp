#ifndef BLUEZ_HID_SERVICES_HPP
#define BLUEZ_HID_SERVICES_HPP

#include "Bluetooth/bluetooth_configs.hpp"
#include "Bluetooth/Gatt/Base_App_Obj.hpp"
#include "Bluetooth/Gatt/Characteristic.hpp"
#include "Bluetooth/Gatt/Descriptor.hpp"
#include "Bluetooth/Gatt/Service.hpp"

#include <functional>
#include <iostream>
#include <string>

#include <sdbus-c++/Error.h>


// Long reads (values larger than ATT_MTU - 1, such as the report map) arrive
// as several ReadValue calls with an increasing "offset" option. BlueZ does
// not slice the value itself, so each read must return the tail from offset.
inline ByteArray read_from_offset(const ByteArray& value, const OptionsMap& options)
{
	auto it = options.find("offset");
	if (it == options.end())
		return value;

	const std::size_t offset = it->second.get<uint16_t>();
	if (offset > value.size())
		throw sdbus::Error("org.bluez.Error.InvalidOffset", "Invalid offset");

	return ByteArray(value.begin() + offset, value.end());
}


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

		ByteArray on_read_value(OptionsMap options) const override
		{
			return read_from_offset(this->values, options);
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
		ByteArray values;
	
	public:
		InputReportChar(uint8_t report_id, std::size_t report_size = 2)
			: Characteristic("2a4d", { "read", "notify" }), values(report_size, 0x00)
		{
			this->add_subelement(new RefDesc({ report_id, 0x01 }));	// desc0: Report Reference (Input)
		}

		ByteArray on_read_value(OptionsMap options) const override
		{
			return read_from_offset(this->values, options);
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

// Feature Report Characteristic (UUID: 0x2A4D)
// Flags: ["read", "write"]
// Descriptors: desc0 = Report Reference [id, 0x03=Feature]
// Backed by callbacks so the value lives with the state it describes (e.g. the
// digitizer Input Mode the host writes and the event processor reads).
class FeatureReportChar : public Characteristic
{
	public:
		using Reader = std::function<ByteArray()>;
		using Writer = std::function<void(const ByteArray&)>;

	private:
		Reader reader;
		Writer writer;

	public:
		FeatureReportChar(uint8_t report_id, Reader reader, Writer writer = nullptr)
			: Characteristic("2a4d", { "read", "write" }), reader(std::move(reader)), writer(std::move(writer))
		{
			this->add_subelement(new RefDesc({ report_id, 0x03 }));	// desc0: Report Reference (Feature)
		}

		ByteArray on_read_value(OptionsMap options) const override
		{
			return read_from_offset(this->reader(), options);
		}

		void on_write_value(ByteArray values, OptionsMap) override
		{
			if (this->writer)
				this->writer(values);	// Read-only features silently ignore writes
		}
};

class HIDService : public Service
{
	public:
		explicit HIDService(const hid::Report_Map& report_map = hid::Report_Map{}) : Service("1812", true)
		{
			// char0: Protocol Mode (0x2A4E) — boot or report protocol
			this->add_subelement(new HIDChar("2a4e", ByteArray{ 0x01 }, { "read", "write-without-response" }));

			// char1: HID Information (0x2A4A) — [bcdHID=1.11, bCountryCode=0, Flags=NormallyConnectable]
			this->add_subelement(new HIDChar("2a4a", ByteArray{ 0x11, 0x01, 0x00, 0x02 }, { "read" }));

			// char2: Report Map (0x2A4B) — HID report descriptor bytes
			this->add_subelement(new HIDChar("2a4b", report_map.descriptor, { "read" }));

			// char3: HID Control Point (0x2A4C) — 0x00=Suspend, 0x01=ExitSuspend
			this->add_subelement(new HIDChar("2a4c", ByteArray{}, { "write-without-response" }));

			// char4: Input Report (mouse, report ID 1)
			this->add_subelement(new InputReportChar(hid::MOUSE_REPORT_ID, sizeof(hid_mouse_report)));

			// char5: Output Report (keyboard LEDs, report ID 2)
			this->add_subelement(new OutputReportChar(hid::KEYBOARD_REPORT_ID));

			// char6: Input Report (keyboard, report ID 2)
			this->add_subelement(new InputReportChar(hid::KEYBOARD_REPORT_ID, sizeof(hid_keyboard_report)));

			// char7: Input Report (consumer control, report ID 3)
			this->add_subelement(new InputReportChar(hid::CONSUMER_REPORT_ID, sizeof(hid_consumer_report)));

			// char8+: Generated digitizers (touchpads / touchscreens). Layouts are
			// captured by value; they share their feature state via shared_ptr.
			for (const hid::Digitizer_Layout& layout : report_map.digitizers)
			{
				this->add_subelement(new InputReportChar(layout.input_report_id, layout.input_report_size()));

				this->add_subelement(new FeatureReportChar(layout.caps_report_id,
					[layout]() { return layout.caps_feature(); }));

				if (layout.is_touchpad())
				{
					this->add_subelement(new FeatureReportChar(layout.mode_report_id,
						[layout]() { return layout.mode_feature(); },
						[layout](const ByteArray& value) { layout.write_mode_feature(value); }));

					this->add_subelement(new FeatureReportChar(layout.switch_report_id,
						[layout]() { return layout.switch_feature(); },
						[layout](const ByteArray& value) { layout.write_switch_feature(value); }));
				}
			}
		}
};

#endif	// BLUEZ_HID_SERVICES_HPP