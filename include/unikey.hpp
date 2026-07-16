#ifndef UNIKEY_HPP
#define UNIKEY_HPP

#include "Core/Device.hpp"

#include <memory>

#include <linux/input.h>

#include <sdbus-c++/sdbus-c++.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IObject.h>
#include <sdbus-c++/Message.h>

extern std::unique_ptr<sdbus::IConnection> unikey_dbus_connection;

extern std::unique_ptr<sdbus::IObject> unikey_root_dbus_obj;
extern std::unique_ptr<sdbus::IObject> unikey_device_dbus_obj;
extern std::unique_ptr<sdbus::IObject> unikey_bluetooth_dbus_obj;
extern std::unique_ptr<sdbus::IObject> unikey_wifi_dbus_obj;

extern void register_to_dbus();
extern void register_device_dbus_cmds();
extern void dbus_trigger_cmd();
extern void dbus_set_timeout_cmd(sdbus::MethodCall);

// extern void broadcast_service();
extern int change_group_permissions();
extern int return_to_original_group_permissions(int gid);

#endif // UNIKEY_HPP
