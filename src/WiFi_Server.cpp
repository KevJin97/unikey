#include "WiFi_Server.hpp"

#include <atomic>
#include <cstdint>
#include <stdlib.h>
#include <thread>

#include <sys/socket.h>
#include <unistd.h>

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
		socklen_t addr_len = sizeof(this->server_addr);
		
		if ((this->client_socket = accept(this->server_socket, (struct sockaddr*)&this->server_addr, &addr_len)) < 0)
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

void* WiFi_Server::read_sent_data(void* p_data)
{	
	// Define a function that guarantees packet delivery of size min_bytes
	ssize_t (*recv_all)(int, void*, ssize_t, int) = [](int __fd, void* __buf, ssize_t min_bytes, int __flags)
	{
		uint8_t* p_buffer = (uint8_t*)__buf;
		ssize_t bytes_read = 0;
		ssize_t recv_value = 0;

		while (bytes_read < min_bytes)
		{
			if ((recv_value = recv(__fd, p_buffer + bytes_read, min_bytes - bytes_read, __flags)) > 0)
				bytes_read += recv_value;
			else
				return recv_value;
		}
		return min_bytes;
	};

	if (this->is_connected.load(std::memory_order_acquire))
	{
		uint64_t bytes = 0;

		if (this->blocks_left.load(std::memory_order_acquire) == 0)	// If previous recv was incomplete
		{
			if (recv_all(this->client_socket, &bytes, sizeof(bytes), 0) <= 0)	// Error or closed connection
			{
				return nullptr;
			}
			else if (bytes == 0)	// This is command that tells server to close connection
			{
				this->close_connection();
				return nullptr;
			}
			else
			{
				this->block_size.store(bytes, std::memory_order_release);	// First recv_all returns size of a single block
				recv_all(this->client_socket, &bytes, sizeof(bytes), 0);	// Second recv_all returns how many blocks
				this->blocks_left.store(bytes, std::memory_order_release);
			}
		}

		/*
			WARNING: IF THIS CLASS WILL BE GENERALIZED FOR OTHER PROJECTS, 
			THEN THE FOLLOWING DO-WHILE LOOP BELOW SHOULD IMPLEMENT SOME
			TYPE OF LOCK SO THAT THE MESSAGE CHUNKS DO NOT GET STOLEN BY
			CONCURRENT THREADS
		*/

		auto total_bytes_remaining = [&]
		{
			return this->blocks_left.load(std::memory_order_acquire) * this->block_size.load(std::memory_order_acquire);
		};
		
		if (p_data == nullptr)	// If inputted buffer is not defined, then dynamically allocate.
			p_data = malloc(total_bytes_remaining() + sizeof(uint64_t));
		
		/*
			For the following code below, recv_all wasn't used so that this function can
			potentially return with less data than what was sent from the client-side.
			This is so that the server can begin to respond quickly rather than having
			to wait for all the chunks to be received.
		*/
		uint8_t* p_data_buffer = (uint8_t*)(1 + (uint64_t*)p_data);	
		uint64_t bytes_read = 0;
		do	// Loop until the last memory block in the buffer is atomically received
		{
			ssize_t received = recv(this->client_socket, p_data_buffer, total_bytes_remaining() - bytes_read, 0);
			if (received <= 0)
			{
				/*
					TODO :If the connection has an error or is closed from the client side
					and p_data is treated with the default behavior using malloc, this
					portion will result in a memory leak.
				*/
				return nullptr;
			}

			bytes_read += received;	// Count total bytes read
			p_data_buffer += received;	// Shift buffer memory location over
		} while ((bytes_read % this->block_size.load(std::memory_order_acquire)) != 0);

		// Set the first 8 bytes of p_data to be how many data blocks were read
		*(uint64_t*)p_data = (uint64_t)(bytes_read / this->block_size.load(std::memory_order_acquire));

		// Reset the atomic values if all data in the stream is grabbed.
		if (this->blocks_left.fetch_sub(*(uint64_t*)p_data, std::memory_order_acq_rel) == *(uint64_t*)p_data)
			this->block_size.store(0, std::memory_order_release);
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

	this->blocks_left.store(0, std::memory_order_relaxed);
	this->block_size.store(0, std::memory_order_relaxed);
}