#ifndef UNIKEY_WIFI_HPP
#define UNIKEY_WIFI_HPP

#include <sdbus-c++/Message.h>

extern void register_wifi_dbus_cmds();
extern void dbus_connect_to_ip(sdbus::MethodCall call);
extern void dbus_toggle_unikey_server();

#endif	// UNIKEY_WIFI_HPP