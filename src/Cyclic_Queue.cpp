#include "Cyclic_Queue.hpp"

#include <atomic>
#include <stdlib.h>

Cyclic_Queue::~Cyclic_Queue()
{
	while (this->head.load() != this->tail.load())
	{
		free(this->queue[this->head.fetch_add(1, std::memory_order_acq_rel)]);
	}
}

void Cyclic_Queue::push(void* data)
{
	this->queue[this->tail.fetch_add(1, std::memory_order_acq_rel) % SIZE] = data;
}

void* Cyclic_Queue::pop()
{
	return this->queue[this->head.fetch_add(1, std::memory_order_acq_rel) % SIZE];
}

std::size_t Cyclic_Queue::size() const
{
	return this->tail.load() - this->head.load();
}