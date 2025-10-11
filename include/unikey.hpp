#ifndef UNIKEY_HPP
#define UNIKEY_HPP

#include "BitField.hpp"
#include "Device.hpp"
/*
#include "sdbus-c++/sdbus-c++.h"
#include <sdbus-c++/IProxy.h>

static inline const std::string SERVICE_INTERFACE = "org.freedesktop.NetworkManager";
static inline const std::string OBJECT_PATH = "/org/freedesktop/NetworkManager";

static inline auto connection = sdbus::createSystemBusConnection();
static inline auto manager_proxy = sdbus::createProxy(*connection, SERVICE_INTERFACE, OBJECT_PATH);
static std::vector<sdbus::ObjectPath> device_paths;


inline std::atomic_bool broadcast_service_status{false};
*/

extern void broadcast_service();
extern void unikey_server();
extern void unikey_client();
extern int change_group_permissions();
extern int return_to_original_group_permissions(int gid);

#endif // UNIKEY_HPP
