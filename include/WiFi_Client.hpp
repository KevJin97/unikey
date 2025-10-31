#ifndef WIFI_CLIENT_HPP
#define WIFI_CLIENT_HPP

#include <atomic>
#include <stdint.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

class WiFi_Client
{
	private:
		int client_socket=socket(AF_INET, SOCK_STREAM, 0);
		struct sockaddr_in server_addr;
		std::atomic_bool connected_to_server=false;

	public:
		WiFi_Client() = default;
		WiFi_Client(const WiFi_Client&) = delete;
		WiFi_Client(const char* ip_addr, uint16_t port_num=42069);
		~WiFi_Client();

		void set_server_addr(const char* ip_addr, uint16_t port_num=42069);
		bool server_connection_status() const;
		void send_formatted_data(const void* data , uint64_t data_unit_size=0) const;
		void send_unformatted_data(const void* data, uint64_t data_unit_size=0, uint64_t length=1) const;
		void connect_to_server();
		void connect_to_server(const char* ip_addr, uint16_t port_num=42069);
		void wait_until_connected();
		void close_connection();

		WiFi_Client& operator= (const WiFi_Client&) = delete;
};

#endif	// WIFI_CLIENT_HPP