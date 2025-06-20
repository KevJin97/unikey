#ifndef SHARED_CYCLIC_QUEUE_HPP
#define SHARED_CYCLIC_QUEUE_HPP

#include <atomic>
#include <cstdint>

template<typename Type>
class Shared_Cyclic_Queue
{
	private:
		Type* p_queue;
		std::atomic_uint64_t index[2];
		std::size_t queue_size;

	public:
	// CONSTRUCTORS
		Shared_Cyclic_Queue();
		Shared_Cyclic_Queue(std::size_t queue_size);

	// DESTRUCTOR
		~Shared_Cyclic_Queue();

	// PUBLIC INTERFACE
		void push(const Type& data);
		Type pop();
};


// IMPLEMENTATION

template<typename Type>
Shared_Cyclic_Queue<Type>::Shared_Cyclic_Queue()
{
	this->queue_size = 64;
	this->index[0].store(0);
	this->index[1].store(0);
	this->p_queue = new Type[this->queue_size];
}

template<typename Type>
Shared_Cyclic_Queue<Type>::Shared_Cyclic_Queue(std::size_t queue_size)
{
	this->queue_size = queue_size;
	this->index[0].store(0);
	this->index[1].store(0);
	this->p_queue = new Type[this->queue_size];
}

template<typename Type>
Shared_Cyclic_Queue<Type>::~Shared_Cyclic_Queue()
{
	delete[] this->p_queue;
}

template<typename Type>
void Shared_Cyclic_Queue<Type>::push(const Type& data)
{
	uint64_t index = this->index[1].load(std::memory_order_acquire);
	this->index[1].fetch_add(1, std::memory_order_release);
	this->index[1].compare_exchange_weak(this->queue_size, 0, std::memory_order_release);

	if (this->index[0].load(std::memory_order_acquire) == this->index[1].load(std::memory_order_acquire))
	{
		this->pop();
	}
	
	this->p_queue[index] = data;
}

template<typename Type>
Type Shared_Cyclic_Queue<Type>::pop()
{
	Type return_val = this->p_queue[this->index[0].load(std::memory_order_acquire)];
	this->index[0].fetch_add(1, std::memory_order_seq_cst);
	this->index[0].compare_exchange_weak(this->queue_size, 0, std::memory_order_release);

	return return_val;
}

#endif // SHARED_CYCLIC_QUEUE_HPP