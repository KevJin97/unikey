#ifndef UNIKEY_BLUETOOTH_HPP
#define UNIKEY_BLUETOOTH_HPP

#include <cstdint>
#include <stdint.h>

#include <sdbus-c++/IObject.h>
#include <sdbus-c++/Message.h>

extern void register_bluetooth_dbus_cmds();
extern void dbus_set_bluetooth_name(sdbus::MethodCall call);
extern void dbus_enable_unikey_bluetooth();
extern void dbus_disable_unikey_bluetooth();
extern void dbus_toggle_unikey_bluetooth();
extern uint8_t keymap_linux_to_hid(uint8_t linux_key);
extern uint16_t keymap_linux_to_consumer(uint16_t linux_key);

#endif	// UNIKEY_BLUETOOTH_HPP