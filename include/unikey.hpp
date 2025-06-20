#ifndef UNIKEY_HPP
#define UNIKEY_HPP

#include "BitField.hpp"
#include "Device.hpp"
//#include "Virtual_Device.hpp"

inline std::atomic_bool broadcast_service_status{false};

extern void broadcast_service();
extern void unikey_server();
extern void unikey_client();
extern int change_group_permissions();
extern int return_to_original_group_permissions(int gid);
extern std::vector<BitField> initialize_all_devices(std::vector<Device*>& devices);

#endif // UNIKEY_HPP
