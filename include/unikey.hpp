#ifndef UNIKEY_HPP
#define UNIKEY_HPP

#include "WiFi_Client.hpp"

#include <linux/input.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IObject.h>
#include <sdbus-c++/Message.h>
#include <sdbus-c++/sdbus-c++.h>

extern void dbus_trigger_cmd();
extern void dbus_set_timeout_cmd(sdbus::MethodCall);

static WiFi_Client messenger_wifi;
extern void send_formatted_data_wifi(const void* data, uint64_t unit_size=sizeof(struct input_event*));

// extern void broadcast_service();
extern void unikey_server();
extern void unikey_client();
extern int change_group_permissions();
extern int return_to_original_group_permissions(int gid);

#endif // UNIKEY_HPP
