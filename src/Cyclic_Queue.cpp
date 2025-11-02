#include "Cyclic_Queue.hpp"

#include <atomic>
#include <stdlib.h>

Cyclic_Queue::~Cyclic_Queue()
{
	while (this->head.load() != this->tail.load())
	{
		free(this->queue[this->head.fetch_add(1)]);
	}
}

void Cyclic_Queue::push(void* data)
{
	this->queue[this->tail.fetch_add(1, std::memory_order_acq_rel) % SIZE]
		.store(data, std::memory_order_release);
}

void* Cyclic_Queue::pop()
{
	return (this->size())
		? this->queue[this->head.fetch_add(1, std::memory_order_acq_rel) % SIZE]
			.exchange(nullptr, std::memory_order_acq_rel)
		: nullptr;
}

std::size_t Cyclic_Queue::size() const
{
	return this->tail.load(std::memory_order_acquire) - this->head.load(std::memory_order_acquire);
}