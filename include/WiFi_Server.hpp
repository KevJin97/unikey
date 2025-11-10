#ifndef WIFI_SERVER_HPP
#define WIFI_SERVER_HPP

#include <atomic>
#include <cstdint>
#include <stdint.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

class WiFi_Server
{
	private:
		int server_socket = -1;
		int client_socket = -1;
		struct sockaddr_in server_addr;
		std::atomic_bool is_connected = false;
		std::atomic_uint64_t blocks_left = 0;
		std::atomic_uint64_t block_size = 0;

	public:
		WiFi_Server();
		WiFi_Server(uint16_t port_num);

		~WiFi_Server();

		const WiFi_Server& init_server(uint16_t port_num=42069);
		const WiFi_Server& begin_listening();
		const WiFi_Server& wait_for_connection() const;
		bool is_connected_to_client() const;
		void* read_sent_data(void* p_data=nullptr);
		void close_connection();
};

#endif	// WIFI_SERVER_HPP