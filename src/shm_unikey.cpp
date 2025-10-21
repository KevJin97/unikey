#include "shm_unikey.hpp"

#include <cstddef>
#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

void* shm_initialize(const std::string& shm_name, std::size_t shm_size, std::size_t mem_offset)
{
	int shm_fd = -1;
	void* shm_addr = nullptr;
	
	if ((shm_fd = shm_open(shm_name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0660)) == -1)
	{
		perror("shm_open (Initialization)");
		std::cerr << "Error: Failed to initialize shared memory block \"" << shm_name << "\"" << std::endl;
	}
	else if (ftruncate(shm_fd, shm_size) == -1)
	{
		close(shm_fd);
		shm_unlink(shm_name.c_str());
	}
	else if ((shm_addr = mmap(nullptr, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, mem_offset)) == MAP_FAILED)
	{
		close(shm_fd);
		shm_unlink(shm_name.c_str());
	}
	else
	{
		close(shm_fd);
		return shm_addr;
	}

	return nullptr;
}

void* shm_ptr_addr(const std::string& shm_name, std::size_t shm_size, std::size_t mem_offset)
{
	int shm_fd = -1;
	void* shm_addr = nullptr;

	if ((shm_fd = shm_open(shm_name.c_str(), O_RDWR, 0660)) == -1)
	{
		return shm_initialize(shm_name, shm_size);
	}
	else if ((shm_addr = mmap(nullptr, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, mem_offset)) == MAP_FAILED)
	{
		close(shm_fd);
		return nullptr;
	}
	else
	{
		close(shm_fd);
		return shm_addr;
	}
}
