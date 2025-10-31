#ifndef WIFI_SERVER_HPP
#define WIFI_SERVER_HPP

#include <atomic>
#include <stdint.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

class WiFi_Server
{
	private:
		int server_socket=socket(AF_INET, SOCK_STREAM, 0);
		int client_socket=-1;
		struct sockaddr_in server_addr;
		std::atomic_bool is_connected=false;

		void listen_loop(void (*process_data));

	public:
		WiFi_Server();
		WiFi_Server(uint16_t port_num);

		~WiFi_Server();

		void set_port_num(uint16_t port_num);
		bool wait_for_connection();
		void* read_sent_data();
};

#endif	// WIFI_SERVER_HPP