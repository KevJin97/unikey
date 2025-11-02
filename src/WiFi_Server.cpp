#include "WiFi_Server.hpp"

#include <atomic>
#include <cstdint>
#include <stdlib.h>
#include <thread>

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
	this->server_addr.sin_addr.s_addr = INADDR_ANY;
	
	this->init_server(port_num);
}

WiFi_Server::~WiFi_Server()
{
	if (this->client_socket != -1)
		this->close_connection();

	if (this->server_socket != -1)
		close(this->server_socket);
}

const WiFi_Server& WiFi_Server::init_server(uint16_t port_num)
{
	if (this->is_connected.load(std::memory_order_acquire))
		return *this;

	if (this->server_socket != -1)
		close(this->server_socket);

	this->server_socket = socket(AF_INET, SOCK_STREAM, 0);
	this->server_addr.sin_port = htons(port_num);
	if (bind(this->server_socket, (struct sockaddr*)&this->server_addr, sizeof(this->server_addr)) < 0)
	{
		perror("Bind Failed");
		return *this;
	}

	return *this;
}

const WiFi_Server& WiFi_Server::begin_listening()
{
	if (listen(this->server_socket, 5) < 0)
	{
		perror("Listen Failed");
		return *this;
	}
	std::thread listen_for_client([&]
	{
		if ((this->client_socket = accept(this->server_socket, (struct sockaddr*)&this->server_addr, (socklen_t*)&this->server_addr)) < 0)
		{
			perror("Client Connection Not Accepted");
		}
		else
		{
			this->is_connected.store(true, std::memory_order_release);
			this->is_connected.notify_all();
		}
	});
	listen_for_client.detach();

	return *this;
}

const WiFi_Server& WiFi_Server::wait_for_connection() const
{
	this->is_connected.wait(false, std::memory_order_acquire);
	return *this;
}

bool WiFi_Server::is_connected_to_client() const
{
	return this->is_connected.load(std::memory_order_acquire);
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
			this->close_connection();
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

void WiFi_Server::close_connection()
{
	if (this->client_socket != -1)
	{
		close(this->client_socket);
		this->client_socket = -1;
	}

	this->is_connected.store(false, std::memory_order_release);
}