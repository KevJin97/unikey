#include "WiFi_Server.hpp"
#include <atomic>
#include <stdlib.h>

#include <sys/socket.h>

WiFi_Server::WiFi_Server()
{
	this->server_addr.sin_family = AF_INET;
	this->server_addr.sin_port = 0;
	this->server_addr.sin_addr.s_addr = INADDR_ANY;
}

WiFi_Server::WiFi_Server(uint16_t port_num)
{
	this->server_addr.sin_family = AF_INET;
	this->server_addr.sin_port = htons(port_num);
	this->server_addr.sin_addr.s_addr = INADDR_ANY;

	bind(this->server_socket, (struct sockaddr*)&this->server_addr, sizeof(this->server_addr));
}

WiFi_Server::~WiFi_Server()
{
	if (this->client_socket != -1)
		close(this->client_socket);

	close(this->server_socket);
}

void WiFi_Server::set_port_num(uint16_t port_num)
{
	this->server_addr.sin_port = htons(port_num);
	bind(this->server_socket, (struct sockaddr*)&this->server_addr, sizeof(this->server_addr));
}

bool WiFi_Server::wait_for_connection()
{
	if (listen(this->server_socket, 5) == 0)
	{
		this->client_socket = accept(this->server_socket, nullptr, nullptr);
		this->is_connected.store(true, std::memory_order_release);
		this->is_connected.notify_all();
		return true;
	}
	else
	{
		return false;
	}
}

void* WiFi_Server::read_sent_data()
{
	void* p_data = nullptr;
	uint64_t num_bytes = 0;
	if (this->is_connected.load(std::memory_order_acquire))
	{
		uint64_t bytes = 0;
		recv(this->client_socket, &bytes, sizeof(uint64_t), 0);
		if ((num_bytes = bytes) == 0)
		{
			this->is_connected.store(false, std::memory_order_release);
			close(this->client_socket);
			this->client_socket = -1;
			return p_data;
		}
		recv(this->client_socket, &bytes, sizeof(uint64_t), 0);
		num_bytes *= bytes;
		p_data = malloc(num_bytes + sizeof(uint64_t));
		*(uint64_t*)p_data = bytes;
		recv(this->client_socket, 1 + (uint64_t*)p_data, num_bytes, 0);
	}
	return p_data;
}