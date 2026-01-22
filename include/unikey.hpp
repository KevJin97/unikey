#ifndef UNIKEY_HPP
#define UNIKEY_HPP

#include <linux/input.h>
#include <memory>
#include <sdbus-c++/sdbus-c++.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IObject.h>
#include <sdbus-c++/Message.h>

extern std::unique_ptr<sdbus::IConnection> unikey_dbus_connection;

extern std::unique_ptr<sdbus::IObject> unikey_root_dbus_obj;
extern std::unique_ptr<sdbus::IObject> unikey_device_dbus_obj;
extern std::unique_ptr<sdbus::IObject> unikey_wifi_dbus_obj;

extern void register_to_dbus();
extern void register_device_dbus_cmds();
extern void register_wifi_dbus_cmds();
extern void dbus_trigger_cmd();
extern void dbus_set_timeout_cmd(sdbus::MethodCall);
extern void dbus_connect_to_ip(sdbus::MethodCall);
extern void dbus_toggle_unikey_server();

// extern void broadcast_service();
extern int change_group_permissions();
extern int return_to_original_group_permissions(int gid);

#endif // UNIKEY_HPP
