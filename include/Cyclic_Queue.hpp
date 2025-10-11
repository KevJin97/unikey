#ifndef CYCLIC_QUEUE_HPP
#define CYCLIC_QUEUE_HPP

#include <atomic>

static inline constexpr std::size_t SIZE = 256;

class Cyclic_Queue
{
	private:
		std::atomic<void*> queue[SIZE] = { nullptr };
		std::atomic<std::size_t> head = 0;
		std::atomic<std::size_t> tail = 0;

	public:
		Cyclic_Queue() {}
		~Cyclic_Queue();
		
		void push(void* data);
		void* pop();
		std::size_t size() const;
};

#endif	// CYCLIC_QUEUE_HPP