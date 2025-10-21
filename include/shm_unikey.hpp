#ifndef SHM_UNIKEY_HPP
#define SHM_UNIKEY_HPP

#include <cstddef>
#include <string>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

void* shm_initialize(const std::string& shm_name, std::size_t shm_size, std::size_t mem_offset=0);
void* shm_ptr_addr(const std::string& shm_name, std::size_t shm_size, std::size_t mem_offset=0);

// To deallocate, munmap(void* shm_name, std::size_t shm_size);

#endif	// SHM_UNIKEY_HPP