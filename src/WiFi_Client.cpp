#include "WiFi_Client.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>

#include <thread>
#include <unistd.h>

WiFi_Client::WiFi_Client(const char* ip_addr, uint16_t port_num)
{
	this->connect_to_server(ip_addr, port_num);
	/*
		std::vector<uint64_t> global_bitfield = Device::return_enabled_global_key_states().return_vector();
		send(this->client_socket, global_bitfield.data(), sizeof(uint64_t) * global_bitfield.size(), 0);
		global_bitfield = Device::return_enabled_global_rel_states().return_vector();
		send(this->client_socket, global_bitfield.data(), sizeof(uint64_t) * global_bitfield.size(), 0);
	*/
}

WiFi_Client::~WiFi_Client()
{
	this->close_connection();
}

void WiFi_Client::set_server_addr(const char* ip_addr, uint16_t port_num)
{
	this->server_addr = { .sin_family = AF_INET, .sin_port = htons(port_num) };
	inet_pton(AF_INET, ip_addr, &(this->server_addr.sin_addr));
}

bool WiFi_Client::server_connection_status() const
{
	return this->connected_to_server.load(std::memory_order_acquire);
}

void WiFi_Client::send_formatted_data(const void* formatted_data, uint64_t data_unit_size) const
{
	// Data should be formatted in the form of (uint64_t, struct[])
	if (!this->connected_to_server.load(std::memory_order_acquire)) return;
	else if (!(formatted_data || data_unit_size))
	{
		send(this->client_socket, &data_unit_size, sizeof(uint64_t), 0);
	}
	else if (formatted_data == nullptr) return;
	else
	{
		send(this->client_socket, &data_unit_size, sizeof(uint64_t), 0);
		send(this->client_socket, formatted_data, sizeof(uint64_t) + *(uint64_t*)formatted_data * data_unit_size, 0);
	}
}

void WiFi_Client::send_unformatted_data(const void* data, uint64_t data_unit_size, uint64_t length) const
{
	if (!this->connected_to_server.load(std::memory_order_acquire)) return;
	else if (!(data || data_unit_size || length))
	{
		send(this->client_socket, &data_unit_size, sizeof(uint64_t), 0);
	}
	else if (data == nullptr) return;
	else
	{
		send(this->client_socket, &data_unit_size, sizeof(uint64_t), 0);
		send(this->client_socket, &length, sizeof(uint64_t), 0);
		send(this->client_socket, data, data_unit_size * length, 0);
	}
}

void WiFi_Client::connect_to_server()
{
	if (this->client_socket == -1)
	{
		this->client_socket = socket(AF_INET, SOCK_STREAM, 0);
	}

	std::thread connecting_thread([&] {
		uint8_t fail_count = 0;

		while (this->client_socket != -1)
		{
			if (fail_count == 5)
			{
				fail_count = 0;
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			else if (connect(this->client_socket, (struct sockaddr*)&this->server_addr, sizeof(this->server_addr)) < 0)
			{
				++fail_count;
			}
			else
			{
				this->connected_to_server.store(true, std::memory_order_release);
				this->connected_to_server.notify_all();
				this->connected_to_server.wait(true, std::memory_order_acquire);
			}
		}
	});
	connecting_thread.detach();
}

void WiFi_Client::connect_to_server(const char* ip_addr, uint16_t port_num)
{
	this->set_server_addr(ip_addr, port_num);
	this->connect_to_server();
}

void WiFi_Client::wait_until_connected()
{
	this->connected_to_server.wait(false, std::memory_order_acquire);
}

void WiFi_Client::close_connection()
{
	if (this->client_socket != -1)
	{
		close(this->client_socket);
		this->client_socket = -1;

		if (this->connected_to_server.load(std::memory_order_acquire) == true)
		{
			this->connected_to_server.store(false, std::memory_order_release);
			this->connected_to_server.notify_one();
		}
		else
		{
			this->connected_to_server.store(true, std::memory_order_release);
			this->connected_to_server.notify_all();
		}
	}
}