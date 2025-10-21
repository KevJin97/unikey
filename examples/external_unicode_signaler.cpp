#include <atomic>
#include <string>

#include "shm_unikey.hpp"

int main(int argc, char* argv[])
{
	void* ptr = shm_ptr_addr("/shm_unikey", sizeof(std::atomic_bool) * 2);
	std::atomic_bool* event_signal = (std::atomic_bool*)ptr;
	if (argc == 2)
	{
		if (std::string(argv[1]) == "trigger")
		{
			event_signal[0].store(true, std::memory_order_release);
		}
		else if (std::string(argv[1]) == "exit")
		{
			event_signal[1].store(true, std::memory_order_release);
		}
	}

	return 0;
}
