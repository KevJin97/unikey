#include "Bluetooth/unikey-bluetooth.hpp"
#include "Bluetooth/bluetooth_configs.hpp"
#include "Bluetooth/BlueZ_Interface.hpp"
#include "unikey.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <string>
#include <vector>

#include <sdbus-c++/Message.h>

static BlueZ_Interface* unikey_bluetooth = nullptr;
std::unique_ptr<sdbus::IObject> unikey_bluetooth_dbus_obj;

// ─── Persistent HID report state ─────────────────────────────────────────────
// These persist across calls so that key-down events from one batch and
// key-up events from a later batch produce the correct accumulated state.
// s_mouse.buttons holds physical mouse buttons only; touchpad click emulation
// is OR-ed in when the report is sent (see send_mouse_report).
static hid_mouse_report    s_mouse;
static hid_keyboard_report s_keyboard;
static hid_consumer_report s_consumer;


// ─── Touch device tracking ───────────────────────────────────────────────────
namespace
{
	// Relative-mouse emulation used while a touchpad is in Input Mode 0
	// (the host never switched it to touchpad mode: macOS, iPadOS, or Windows
	// without Precision Touchpad certification).
	constexpr double MOUSE_COUNTS_PER_MM  = 16.0;	// ≈ 400 DPI before host acceleration
	constexpr double SCROLL_MM_PER_DETENT = 3.0;	// Two-finger travel per wheel step
	constexpr double FALLBACK_SURFACE_MM  = 100.0;	// Assumed size when resolution is unknown

	constexpr unsigned MAX_TOUCHPAD_CONTACTS    = 5;	// Windows Precision Touchpad maximum
	constexpr unsigned MAX_TOUCHSCREEN_CONTACTS = 10;

	constexpr uint8_t BUTTON_LEFT   = 0x01;	// → HID Button 1
	constexpr uint8_t BUTTON_RIGHT  = 0x02;	// → HID Button 2
	constexpr uint8_t BUTTON_MIDDLE = 0x04;	// → HID Button 3

	template <typename T>
	T saturate(long long value)
	{
		return static_cast<T>(std::clamp<long long>(value, std::numeric_limits<T>::min(), std::numeric_limits<T>::max()));
	}

	// Follows the evdev state of one touch device (MT protocol B slots, or
	// ABS_X/ABS_Y + BTN_TOUCH for single-touch devices) and converts each
	// completed batch into digitizer input reports – or, for a touchpad the
	// host has left in mouse mode, into relative mouse motion and clicks.
	class Touch_Tracker
	{
		private:
			struct Slot
			{
				int32_t tracking_id = -1;	// -1 = no contact
				int32_t x = 0;
				int32_t y = 0;
				bool lifting = false;		// Contact ended this frame: report once with Tip = 0
			};

			hid::Digitizer_Layout layout;
			std::vector<Slot> slots;
			std::size_t current_slot = 0;	// Kernel does not resend ABS_MT_SLOT while unchanged
			uint8_t buttons = 0;			// Physical BUTTON_* bitmap
			uint16_t scan_time = 0;
			bool dirty = false;

			// Mouse-mode emulation
			double counts_per_unit_x = 1.0;
			double counts_per_unit_y = 1.0;
			double detents_per_unit_x = 0.0;
			double detents_per_unit_y = 0.0;
			double acc_x = 0.0, acc_y = 0.0, acc_wheel = 0.0, acc_hwheel = 0.0;
			std::size_t previous_active = 0;
			std::ptrdiff_t previous_slot = -1;
			double previous_x = 0.0, previous_y = 0.0;
			uint32_t emulated_buttons = 0;
			uint32_t click_buttons = 0;		// Button chosen when a clickpad click started
			bool click_held = false;

			static double units_per_mm(const hid::Axis_Spec& axis)
			{
				if (axis.resolution > 0)
					return axis.resolution;
				return std::max(1.0, (static_cast<double>(axis.maximum) - axis.minimum) / FALLBACK_SURFACE_MM);
			}

			Slot* active_slot()
			{
				return (this->current_slot < this->slots.size()) ? &this->slots[this->current_slot] : nullptr;
			}

			void set_contact(Slot& slot, bool down, int32_t tracking_id)
			{
				if (down)
				{
					slot.tracking_id = tracking_id;
					slot.lifting = false;
				}
				else
				{
					if (slot.tracking_id >= 0)
						slot.lifting = true;
					slot.tracking_id = -1;
				}
			}

			void stamp(const struct input_event& ev)
			{
				// Digitizer Scan Time: 100 µs units, wraps at 16 bits
				uint64_t ticks = static_cast<uint64_t>(ev.input_event_sec) * 10000u + static_cast<uint64_t>(ev.input_event_usec) / 100u;
				this->scan_time = static_cast<uint16_t>(ticks & 0xFFFF);
				this->dirty = true;
			}

			bool in_mouse_mode() const
			{
				return this->layout.is_touchpad()
					&& this->layout.state->input_mode.load(std::memory_order_acquire) == hid::input_mode::MOUSE;
			}

			void reset_emulation()
			{
				this->acc_x = this->acc_y = this->acc_wheel = this->acc_hwheel = 0.0;
				this->previous_active = 0;
				this->previous_slot = -1;
				this->click_held = false;
				this->click_buttons = 0;
			}

			void send_digitizer_frame(BlueZ_Interface& bt)
			{
				const bool surface_on = this->layout.state->surface_switch.load(std::memory_order_acquire);
				const bool buttons_on = this->layout.state->button_switch.load(std::memory_order_acquire);

				std::vector<hid::Contact> contacts;
				for (std::size_t n = 0; n < this->slots.size(); ++n)
				{
					Slot& slot = this->slots[n];
					if (slot.tracking_id < 0 && !slot.lifting)
						continue;

					if (surface_on)
					{
						hid::Contact contact;
						contact.tip = slot.tracking_id >= 0;
						contact.confidence = true;
						contact.id = static_cast<uint8_t>(n);	// Slot index is stable for the contact's lifetime
						contact.x = slot.x;
						contact.y = slot.y;
						contacts.push_back(contact);
					}
					slot.lifting = false;
				}

				const uint8_t button_mask = static_cast<uint8_t>((1u << this->layout.spec.button_count) - 1);
				const uint8_t reported_buttons = buttons_on ? (this->buttons & button_mask) : 0;

				for (const auto& report : this->layout.pack_input_reports(contacts, this->scan_time, reported_buttons))
					bt.send_hid_report(this->layout.input_report_id, report);
			}

			// Returns true if the mouse report needs to be sent
			bool emulate_mouse(hid_mouse_report& mouse)
			{
				std::vector<const Slot*> active;
				for (Slot& slot : this->slots)
				{
					slot.lifting = false;
					if (slot.tracking_id >= 0)
						active.push_back(&slot);
				}

				if (active.size() == 1)
				{
					// One finger: pointer motion
					const std::ptrdiff_t index = active[0] - this->slots.data();
					if (this->previous_active == 1 && this->previous_slot == index)
					{
						this->acc_x += (active[0]->x - this->previous_x) * this->counts_per_unit_x;
						this->acc_y += (active[0]->y - this->previous_y) * this->counts_per_unit_y;
					}
					this->previous_slot = index;
					this->previous_x = active[0]->x;
					this->previous_y = active[0]->y;
				}
				else if (active.size() >= 2)
				{
					// Two or more fingers: scroll by the motion of the first two
					const double cx = (static_cast<double>(active[0]->x) + active[1]->x) / 2.0;
					const double cy = (static_cast<double>(active[0]->y) + active[1]->y) / 2.0;
					if (this->previous_active >= 2)
					{
						this->acc_hwheel += (cx - this->previous_x) * this->detents_per_unit_x;
						this->acc_wheel  -= (cy - this->previous_y) * this->detents_per_unit_y;	// Fingers up = wheel up
					}
					this->previous_slot = -1;
					this->previous_x = cx;
					this->previous_y = cy;
				}
				else
				{
					this->previous_slot = -1;
				}
				this->previous_active = active.size();

				auto take_whole = [](double& accumulator)
				{
					const double whole = std::trunc(accumulator);
					accumulator -= whole;
					return static_cast<long long>(whole);
				};

				bool changed = false;
				const long long dx = take_whole(this->acc_x);
				const long long dy = take_whole(this->acc_y);
				const long long wheel = take_whole(this->acc_wheel);
				const long long hwheel = take_whole(this->acc_hwheel);
				if (dx || dy)
				{
					mouse.x = saturate<int16_t>(mouse.x + dx);
					mouse.y = saturate<int16_t>(mouse.y + dy);
					changed = true;
				}
				if (wheel || hwheel)
				{
					mouse.wheel  = saturate<int8_t>(mouse.wheel + wheel);
					mouse.hwheel = saturate<int8_t>(mouse.hwheel + hwheel);
					changed = true;
				}

				// Clickpads have one physical button: pick left/right by finger
				// count when the click starts and hold that choice until release.
				uint32_t new_buttons = this->buttons;
				if (this->layout.spec.pad_type != hid::pad_type::NON_DEPRESSIBLE)
				{
					if (this->buttons & BUTTON_LEFT)
					{
						if (!this->click_held)
						{
							this->click_buttons = (active.size() >= 2) ? BUTTON_RIGHT : BUTTON_LEFT;
							this->click_held = true;
						}
						new_buttons = this->click_buttons;
					}
					else
					{
						this->click_held = false;
						new_buttons = 0;
					}
				}

				if (new_buttons != this->emulated_buttons)
				{
					this->emulated_buttons = new_buttons;
					changed = true;
				}

				return changed;
			}

		public:
			explicit Touch_Tracker(const hid::Digitizer_Layout& layout)
				: layout(layout), slots(layout.spec.slot_count)
			{
				const double units_x = units_per_mm(layout.spec.x);
				const double units_y = units_per_mm(layout.spec.y);
				this->counts_per_unit_x = MOUSE_COUNTS_PER_MM / units_x;
				this->counts_per_unit_y = MOUSE_COUNTS_PER_MM / units_y;
				this->detents_per_unit_x = 1.0 / (units_x * SCROLL_MM_PER_DETENT);
				this->detents_per_unit_y = 1.0 / (units_y * SCROLL_MM_PER_DETENT);
			}

			unsigned source() const { return this->layout.spec.source_id; }

			uint32_t mouse_buttons() const { return this->emulated_buttons; }

			// Returns true if the event belongs to the touch surface. Anything
			// else (e.g. keys on a combined device) goes to the generic handlers.
			bool consume(const struct input_event& ev)
			{
				switch (ev.type)
				{
					case EV_ABS:
						this->stamp(ev);
						if (this->layout.spec.multitouch)
						{
							switch (ev.code)
							{
								case ABS_MT_SLOT:
									this->current_slot = (ev.value >= 0) ? static_cast<std::size_t>(ev.value) : this->slots.size();
									break;
								case ABS_MT_TRACKING_ID:
									if (Slot* slot = this->active_slot())
										this->set_contact(*slot, ev.value >= 0, ev.value);
									break;
								case ABS_MT_POSITION_X:
									if (Slot* slot = this->active_slot())
										slot->x = ev.value;
									break;
								case ABS_MT_POSITION_Y:
									if (Slot* slot = this->active_slot())
										slot->y = ev.value;
									break;
							}
						}
						else if (ev.code == ABS_X)
						{
							this->slots[0].x = ev.value;
						}
						else if (ev.code == ABS_Y)
						{
							this->slots[0].y = ev.value;
						}
						return true;	// Other axes (pressure, width, ...) are absorbed

					case EV_KEY:
						if (ev.code == BTN_TOUCH)
						{
							if (!this->layout.spec.multitouch)
								this->set_contact(this->slots[0], ev.value != 0, 0);
							this->stamp(ev);
							return true;
						}
						if (ev.code >= BTN_DIGI && ev.code <= BTN_TOOL_QUADTAP)
						{
							return true;	// Tool/finger-count indicators; implied by slot state
						}
						if (this->layout.is_touchpad())
						{
							uint8_t bit = (ev.code == BTN_LEFT) ? BUTTON_LEFT
								: (ev.code == BTN_RIGHT) ? BUTTON_RIGHT
								: (ev.code == BTN_MIDDLE) ? BUTTON_MIDDLE : 0;
							if (bit)
							{
								if (ev.value) this->buttons |= bit;
								else          this->buttons &= static_cast<uint8_t>(~bit);
								this->stamp(ev);
								return true;
							}
						}
						return false;

					default:
						return false;
				}
			}

			// Emit whatever the consumed events changed. Returns true if the
			// shared mouse report changed (mouse-mode emulation).
			bool flush(BlueZ_Interface& bt, hid_mouse_report& mouse)
			{
				if (!this->dirty)
					return false;
				this->dirty = false;

				if (this->in_mouse_mode())
					return this->emulate_mouse(mouse);

				this->send_digitizer_frame(bt);

				// Host switched to digitizer mode mid-gesture: drop emulated clicks
				const bool had_buttons = this->emulated_buttons != 0;
				this->emulated_buttons = 0;
				this->reset_emulation();
				return had_buttons;
			}

			// Lift every contact and button so nothing stays pressed on the host.
			void release(BlueZ_Interface& bt)
			{
				for (Slot& slot : this->slots)
					this->set_contact(slot, false, -1);
				this->buttons = 0;
				this->emulated_buttons = 0;
				this->reset_emulation();
				this->dirty = false;

				if (this->in_mouse_mode())
				{
					for (Slot& slot : this->slots)
						slot.lifting = false;
				}
				else
				{
					this->send_digitizer_frame(bt);
				}
			}
	};
}

static std::vector<Touch_Tracker> s_touch_trackers;
static bool s_hid_processor_installed = false;

static Touch_Tracker* find_touch_tracker(unsigned source_id)
{
	for (Touch_Tracker& tracker : s_touch_trackers)
		if (tracker.source() == source_id)
			return &tracker;
	return nullptr;
}


// ─── Touch device discovery ──────────────────────────────────────────────────
static const struct input_absinfo* find_absinfo(const Device_Capabilities& caps, unsigned code)
{
	for (const auto& [abs_code, info] : caps.absinfo)
		if (abs_code == code)
			return &info;
	return nullptr;
}

// Classifies every input device (same rules as libinput) and describes each
// touchpad and touchscreen found as a digitizer for the report map generator.
static std::vector<hid::Digitizer_Spec> discover_digitizers()
{
	std::vector<hid::Digitizer_Spec> specs;

	for (const Device_Capabilities& caps : Device::return_device_capabilities())
	{
		auto has_key  = [&](unsigned code) { return caps.key_codes.contains(code); };
		auto has_prop = [&](unsigned prop) { return caps.properties.contains(prop); };

		// Touch surfaces report BTN_TOUCH. Pens/tablets and accelerometers are out of scope.
		if (!has_key(BTN_TOUCH) || has_key(BTN_TOOL_PEN) || has_key(BTN_STYLUS) || has_prop(INPUT_PROP_ACCELEROMETER))
			continue;

		hid::Digitizer_Spec spec;
		spec.source_id = caps.id;
		spec.name = caps.name;
		spec.kind = (!has_prop(INPUT_PROP_DIRECT) && (has_key(BTN_TOOL_FINGER) || has_prop(INPUT_PROP_POINTER)))
			? hid::Digitizer_Kind::Touchpad
			: hid::Digitizer_Kind::Touchscreen;

		const struct input_absinfo* mt_x    = find_absinfo(caps, ABS_MT_POSITION_X);
		const struct input_absinfo* mt_y    = find_absinfo(caps, ABS_MT_POSITION_Y);
		const struct input_absinfo* mt_slot = find_absinfo(caps, ABS_MT_SLOT);
		const struct input_absinfo* st_x    = find_absinfo(caps, ABS_X);
		const struct input_absinfo* st_y    = find_absinfo(caps, ABS_Y);

		const struct input_absinfo* abs_x = nullptr;
		const struct input_absinfo* abs_y = nullptr;
		if (mt_x && mt_y && mt_slot)
		{
			spec.multitouch = true;
			spec.slot_count = static_cast<uint8_t>(std::clamp<int>(mt_slot->maximum + 1, 1, hid::MAX_SLOT_COUNT));
			abs_x = mt_x;
			abs_y = mt_y;
		}
		else if (st_x && st_y)	// Single-touch, or legacy MT protocol A (no slots): use the pointer axes
		{
			spec.multitouch = false;
			spec.slot_count = 1;
			abs_x = st_x;
			abs_y = st_y;
		}
		else
		{
			continue;
		}

		// Some drivers only set a resolution on ABS_X/ABS_Y, not on the MT axes
		auto to_axis = [](const struct input_absinfo& info, const struct input_absinfo* fallback)
		{
			hid::Axis_Spec axis{ info.minimum, info.maximum, info.resolution };
			if (axis.resolution <= 0 && fallback != nullptr)
				axis.resolution = fallback->resolution;
			return axis;
		};
		spec.x = to_axis(*abs_x, st_x);
		spec.y = to_axis(*abs_y, st_y);

		if (spec.kind == hid::Digitizer_Kind::Touchpad)
		{
			spec.max_contacts = static_cast<uint8_t>(std::min<unsigned>(spec.slot_count, MAX_TOUCHPAD_CONTACTS));
			spec.button_count = has_key(BTN_MIDDLE) ? 3 : has_key(BTN_RIGHT) ? 2 : 1;
			spec.pad_type = hid::pad_type::NON_DEPRESSIBLE;
#ifdef INPUT_PROP_PRESSUREPAD
			if (has_prop(INPUT_PROP_PRESSUREPAD))
			{
				spec.pad_type = hid::pad_type::PRESSURE;
				spec.button_count = 1;
			}
			else
#endif
			if (has_prop(INPUT_PROP_BUTTONPAD))
			{
				spec.pad_type = hid::pad_type::DEPRESSIBLE;
				spec.button_count = 1;
			}
		}
		else
		{
			spec.max_contacts = static_cast<uint8_t>(std::min<unsigned>(spec.slot_count, MAX_TOUCHSCREEN_CONTACTS));
		}

		specs.push_back(spec);
	}

	return specs;
}

// Generates the report map for the touch devices currently present and hands
// it to the BlueZ interface. Must run before BlueZ_Interface::enable(), since
// the report map is fixed once the GATT application is registered, and while
// process_hid_events is not installed (it reads s_touch_trackers).
static void configure_hid_report_map()
{
	if (unikey_bluetooth == nullptr || s_hid_processor_installed)
		return;

	hid::Report_Map report_map = hid::build_report_map(discover_digitizers());
	if (!unikey_bluetooth->set_hid_report_map(report_map))
		return;	// Already registered with an earlier map; keep the matching trackers

	s_touch_trackers.clear();
	for (const hid::Digitizer_Layout& layout : report_map.digitizers)
	{
		s_touch_trackers.emplace_back(layout);
		std::cout << "HID digitizer: \"" << layout.spec.name << "\" as "
			<< (layout.is_touchpad() ? "touchpad" : "touchscreen")
			<< " (report ID " << static_cast<int>(layout.input_report_id)
			<< ", " << static_cast<int>(layout.spec.max_contacts) << " contacts, "
			<< static_cast<int>(layout.contacts_per_report) << " per report)" << std::endl;
	}
	for (const hid::Digitizer_Spec& spec : report_map.omitted)
	{
		std::cerr << "HID digitizer: \"" << spec.name << "\" omitted (invalid axes or report map full)" << std::endl;
	}
	std::cout << "HID report map: " << report_map.descriptor.size() << " bytes" << std::endl;
}


// ─── Input event processor ───────────────────────────────────────────────────
static void send_mouse_report()
{
	hid_mouse_report report = s_mouse;
	for (const Touch_Tracker& tracker : s_touch_trackers)
		report.buttons |= tracker.mouse_buttons();
	unikey_bluetooth->send_hid_report(hid::MOUSE_REPORT_ID, &report, sizeof(report));
}

static void handle_key_event(const struct input_event& ev, bool& mouse_dirty, bool& keyboard_dirty, bool& consumer_dirty)
{
	// ── Mouse buttons ─────────────────────────────────────────────────────
	if (ev.code >= BTN_MOUSE && ev.code < BTN_MOUSE + 32)
	{
		uint32_t bit = 1u << (ev.code - BTN_MOUSE);
		if (ev.value) s_mouse.buttons |=  bit;
		else          s_mouse.buttons &= ~bit;
		mouse_dirty = true;
		return;
	}

	// ── Consumer control keys ─────────────────────────────────────────────
	// Checked before the keyboard map: media/brightness/volume keys
	// are not in the keyboard descriptor's usage range 0x00–0x67.
	uint16_t consumer_code = keymap_linux_to_consumer(ev.code);
	if (consumer_code)
	{
		// value 1 = press, 2 = repeat (treat as press), 0 = release
		if (ev.value)
			s_consumer.usage_code = consumer_code;
		else if (s_consumer.usage_code == consumer_code)
			s_consumer.usage_code = 0x0000;
		consumer_dirty = true;
		return;
	}

	// ── Keyboard keys ─────────────────────────────────────────────────────
	// The keymap only covers Linux codes 0x00–0xFF. Higher codes (BTN_TOUCH,
	// BTN_TOOL_*, KEY_* ≥ 0x100) must not be truncated to 8 bits, or e.g.
	// BTN_TOUCH (0x14A) would alias KEY_KPMINUS (0x4A).
	if (ev.code > 0xFF)
		return;

	uint8_t hid = keymap_linux_to_hid(static_cast<uint8_t>(ev.code));
	if (hid == 0x00)
		return;

	if (hid >= 0xE0 && hid <= 0xE7)
	{
		// Modifier bitmap: LCtrl=bit0 … RMeta=bit7
		uint8_t bit = static_cast<uint8_t>(1u << (hid - 0xE0));
		if (ev.value) s_keyboard.modifiers |=  bit;
		else          s_keyboard.modifiers &= ~bit;
	}
	else
	{
		// Key presence bitmap across keys[0..12].
		// The descriptor covers usages 0x00–0x67 (104 bits = 13 bytes,
		// Var|Abs). Bit N of the bitmap = usage 0x00+N.
		// Usage 0x00 is "no event" and is never set intentionally.
		// Regular key codes above 0x67 (e.g. F13–F24, keypad parens) are
		// outside the bitmap and are dropped rather than written out of bounds.
		if (hid > 0x67)
			return;
		uint8_t byte_idx = hid / 8;
		uint8_t bit_mask = static_cast<uint8_t>(1u << (hid % 8));
		if (ev.value) s_keyboard.keys[byte_idx] |=  bit_mask;
		else          s_keyboard.keys[byte_idx] &= ~bit_mask;
	}
	keyboard_dirty = true;
}

static void handle_rel_event(const struct input_event& ev, bool& mouse_dirty)
{
	switch (ev.code)
	{
		case REL_X:      s_mouse.x      = saturate<int16_t>(ev.value); break;
		case REL_Y:      s_mouse.y      = saturate<int16_t>(ev.value); break;
		case REL_WHEEL:  s_mouse.wheel  = saturate<int8_t>(ev.value);  break;
		case REL_HWHEEL: s_mouse.hwheel = saturate<int8_t>(ev.value);  break;
		default: return;	// e.g. REL_WHEEL_HI_RES: no field in the report
	}
	mouse_dirty = true;
}

// Shared between dbus_enable_unikey_bluetooth and dbus_toggle_unikey_bluetooth
// so neither duplicates the event handling logic.
//
// Each batch comes from a single input device. If that device is one of the
// generated digitizers, its touch events are routed to its Touch_Tracker,
// which sends the device's own digitizer report (or emulates the mouse);
// all remaining events go through the keyboard/mouse/consumer handlers.
static void process_hid_events(const void* data, uint64_t /*unit_size*/)
{
	const uint64_t* p_count = static_cast<const uint64_t*>(data);
	const struct input_event* events = reinterpret_cast<const struct input_event*>(p_count + 1);
	const uint64_t count = std::min<uint64_t>(*p_count, Device::EVENT_BATCH_CAPACITY);

	Touch_Tracker* tracker = find_touch_tracker(Device::return_batch_source(data));

	// Relative axes accumulate per batch then reset.
	s_mouse.x      = 0;
	s_mouse.y      = 0;
	s_mouse.wheel  = 0;
	s_mouse.hwheel = 0;

	bool mouse_dirty    = false;
	bool keyboard_dirty = false;
	bool consumer_dirty = false;

	for (uint64_t n = 0; n < count; ++n)
	{
		const struct input_event& ev = events[n];

		if (tracker != nullptr && tracker->consume(ev))
			continue;

		switch (ev.type)
		{
			case EV_KEY: 
				if (ev.code == KEY_POWER)
				{
					continue;
				}
				handle_key_event(ev, mouse_dirty, keyboard_dirty, consumer_dirty);
				break;

			case EV_REL: handle_rel_event(ev, mouse_dirty);
				break;
		}
	}

	if (tracker != nullptr && tracker->flush(*unikey_bluetooth, s_mouse))
		mouse_dirty = true;

	if (mouse_dirty)
		send_mouse_report();

	if (keyboard_dirty)
		unikey_bluetooth->send_hid_report(hid::KEYBOARD_REPORT_ID, &s_keyboard, sizeof(s_keyboard));

	if (consumer_dirty)
		unikey_bluetooth->send_hid_report(hid::CONSUMER_REPORT_ID, &s_consumer, sizeof(s_consumer));
}

static void install_hid_processor()
{
	s_hid_processor_installed = true;
	Device::set_event_processor(process_hid_events);
}

// Blocks until any in-flight batch has been processed, so the report state
// below is no longer touched by the watchdog thread once this returns.
static void uninstall_hid_processor()
{
	Device::set_event_processor();	// Reset to default
	s_hid_processor_installed = false;
}

// Send released states for every report so the host sees nothing held down
// before we tear down the connection.
static void release_all_inputs()
{
	if (!unikey_bluetooth) return;

	for (Touch_Tracker& tracker : s_touch_trackers)
		tracker.release(*unikey_bluetooth);

	s_keyboard = hid_keyboard_report{};
	s_consumer = hid_consumer_report{};
	s_mouse = hid_mouse_report{};

	unikey_bluetooth->send_hid_report(hid::KEYBOARD_REPORT_ID, &s_keyboard, sizeof(s_keyboard));
	unikey_bluetooth->send_hid_report(hid::CONSUMER_REPORT_ID, &s_consumer, sizeof(s_consumer));
	send_mouse_report();
}


// ─── D-Bus interface ──────────────────────────────────────────────────────────

void register_bluetooth_dbus_cmds()
{
	unikey_bluetooth_dbus_obj = sdbus::createObject(*unikey_dbus_connection, "/io/unikey/Bluetooth");
	
	unikey_bluetooth_dbus_obj->registerMethod("io.unikey.Bluetooth.Methods",
		"SetDisplayName", "s", "", &dbus_set_bluetooth_name);

	unikey_bluetooth_dbus_obj->registerMethod("EnableBluetooth")
		.onInterface("io.unikey.Bluetooth.Methods")
			.implementedAs(&dbus_enable_unikey_bluetooth);

	unikey_bluetooth_dbus_obj->registerMethod("DisableBluetooth")
		.onInterface("io.unikey.Bluetooth.Methods")
			.implementedAs(&dbus_disable_unikey_bluetooth);
	
	unikey_bluetooth_dbus_obj->registerMethod("ToggleBluetooth")
		.onInterface("io.unikey.Bluetooth.Methods")
			.implementedAs(&dbus_toggle_unikey_bluetooth);

	unikey_bluetooth_dbus_obj->finishRegistration();
}

void dbus_set_bluetooth_name(sdbus::MethodCall call)
{
	std::string bluetooth_device_name;
	call >> bluetooth_device_name;
	call.createReply().send();

	if (bluetooth_device_name.empty())
	{
		return;
	}
	else if (unikey_bluetooth == nullptr)
	{
		unikey_bluetooth = new BlueZ_Interface(bluetooth_device_name, unikey_dbus_connection, "io.unikey");
	}
	else
	{
		unikey_bluetooth->set_device_name(bluetooth_device_name);
	}
}

void dbus_enable_unikey_bluetooth()
{
	if (unikey_bluetooth == nullptr)
	{
		unikey_bluetooth = new BlueZ_Interface("Unikey HID", unikey_dbus_connection, "io.unikey");
	}

	configure_hid_report_map();

	if (unikey_bluetooth->enable())
	{
		install_hid_processor();
	}
	else
	{
		delete unikey_bluetooth;
		unikey_bluetooth = nullptr;
	}
}

void dbus_disable_unikey_bluetooth()
{
	uninstall_hid_processor();
	release_all_inputs();
	delete unikey_bluetooth;
	unikey_bluetooth = nullptr;
}

void dbus_toggle_unikey_bluetooth()
{
	if (unikey_bluetooth == nullptr)
	{
		unikey_bluetooth = new BlueZ_Interface("Unikey HID", unikey_dbus_connection, "io.unikey");
		configure_hid_report_map();

		if (unikey_bluetooth->enable())
		{
			if (Device::return_grab_state())
			{
				Device::trigger_activation();
			}

			install_hid_processor();
			return;
		}
	}
	else
	{
		if (Device::return_grab_state())
		{
			Device::trigger_activation();
		}

		uninstall_hid_processor();
		release_all_inputs();
	}

	delete unikey_bluetooth;
	unikey_bluetooth = nullptr;
}



// ─── Linux → HID keymap ──────────────────────────────────────────────────────
/*
	Based on HID usage tables version 1.21 specified
	at https://usb.org/sites/default/files/hut1_21.pdf

	Only covers the keyboard descriptor's usage range 0x01–0x67 for regular
	keys and 0xE0–0xE7 for modifiers. Consumer control keys (media, volume,
	brightness) return 0x00 here; they are handled by keymap_linux_to_consumer
	and routed to report ID 3 instead.
*/
uint8_t keymap_linux_to_hid(uint8_t linux_key)
{
	static std::vector<uint8_t> keymap;

	if (keymap.empty())
	{
		keymap.resize(256, 0x00);
		keymap[KEY_RESERVED] = 0x00;

		// Alphanumeric
		keymap[KEY_A] = 0x04;
		keymap[KEY_B] = 0x05;
		keymap[KEY_C] = 0x06;
		keymap[KEY_D] = 0x07;
		keymap[KEY_E] = 0x08;
		keymap[KEY_F] = 0x09;
		keymap[KEY_G] = 0x0a;
		keymap[KEY_H] = 0x0b;
		keymap[KEY_I] = 0x0c;
		keymap[KEY_J] = 0x0d;
		keymap[KEY_K] = 0x0e;
		keymap[KEY_L] = 0x0f;
		keymap[KEY_M] = 0x10;
		keymap[KEY_N] = 0x11;
		keymap[KEY_O] = 0x12;
		keymap[KEY_P] = 0x13;
		keymap[KEY_Q] = 0x14;
		keymap[KEY_R] = 0x15;
		keymap[KEY_S] = 0x16;
		keymap[KEY_T] = 0x17;
		keymap[KEY_U] = 0x18;
		keymap[KEY_V] = 0x19;
		keymap[KEY_W] = 0x1a;
		keymap[KEY_X] = 0x1b;
		keymap[KEY_Y] = 0x1c;
		keymap[KEY_Z] = 0x1d;
		keymap[KEY_1] = 0x1e;
		keymap[KEY_2] = 0x1f;
		keymap[KEY_3] = 0x20;
		keymap[KEY_4] = 0x21;
		keymap[KEY_5] = 0x22;
		keymap[KEY_6] = 0x23;
		keymap[KEY_7] = 0x24;
		keymap[KEY_8] = 0x25;
		keymap[KEY_9] = 0x26;
		keymap[KEY_0] = 0x27;

		// Function
		keymap[KEY_F1]  = 0x3a;
		keymap[KEY_F2]  = 0x3b;
		keymap[KEY_F3]  = 0x3c;
		keymap[KEY_F4]  = 0x3d;
		keymap[KEY_F5]  = 0x3e;
		keymap[KEY_F6]  = 0x3f;
		keymap[KEY_F7]  = 0x40;
		keymap[KEY_F8]  = 0x41;
		keymap[KEY_F9]  = 0x42;
		keymap[KEY_F10] = 0x43;
		keymap[KEY_F11] = 0x44;
		keymap[KEY_F12] = 0x45;
		keymap[KEY_F13] = 0x68;
		keymap[KEY_F14] = 0x69;
		keymap[KEY_F15] = 0x6a;
		keymap[KEY_F16] = 0x6b;
		keymap[KEY_F17] = 0x6c;
		keymap[KEY_F18] = 0x6d;
		keymap[KEY_F19] = 0x6e;
		keymap[KEY_F20] = 0x6f;
		keymap[KEY_F21] = 0x70;
		keymap[KEY_F22] = 0x71;
		keymap[KEY_F23] = 0x72;
		keymap[KEY_F24] = 0x73;

		// Miscellaneous
		keymap[KEY_POWER]      = 0x66;
		keymap[KEY_GRAVE]      = 0x35;
		keymap[KEY_ESC]        = 0x29;
		keymap[KEY_ENTER]      = 0x28;
		keymap[KEY_SPACE]      = 0x2c;
		keymap[KEY_INSERT]     = 0x49;
		keymap[KEY_DELETE]     = 0x4c;
		keymap[KEY_HOME]       = 0x4a;
		keymap[KEY_PAGEUP]     = 0x4b;
		keymap[KEY_END]        = 0x4d;
		keymap[KEY_PAGEDOWN]   = 0x4e;
		keymap[KEY_SEMICOLON]  = 0x33;
		keymap[KEY_APOSTROPHE] = 0x34;
		keymap[KEY_MINUS]      = 0x2d;
		keymap[KEY_EQUAL]      = 0x2e;
		keymap[KEY_BACKSPACE]  = 0x2a;
		keymap[KEY_TAB]        = 0x2b;
		keymap[KEY_CAPSLOCK]   = 0x39;
		keymap[KEY_COMMA]      = 0x36;
		keymap[KEY_DOT]        = 0x37;
		keymap[KEY_SLASH]      = 0x38;
		keymap[KEY_BACKSLASH]  = 0x31;
		keymap[KEY_SYSRQ]      = 0x46;
		keymap[KEY_NUMLOCK]    = 0x53;
		keymap[KEY_SCROLLLOCK] = 0x47;
		keymap[KEY_LEFTBRACE]  = 0x2f;
		keymap[KEY_RIGHTBRACE] = 0x30;
		keymap[KEY_UP]         = 0x52;
		keymap[KEY_DOWN]       = 0x51;
		keymap[KEY_LEFT]       = 0x50;
		keymap[KEY_RIGHT]      = 0x4f;

		// NOTE: KEY_MUTE, KEY_VOLUMEUP, KEY_VOLUMEDOWN are intentionally NOT
		// mapped here. Their original HID keyboard-page codes (0x7F-0x81)
		// exceed the descriptor bitmap's Usage Maximum (0x67) and would write
		// out-of-bounds into keys[]. They are handled in keymap_linux_to_consumer
		// and sent via report ID 3 (Consumer Control).

		// Modifiers — returned as 0xE0–0xE7; handled via the modifier byte
		keymap[KEY_LEFTCTRL]   = 0xe0;
		keymap[KEY_LEFTSHIFT]  = 0xe1;
		keymap[KEY_LEFTALT]    = 0xe2;
		keymap[KEY_LEFTMETA]   = 0xe3;
		keymap[KEY_RIGHTCTRL]  = 0xe4;
		keymap[KEY_RIGHTSHIFT] = 0xe5;
		keymap[KEY_RIGHTALT]   = 0xe6;
		keymap[KEY_RIGHTMETA]  = 0xe7;

		// Keypad
		keymap[KEY_KP1] = 0x59;
		keymap[KEY_KP2] = 0x5a;
		keymap[KEY_KP3] = 0x5b;
		keymap[KEY_KP4] = 0x5c;
		keymap[KEY_KP5] = 0x5d;
		keymap[KEY_KP6] = 0x5e;
		keymap[KEY_KP7] = 0x5f;
		keymap[KEY_KP8] = 0x60;
		keymap[KEY_KP9] = 0x61;
		keymap[KEY_KP0] = 0x62;

		// Keypad miscellaneous
		keymap[KEY_KPSLASH]      = 0x54;
		keymap[KEY_KPASTERISK]   = 0x55;
		keymap[KEY_KPMINUS]      = 0x56;
		keymap[KEY_KPPLUS]       = 0x57;
		keymap[KEY_KPENTER]      = 0x58;
		keymap[KEY_KPDOT]        = 0x63;
		keymap[KEY_KPEQUAL]      = 0x67;
		keymap[KEY_KPLEFTPAREN]  = 0xb6;
		keymap[KEY_KPRIGHTPAREN] = 0xb7;
		keymap[KEY_KPPLUSMINUS]  = 0xd7;
	}
	
	return keymap[linux_key];
}


// ─── Linux → HID Consumer page keymap ────────────────────────────────────────
/*
	Maps Linux EV_KEY media/consumer codes to 16-bit HID Consumer page usage
	codes (HUT 1.21 §15). The consumer report (ID 3) is a single-usage Array
	field: send the usage on press, 0x0000 on release.
*/
uint16_t keymap_linux_to_consumer(uint16_t linux_key)
{
	switch (linux_key)
	{
		// Transport controls
		case KEY_PLAYPAUSE:    return 0x00CD;	// Play/Pause
		case KEY_STOPCD:       return 0x00B7;	// Stop
		case KEY_NEXTSONG:     return 0x00B5;	// Scan Next Track
		case KEY_PREVIOUSSONG: return 0x00B6;	// Scan Previous Track
		case KEY_FASTFORWARD:  return 0x00B3;	// Fast Forward
		case KEY_REWIND:       return 0x00B4;	// Rewind
		case KEY_EJECTCD:      return 0x00B8;	// Eject

		// Volume and audio
		case KEY_MUTE:         return 0x00E2;	// Mute
		case KEY_VOLUMEUP:     return 0x00E9;	// Volume Increment
		case KEY_VOLUMEDOWN:   return 0x00EA;	// Volume Decrement

		// Display brightness
		case KEY_BRIGHTNESSUP:   return 0x006F;	// Display Brightness Increment
		case KEY_BRIGHTNESSDOWN: return 0x0070;	// Display Brightness Decrement

		// Application launch
		case KEY_MEDIA:    return 0x0183;	// AL Media Select
		case KEY_MAIL:     return 0x018A;	// AL Email Reader
		case KEY_CALC:     return 0x0192;	// AL Calculator

		// Browser / navigation
		case KEY_HOMEPAGE: return 0x0223;	// AC Home
		case KEY_SEARCH:   return 0x0221;	// AC Search
		case KEY_BACK:     return 0x0224;	// AC Back
		case KEY_FORWARD:  return 0x0225;	// AC Forward
		case KEY_REFRESH:  return 0x0227;	// AC Refresh
		case KEY_STOP:     return 0x0226;	// AC Stop

		default: return 0x0000;
	}
}