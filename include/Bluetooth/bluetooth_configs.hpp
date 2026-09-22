#ifndef BLUETOOTH_CONFIGS_HPP
#define BLUETOOTH_CONFIGS_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdint.h>
#include <string>
#include <vector>

// ─── Fixed report IDs (static part of the report map) ────────────────────────
namespace hid
{
	constexpr uint8_t MOUSE_REPORT_ID          = 1;
	constexpr uint8_t KEYBOARD_REPORT_ID       = 2;
	constexpr uint8_t CONSUMER_REPORT_ID       = 3;
	constexpr uint8_t FIRST_DYNAMIC_REPORT_ID  = 4;	// Digitizer report IDs are allocated from here
	constexpr uint8_t LAST_REPORT_ID           = 255;

	// ATT caps every attribute value (including the HOGP Report Map) at 512
	// bytes, so the complete generated descriptor must fit in this.
	constexpr std::size_t MAX_REPORT_MAP_SIZE = 512;

	// Largest input report payload (excluding the report ID, which HOGP carries
	// in the Report Reference descriptor) that fits in one ATT notification at
	// the default ATT_MTU of 23. Frames with more contacts than fit are split
	// across several reports using the digitizer "hybrid" reporting mode.
	constexpr std::size_t DEFAULT_MAX_INPUT_PAYLOAD = 20;
}

// ─── Static keyboard / mouse / consumer descriptor (report IDs 1–3) ──────────
const std::vector<uint8_t> corsair_hid_report_desc =
{
	0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
	0x09, 0x02,        // Usage (Mouse)
	0xA1, 0x01,        // Collection (Application)
	0x09, 0x01,        //   Usage (Pointer)
	0xA1, 0x00,        //   Collection (Physical)
	0x85, 0x01,        //     Report ID (1)
	0x05, 0x09,        //     Usage Page (Button)
	0x75, 0x01,        //     Report Size (1)
	0x95, 0x20,        //     Report Count (32)
	0x19, 0x01,        //     Usage Minimum (0x01)
	0x29, 0x20,        //     Usage Maximum (0x20)
	0x15, 0x00,        //     Logical Minimum (0)
	0x25, 0x01,        //     Logical Maximum (1)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
	0x09, 0x30,        //     Usage (X)
	0x09, 0x31,        //     Usage (Y)
	0x75, 0x10,        //     Report Size (16)
	0x95, 0x02,        //     Report Count (2)
	0x26, 0xFF, 0x7F,  //     Logical Maximum (32767)
	0x16, 0x01, 0x80,  //     Logical Minimum (-32767)
	0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
	0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
	0x09, 0x38,        //     Usage (Wheel)
	0x75, 0x08,        //     Report Size (8)
	0x95, 0x01,        //     Report Count (1)
	0x15, 0x81,        //     Logical Minimum (-127)
	0x25, 0x7F,        //     Logical Maximum (127)
	0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
	0x05, 0x0C,        //     Usage Page (Consumer)
	0x0A, 0x38, 0x02,  //     Usage (AC Pan)
	0x95, 0x01,        //     Report Count (1)
	0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
	0xC0,              //   End Collection
	0xC0,              // End Collection
	0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
	0x09, 0x06,        // Usage (Keyboard)
	0xA1, 0x01,        // Collection (Application)
	0x85, 0x02,        //   Report ID (2)
	0x05, 0x08,        //   Usage Page (LEDs)
	0x75, 0x01,        //   Report Size (1)
	0x95, 0x03,        //   Report Count (3)
	0x19, 0x01,        //   Usage Minimum (Num Lock)
	0x29, 0x03,        //   Usage Maximum (Scroll Lock)
	0x15, 0x00,        //   Logical Minimum (0)
	0x25, 0x01,        //   Logical Maximum (1)
	0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	0x95, 0x05,        //   Report Count (5)
	0x91, 0x03,        //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	0x05, 0x07,        //   Usage Page (Kbrd/Keypad)
	0x75, 0x01,        //   Report Size (1)
	0x95, 0x08,        //   Report Count (8)
	0x19, 0xE0,        //   Usage Minimum (0xE0)
	0x29, 0xE7,        //   Usage Maximum (0xE7)
	0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x95, 0x68,        //   Report Count (104)
	0x19, 0x00,        //   Usage Minimum (0x00)
	0x29, 0x67,        //   Usage Maximum (0x67)
	0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0xC0,              // End Collection
	0x05, 0x0C,        // Usage Page (Consumer)
	0x09, 0x01,        // Usage (Consumer Control)
	0xA1, 0x01,        // Collection (Application)
	0x85, 0x03,        //   Report ID (3)
	0x75, 0x10,        //   Report Size (16)
	0x95, 0x01,        //   Report Count (1)
	0x19, 0x00,        //   Usage Minimum (Unassigned)
	0x2A, 0xFF, 0x03,  //   Usage Maximum (0x03FF)
	0x15, 0x00,        //   Logical Minimum (0)
	0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
	0x81, 0x00,        //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0xC0,              // End Collection
};

struct __attribute__((packed)) hid_mouse_report
{
    uint32_t buttons{0};		
    int16_t x{0};
	int16_t y{0};
    int8_t wheel{0};
    int8_t hwheel{0};
};

struct __attribute__((packed)) hid_keyboard_report
{
	uint8_t modifiers{0};
	uint8_t keys[13] = { 0 };
};

struct __attribute__((packed)) hid_consumer_report
{
	uint16_t usage_code{0};
};


// ═════════════════════════════════════════════════════════════════════════════
//  Dynamic touchpad / touchscreen (digitizer) report generation
// ═════════════════════════════════════════════════════════════════════════════
//
//  Instead of a hard-coded touchpad descriptor, one digitizer top-level
//  collection set is generated per touch device detected at runtime, using
//  that device's real axis ranges, resolution and contact count. The
//  descriptor and the code that packs input reports are both driven by the
//  same Digitizer_Layout, so the two cannot disagree about field layout.
//
//  Generated collections follow the Windows Precision Touchpad / touchscreen
//  HID conventions, which Linux hid-multitouch also understands:
//
//    Touch Pad    (0x0D:0x05)  input:   N × Finger{Confidence, Tip, Contact ID, X, Y},
//                                       Scan Time, Contact Count, Button 1..B
//                              feature: Contact Count Maximum, Pad Type
//    Device Configuration (0x0D:0x0E)
//                              feature: Input Mode
//                              feature: Surface Switch, Button Switch
//    Touch Screen (0x0D:0x04)  input:   N × Finger{...}, Scan Time, Contact Count
//                              feature: Contact Count Maximum
//
//  NOTE: Windows only enables Precision Touchpad mode when the device also
//  returns Microsoft's signed certification blob, which cannot be generated
//  here. Without it Windows leaves the touchpad in mouse mode (Input Mode 0);
//  the event processor then emulates a relative mouse on report ID 1, which
//  is also what hosts without HID touchpad support (macOS, iPadOS) get.

namespace hid
{
	namespace usage_page
	{
		constexpr uint16_t GENERIC_DESKTOP = 0x01;
		constexpr uint16_t BUTTON          = 0x09;
		constexpr uint16_t DIGITIZER       = 0x0D;
	}

	namespace usage
	{
		// Generic Desktop page
		constexpr uint16_t X = 0x30;
		constexpr uint16_t Y = 0x31;

		// Digitizer page
		constexpr uint16_t TOUCH_SCREEN          = 0x04;
		constexpr uint16_t TOUCH_PAD             = 0x05;
		constexpr uint16_t DEVICE_CONFIGURATION  = 0x0E;
		constexpr uint16_t FINGER                = 0x22;
		constexpr uint16_t TIP_SWITCH            = 0x42;
		constexpr uint16_t CONFIDENCE            = 0x47;
		constexpr uint16_t CONTACT_IDENTIFIER    = 0x51;
		constexpr uint16_t INPUT_MODE            = 0x52;
		constexpr uint16_t CONTACT_COUNT         = 0x54;
		constexpr uint16_t CONTACT_COUNT_MAXIMUM = 0x55;
		constexpr uint16_t SCAN_TIME             = 0x56;
		constexpr uint16_t SURFACE_SWITCH        = 0x57;
		constexpr uint16_t BUTTON_SWITCH         = 0x58;
		constexpr uint16_t PAD_TYPE              = 0x59;
	}

	namespace collection
	{
		constexpr uint8_t PHYSICAL    = 0x00;
		constexpr uint8_t APPLICATION = 0x01;
		constexpr uint8_t LOGICAL     = 0x02;
	}

	namespace field
	{
		constexpr uint8_t DATA_VAR_ABS  = 0x02;
		constexpr uint8_t CONST_VAR_ABS = 0x03;
	}

	namespace unit
	{
		constexpr uint32_t NONE       = 0x0000;
		constexpr uint32_t CENTIMETER = 0x0011;	// SI Linear, length^1
		constexpr uint32_t SECOND     = 0x1001;	// SI Linear, time^1
	}

	// Values the host writes to the Input Mode feature
	namespace input_mode
	{
		constexpr uint8_t MOUSE    = 0x00;
		constexpr uint8_t TOUCHPAD = 0x03;
	}

	// Values reported in the Pad Type feature
	namespace pad_type
	{
		constexpr uint8_t DEPRESSIBLE     = 0x00;	// Clickpad
		constexpr uint8_t PRESSURE        = 0x01;	// Pressure pad
		constexpr uint8_t NON_DEPRESSIBLE = 0x02;	// Discrete buttons
	}

	constexpr unsigned CONTACT_ID_BITS      = 6;	// Contact ID field width
	constexpr unsigned MAX_SLOT_COUNT       = 1u << CONTACT_ID_BITS;
	constexpr unsigned MAX_TOUCHPAD_BUTTONS = 3;


	// ─── HID descriptor item encoder ─────────────────────────────────────────
	// Emits short items with the smallest valid data size. Global items that
	// would not change the parser's global state are skipped (global state
	// persists across fields and collections), which keeps repeated finger
	// collections compact enough to fit in the 512-byte report map.
	class Descriptor_Builder
	{
		private:
			enum : uint8_t { MAIN = 0, GLOBAL = 1, LOCAL = 2 };

			std::vector<uint8_t> data;
			std::array<std::optional<std::vector<uint8_t>>, 16> global_state;
			int depth = 0;

			static std::vector<uint8_t> encode_unsigned(uint32_t value)
			{
				std::size_t size = (value <= 0xFFu) ? 1 : (value <= 0xFFFFu) ? 2 : 4;
				std::vector<uint8_t> out(size);
				for (std::size_t n = 0; n < size; ++n)
					out[n] = static_cast<uint8_t>(value >> (8 * n));
				return out;
			}

			static std::vector<uint8_t> encode_signed(int32_t value)
			{
				std::size_t size = (value >= INT8_MIN && value <= INT8_MAX) ? 1
					: (value >= INT16_MIN && value <= INT16_MAX) ? 2 : 4;
				std::vector<uint8_t> out(size);
				for (std::size_t n = 0; n < size; ++n)
					out[n] = static_cast<uint8_t>(static_cast<uint32_t>(value) >> (8 * n));
				return out;
			}

			Descriptor_Builder& emit(uint8_t tag, uint8_t type, const std::vector<uint8_t>& payload)
			{
				const uint8_t size_code = (payload.size() == 4) ? 3 : static_cast<uint8_t>(payload.size());
				this->data.push_back(static_cast<uint8_t>((tag << 4) | (type << 2) | size_code));
				this->data.insert(this->data.end(), payload.begin(), payload.end());
				return *this;
			}

			Descriptor_Builder& global(uint8_t tag, const std::vector<uint8_t>& payload)
			{
				if (this->global_state[tag] == payload)
					return *this;	// Redundant: parser state already holds this value
				this->global_state[tag] = payload;
				return this->emit(tag, GLOBAL, payload);
			}

		public:
			// Global items
			Descriptor_Builder& usage_page(uint16_t page)       { return this->global(0x0, encode_unsigned(page)); }
			Descriptor_Builder& logical_minimum(int32_t value)  { return this->global(0x1, encode_signed(value)); }
			Descriptor_Builder& logical_maximum(int32_t value)  { return this->global(0x2, encode_signed(value)); }
			Descriptor_Builder& physical_minimum(int32_t value) { return this->global(0x3, encode_signed(value)); }
			Descriptor_Builder& physical_maximum(int32_t value) { return this->global(0x4, encode_signed(value)); }
			// 4-bit two's-complement nibble (e.g. -2 → 0x0E), the form Windows expects
			Descriptor_Builder& unit_exponent(int8_t exponent)  { return this->global(0x5, { static_cast<uint8_t>(exponent & 0x0F) }); }
			Descriptor_Builder& unit(uint32_t value)            { return this->global(0x6, encode_unsigned(value)); }
			Descriptor_Builder& report_size(uint32_t bits)      { return this->global(0x7, encode_unsigned(bits)); }
			Descriptor_Builder& report_id(uint8_t id)           { return this->global(0x8, encode_unsigned(id)); }
			Descriptor_Builder& report_count(uint32_t count)    { return this->global(0x9, encode_unsigned(count)); }

			// Local items
			Descriptor_Builder& usage(uint16_t id)              { return this->emit(0x0, LOCAL, encode_unsigned(id)); }
			Descriptor_Builder& usage_minimum(uint16_t id)      { return this->emit(0x1, LOCAL, encode_unsigned(id)); }
			Descriptor_Builder& usage_maximum(uint16_t id)      { return this->emit(0x2, LOCAL, encode_unsigned(id)); }

			// Main items
			Descriptor_Builder& input(uint8_t flags)            { return this->emit(0x8, MAIN, { flags }); }
			Descriptor_Builder& output(uint8_t flags)           { return this->emit(0x9, MAIN, { flags }); }
			Descriptor_Builder& feature(uint8_t flags)          { return this->emit(0xB, MAIN, { flags }); }
			Descriptor_Builder& collection(uint8_t type)        { ++this->depth; return this->emit(0xA, MAIN, { type }); }
			Descriptor_Builder& end_collection()                { --this->depth; return this->emit(0xC, MAIN, {}); }

			const std::vector<uint8_t>& bytes() const { return this->data; }
			int open_collections() const { return this->depth; }
	};


	// ─── Report bit packer ───────────────────────────────────────────────────
	// Packs fields LSB-first in declaration order, as HID requires.
	class Report_Writer
	{
		private:
			std::vector<uint8_t> data;
			std::size_t bit = 0;

		public:
			void write(uint32_t value, unsigned bits)
			{
				for (unsigned n = 0; n < bits; ++n, ++this->bit)
				{
					if (this->bit / 8 >= this->data.size())
						this->data.push_back(0);
					if ((value >> n) & 1u)
						this->data[this->bit / 8] |= static_cast<uint8_t>(1u << (this->bit % 8));
				}
			}

			std::vector<uint8_t> take() { return std::move(this->data); }
	};


	// ─── Digitizer description (generator input) ─────────────────────────────
	enum class Digitizer_Kind : uint8_t { Touchpad, Touchscreen };

	struct Axis_Spec
	{
		int32_t minimum = 0;
		int32_t maximum = 0;
		int32_t resolution = 0;	// Units per millimetre (evdev convention); 0 = unknown
	};

	struct Digitizer_Spec
	{
		unsigned source_id = 0;			// ID of the input device feeding this digitizer
		std::string name;
		Digitizer_Kind kind = Digitizer_Kind::Touchpad;
		bool multitouch = false;		// true: evdev MT protocol B slots; false: ABS_X/Y + BTN_TOUCH
		uint8_t slot_count = 1;			// Contact IDs are slot indices 0..slot_count-1
		uint8_t max_contacts = 1;		// Contacts reported simultaneously (≤ slot_count)
		Axis_Spec x;
		Axis_Spec y;
		uint8_t button_count = 1;		// Touchpad only: Button 1..N
		uint8_t pad_type = pad_type::NON_DEPRESSIBLE;	// Touchpad only
	};

	// Host-writable feature state, shared between the GATT feature report
	// characteristics (written on the D-Bus thread) and the event processor.
	struct Digitizer_Feature_State
	{
		std::atomic<uint8_t> input_mode{input_mode::MOUSE};
		std::atomic<bool> surface_switch{true};
		std::atomic<bool> button_switch{true};
	};

	struct Contact
	{
		bool tip = false;
		bool confidence = true;
		uint8_t id = 0;
		int32_t x = 0;
		int32_t y = 0;
	};


	// ─── Digitizer layout (generator output) ─────────────────────────────────
	struct Digitizer_Layout
	{
		Digitizer_Spec spec;
		uint8_t input_report_id = 0;
		uint8_t caps_report_id = 0;		// Feature: Contact Count Maximum [, Pad Type]
		uint8_t mode_report_id = 0;		// Feature: Input Mode (touchpad only, else 0)
		uint8_t switch_report_id = 0;	// Feature: Surface/Button Switch (touchpad only, else 0)
		uint8_t contacts_per_report = 1;
		uint8_t coordinate_bits = 16;
		std::shared_ptr<Digitizer_Feature_State> state = std::make_shared<Digitizer_Feature_State>();

		bool is_touchpad() const { return this->spec.kind == Digitizer_Kind::Touchpad; }

		static unsigned report_ids_needed(Digitizer_Kind kind)
		{
			return (kind == Digitizer_Kind::Touchpad) ? 4 : 2;
		}

		static uint8_t coordinate_bits_for(const Axis_Spec& x, const Axis_Spec& y)
		{
			auto fits_16 = [](const Axis_Spec& a)
			{
				return (a.minimum >= INT16_MIN && a.maximum <= INT16_MAX)
					|| (a.minimum >= 0 && a.maximum <= static_cast<int32_t>(UINT16_MAX));
			};
			return (fits_16(x) && fits_16(y)) ? 16 : 32;
		}

		std::size_t finger_bytes() const { return 1 + 2 * (this->coordinate_bits / 8); }

		// Bytes after the finger fields: scan time (2) + contact count (1) [+ buttons (1)]
		std::size_t trailer_bytes() const { return 3 + (this->is_touchpad() ? 1 : 0); }

		std::size_t input_report_size() const
		{
			return this->contacts_per_report * this->finger_bytes() + this->trailer_bytes();
		}

		// ── Feature report values (without report ID) ────────────────────────
		std::vector<uint8_t> caps_feature() const
		{
			if (this->is_touchpad())
				return { this->spec.max_contacts, this->spec.pad_type };
			return { this->spec.max_contacts };
		}

		std::vector<uint8_t> mode_feature() const
		{
			return { this->state->input_mode.load(std::memory_order_acquire) };
		}

		void write_mode_feature(const std::vector<uint8_t>& value) const
		{
			if (!value.empty())
				this->state->input_mode.store(value[0], std::memory_order_release);
		}

		std::vector<uint8_t> switch_feature() const
		{
			return {
				static_cast<uint8_t>(
					(this->state->surface_switch.load(std::memory_order_acquire) ? 0x01 : 0x00) |
					(this->state->button_switch.load(std::memory_order_acquire) ? 0x02 : 0x00))
			};
		}

		void write_switch_feature(const std::vector<uint8_t>& value) const
		{
			if (value.empty())
				return;
			this->state->surface_switch.store(value[0] & 0x01, std::memory_order_release);
			this->state->button_switch.store(value[0] & 0x02, std::memory_order_release);
		}

		// ── Descriptor ───────────────────────────────────────────────────────
		void append_descriptor(Descriptor_Builder& b) const
		{
			const bool touchpad = this->is_touchpad();
			const bool has_units = this->spec.x.resolution > 0 || this->spec.y.resolution > 0;

			// Physical size in 0.1 mm (unit = cm, exponent -2), derived from the
			// evdev resolution in units/mm. 0 = unknown (host uses logical range).
			auto physical_extent = [](const Axis_Spec& a) -> int32_t
			{
				if (a.resolution <= 0)
					return 0;
				int64_t tenths_mm = (static_cast<int64_t>(a.maximum) - a.minimum) * 10 / a.resolution;
				return static_cast<int32_t>(std::min<int64_t>(tenths_mm, INT32_MAX));
			};

			b.usage_page(usage_page::DIGITIZER)
				.usage(touchpad ? usage::TOUCH_PAD : usage::TOUCH_SCREEN)
				.collection(collection::APPLICATION)
				.report_id(this->input_report_id);

			for (unsigned n = 0; n < this->contacts_per_report; ++n)
			{
				b.usage_page(usage_page::DIGITIZER)
					.usage(usage::FINGER)
					.collection(collection::LOGICAL)
						// Confidence (bit 0), Tip Switch (bit 1)
						.logical_minimum(0).logical_maximum(1)
						.report_size(1).report_count(2)
						.usage(usage::CONFIDENCE).usage(usage::TIP_SWITCH)
						.input(field::DATA_VAR_ABS)
						// Contact Identifier (bits 2–7) = evdev slot index
						.logical_maximum(this->spec.slot_count - 1)
						.report_size(CONTACT_ID_BITS).report_count(1)
						.usage(usage::CONTACT_IDENTIFIER)
						.input(field::DATA_VAR_ABS)
						// X, Y
						.usage_page(usage_page::GENERIC_DESKTOP)
						.report_size(this->coordinate_bits);

				if (has_units)
					b.unit_exponent(-2).unit(unit::CENTIMETER);
				else
					b.unit_exponent(0).unit(unit::NONE);

				b.logical_minimum(this->spec.x.minimum).logical_maximum(this->spec.x.maximum)
					.physical_minimum(0).physical_maximum(physical_extent(this->spec.x))
					.usage(usage::X).input(field::DATA_VAR_ABS)
					.logical_minimum(this->spec.y.minimum).logical_maximum(this->spec.y.maximum)
					.physical_maximum(physical_extent(this->spec.y))
					.usage(usage::Y).input(field::DATA_VAR_ABS)
				.end_collection();
			}

			// Scan Time: 100 µs units, wrapping 16-bit counter
			b.usage_page(usage_page::DIGITIZER)
				.unit_exponent(-4).unit(unit::SECOND)
				.logical_minimum(0).logical_maximum(UINT16_MAX)
				.physical_minimum(0).physical_maximum(UINT16_MAX)
				.report_size(16).report_count(1)
				.usage(usage::SCAN_TIME).input(field::DATA_VAR_ABS)
				// Contact Count: total in this frame (0 in hybrid follow-up reports)
				.unit_exponent(0).unit(unit::NONE)
				.physical_maximum(0)
				.logical_maximum(this->spec.max_contacts)
				.report_size(8)
				.usage(usage::CONTACT_COUNT).input(field::DATA_VAR_ABS);

			if (touchpad)
			{
				const unsigned buttons = this->spec.button_count;
				b.usage_page(usage_page::BUTTON)
					.usage_minimum(1).usage_maximum(buttons)
					.logical_maximum(1)
					.report_size(1).report_count(buttons)
					.input(field::DATA_VAR_ABS);
				if (buttons < 8)
					b.report_count(8 - buttons).input(field::CONST_VAR_ABS);
			}

			// Device capabilities feature report
			b.usage_page(usage_page::DIGITIZER)
				.report_id(this->caps_report_id)
				.logical_maximum(this->spec.max_contacts)
				.report_size(8).report_count(1)
				.usage(usage::CONTACT_COUNT_MAXIMUM).feature(field::DATA_VAR_ABS);
			if (touchpad)
				b.logical_maximum(pad_type::NON_DEPRESSIBLE)
					.usage(usage::PAD_TYPE).feature(field::DATA_VAR_ABS);

			b.end_collection();

			if (touchpad)
			{
				// Configuration collection: lets the host switch between mouse
				// and touchpad reporting, and selectively disable surface/buttons.
				b.usage_page(usage_page::DIGITIZER)
					.usage(usage::DEVICE_CONFIGURATION)
					.collection(collection::APPLICATION)
						.report_id(this->mode_report_id)
						.usage(usage::FINGER)
						.collection(collection::LOGICAL)
							.usage(usage::INPUT_MODE)
							.logical_maximum(10)
							.report_size(8).report_count(1)
							.feature(field::DATA_VAR_ABS)
						.end_collection()
						.usage(usage::FINGER)
						.collection(collection::PHYSICAL)
							.report_id(this->switch_report_id)
							.usage(usage::SURFACE_SWITCH).usage(usage::BUTTON_SWITCH)
							.logical_maximum(1)
							.report_size(1).report_count(2)
							.feature(field::DATA_VAR_ABS)
							.report_count(6).feature(field::CONST_VAR_ABS)
						.end_collection()
					.end_collection();
			}
		}

		// ── Input report packing ─────────────────────────────────────────────
		// Returns one or more reports (without report ID) for one frame. Frames
		// with more contacts than fit in a report use hybrid mode: the first
		// report carries the frame's total contact count, follow-ups carry 0.
		std::vector<std::vector<uint8_t>> pack_input_reports(const std::vector<Contact>& contacts, uint16_t scan_time, uint8_t buttons) const
		{
			const std::size_t total = std::min<std::size_t>(contacts.size(), this->spec.max_contacts);
			const std::size_t per_report = this->contacts_per_report;
			const std::size_t report_count = std::max<std::size_t>(1, (total + per_report - 1) / per_report);

			auto clamp_axis = [](int32_t value, const Axis_Spec& a)
			{
				return static_cast<uint32_t>(std::clamp(value, a.minimum, a.maximum));
			};

			std::vector<std::vector<uint8_t>> reports;
			reports.reserve(report_count);

			for (std::size_t r = 0; r < report_count; ++r)
			{
				Report_Writer w;
				for (std::size_t n = 0; n < per_report; ++n)
				{
					const std::size_t index = r * per_report + n;
					if (index < total)
					{
						const Contact& c = contacts[index];
						w.write(c.confidence ? 1 : 0, 1);
						w.write(c.tip ? 1 : 0, 1);
						w.write(c.id, CONTACT_ID_BITS);
						w.write(clamp_axis(c.x, this->spec.x), this->coordinate_bits);
						w.write(clamp_axis(c.y, this->spec.y), this->coordinate_bits);
					}
					else	// Unused finger slot; beyond Contact Count, so ignored by the host
					{
						w.write(0, 2 + CONTACT_ID_BITS);
						w.write(static_cast<uint32_t>(this->spec.x.minimum), this->coordinate_bits);
						w.write(static_cast<uint32_t>(this->spec.y.minimum), this->coordinate_bits);
					}
				}

				w.write(scan_time, 16);
				w.write(r == 0 ? static_cast<uint32_t>(total) : 0u, 8);

				if (this->is_touchpad())
				{
					w.write(buttons, this->spec.button_count);
					w.write(0, 8 - this->spec.button_count);
				}

				reports.push_back(w.take());
			}

			return reports;
		}
	};


	// ─── Complete report map ─────────────────────────────────────────────────
	struct Report_Map
	{
		std::vector<uint8_t> descriptor = corsair_hid_report_desc;
		std::vector<Digitizer_Layout> digitizers;
		std::vector<Digitizer_Spec> omitted;	// Specs that were invalid or did not fit

		const Digitizer_Layout* find(unsigned source_id) const
		{
			for (const auto& d : this->digitizers)
				if (d.spec.source_id == source_id)
					return &d;
			return nullptr;
		}
	};

	namespace detail
	{
		inline Digitizer_Spec sanitize(Digitizer_Spec spec)
		{
			spec.slot_count = static_cast<uint8_t>(std::clamp<unsigned>(spec.slot_count, 1, MAX_SLOT_COUNT));
			spec.max_contacts = static_cast<uint8_t>(std::clamp<unsigned>(spec.max_contacts, 1, spec.slot_count));
			if (spec.kind == Digitizer_Kind::Touchpad)
			{
				spec.button_count = static_cast<uint8_t>(std::clamp<unsigned>(spec.button_count, 1, MAX_TOUCHPAD_BUTTONS));
				spec.pad_type = std::min(spec.pad_type, pad_type::NON_DEPRESSIBLE);
			}
			else
			{
				spec.button_count = 0;
			}
			return spec;
		}

		inline bool is_valid(const Digitizer_Spec& spec)
		{
			return spec.x.maximum > spec.x.minimum && spec.y.maximum > spec.y.minimum;
		}

		// Assemble a report map for the given specs and per-digitizer contacts
		// per report. Specs that would run out of report IDs are omitted.
		inline Report_Map assemble(const std::vector<Digitizer_Spec>& specs, const std::vector<uint8_t>& per_report, std::vector<Digitizer_Spec>& omitted)
		{
			Report_Map map;
			Descriptor_Builder builder;
			unsigned next_id = FIRST_DYNAMIC_REPORT_ID;

			for (std::size_t n = 0; n < specs.size(); ++n)
			{
				const unsigned ids = Digitizer_Layout::report_ids_needed(specs[n].kind);
				if (next_id + ids - 1 > LAST_REPORT_ID)
				{
					omitted.push_back(specs[n]);
					continue;
				}

				Digitizer_Layout layout;
				layout.spec = specs[n];
				layout.coordinate_bits = Digitizer_Layout::coordinate_bits_for(specs[n].x, specs[n].y);
				layout.contacts_per_report = per_report[n];
				layout.input_report_id = static_cast<uint8_t>(next_id++);
				layout.caps_report_id = static_cast<uint8_t>(next_id++);
				if (layout.is_touchpad())
				{
					layout.mode_report_id = static_cast<uint8_t>(next_id++);
					layout.switch_report_id = static_cast<uint8_t>(next_id++);
				}
				else
				{
					// Touchscreens have no mouse-mode fallback; always report as a digitizer
					layout.state->input_mode.store(input_mode::TOUCHPAD, std::memory_order_relaxed);
				}

				layout.append_descriptor(builder);
				map.digitizers.push_back(std::move(layout));
			}

			map.descriptor.insert(map.descriptor.end(), builder.bytes().begin(), builder.bytes().end());
			return map;
		}
	}

	// Build the complete report map: the static keyboard/mouse/consumer
	// collections followed by generated collections for each digitizer.
	//
	// Contacts per input report start at whatever fits in max_input_payload.
	// If the descriptor then exceeds max_descriptor_size, contacts per report
	// are reduced (largest first, down to 1 – the host reassembles frames via
	// hybrid mode) and only then are digitizers dropped from the end.
	inline Report_Map build_report_map(
		const std::vector<Digitizer_Spec>& requested,
		std::size_t max_input_payload = DEFAULT_MAX_INPUT_PAYLOAD,
		std::size_t max_descriptor_size = MAX_REPORT_MAP_SIZE)
	{
		std::vector<Digitizer_Spec> specs;
		std::vector<Digitizer_Spec> omitted;
		for (const auto& spec : requested)
		{
			if (detail::is_valid(spec))
				specs.push_back(detail::sanitize(spec));
			else
				omitted.push_back(spec);
		}

		std::vector<uint8_t> per_report;
		for (const auto& spec : specs)
		{
			Digitizer_Layout probe;
			probe.spec = spec;
			probe.coordinate_bits = Digitizer_Layout::coordinate_bits_for(spec.x, spec.y);
			const std::size_t room = (max_input_payload > probe.trailer_bytes()) ? max_input_payload - probe.trailer_bytes() : 0;
			const std::size_t fit = std::max<std::size_t>(1, room / probe.finger_bytes());
			per_report.push_back(static_cast<uint8_t>(std::min<std::size_t>(fit, spec.max_contacts)));
		}

		for (;;)
		{
			std::vector<Digitizer_Spec> id_overflow;
			Report_Map map = detail::assemble(specs, per_report, id_overflow);

			if (map.descriptor.size() <= max_descriptor_size || specs.empty())
			{
				map.omitted = omitted;
				map.omitted.insert(map.omitted.end(), id_overflow.begin(), id_overflow.end());
				return map;
			}

			auto largest = std::max_element(per_report.begin(), per_report.end());
			if (*largest > 1)
			{
				--*largest;
			}
			else
			{
				omitted.push_back(specs.back());
				specs.pop_back();
				per_report.pop_back();
			}
		}
	}
}

#endif	// BLUETOOTH_CONFIGS_HPP