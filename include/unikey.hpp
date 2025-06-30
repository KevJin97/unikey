#ifndef UNIKEY_HPP
#define UNIKEY_HPP

#include "BitField.hpp"
#include "Device.hpp"
//#include "Virtual_Device.hpp"

inline std::atomic_bool broadcast_service_status{false};

void broadcast_service();
void unikey_server();
void unikey_client();
int change_group_permissions();
int return_to_original_group_permissions(int gid);
std::vector<BitField> initialize_all_devices(std::vector<Device*>& devices);


#endif // UNIKEY_HPP
