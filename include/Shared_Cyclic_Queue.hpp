#ifndef SHARED_CYCLIC_QUEUE_HPP
#define SHARED_CYCLIC_QUEUE_HPP

#include <atomic>
#include <cstdint>

template<typename Type>
class Shared_Cyclic_Queue
{
	private:
	// PRIVATE MEMBERS
		Type* p_queue;
		std::atomic_uint64_t index[2];
		std::atomic_uint64_t num_in_q;
		std::size_t capacity;

	public:
	// CONSTRUCTORS
		Shared_Cyclic_Queue();
		Shared_Cyclic_Queue(const Shared_Cyclic_Queue&) = delete;	// Delete copy-constructors
		Shared_Cyclic_Queue(std::size_t queue_size);

	// DESTRUCTOR
		~Shared_Cyclic_Queue();

	// PUBLIC INTERFACE
		void push(const Type& data);
		Type pop();
		bool is_empty();

		Shared_Cyclic_Queue& operator=(const Shared_Cyclic_Queue&) = delete; // Delete copy-constructors
};


// IMPLEMENTATION
template<typename Type>
Shared_Cyclic_Queue<Type>::Shared_Cyclic_Queue()
{
	this->capacity = 64;
	this->index[0].store(0);
	this->index[1].store(0);
	this->num_in_q.store(0);
	this->p_queue = new Type[this->capacity];
}

template<typename Type>
Shared_Cyclic_Queue<Type>::Shared_Cyclic_Queue(std::size_t queue_size)
{
	this->capacity = queue_size;
	this->index[0].store(0);
	this->index[1].store(0);
	this->num_in_q.store(0);
	this->p_queue = new Type[this->capacity];
}

template<typename Type>
Shared_Cyclic_Queue<Type>::~Shared_Cyclic_Queue()
{
	delete[] this->p_queue;
}

template<typename Type>
void Shared_Cyclic_Queue<Type>::push(const Type& data)
{
	// Atomically claim a free "slot" and cycle counter to 0 when out-of-bounds is reached
	uint64_t slot = this->index[1].fetch_add(1, std::memory_order_acq_rel);
	this->index[1].compare_exchange_strong(this->capacity, 0, std::memory_order_acq_rel, std::memory_order_relaxed);
	
	this->p_queue[slot] = data;	// Set data

	slot = this->num_in_q.fetch_add(1, std::memory_order_acq_rel);	// After setting raise count by one
	this->num_in_q.notify_one();
	if (this->num_in_q.compare_exchange_strong(this->capacity + 1, slot, std::memory_order_acq_rel, std::memory_order_relaxed))
	{
		// When num_in_q == capacity + 1, index[1] has cycled around to the same position as index[0].
		// Get rid of oldest value in queue
		this->index[0].fetch_add(1, std::memory_order_acq_rel);
		this->index[0].compare_exchange_strong(this->capacity, 0, std::memory_order_acq_rel, std::memory_order_relaxed);
	}
}

template<typename Type>
Type Shared_Cyclic_Queue<Type>::pop()
{
	this->num_in_q.wait(0, std::memory_order_acquire);	// Ensures that running pop() won't interrupt setting value at index[1]
	
	// Get index[0] value and increment by 1. If out-of-bounds, cycle to 0
	Type return_val = this->p_queue[this->index[0].fetch_add(1, std::memory_order_acq_rel)];
	this->index[0].compare_exchange_strong(this->capacity, 0, std::memory_order_acq_rel, std::memory_order_relaxed);
	this->num_in_q.fetch_sub(1, std::memory_order_acq_rel);	// Reduce counter
	return return_val;
}

template<typename Type>
bool Shared_Cyclic_Queue<Type>::is_empty()
{
	if (this->num_in_q.load(std::memory_order_acquire)) // if num_in_q != 0
	{
		return false;
	}
	return true;
}

#endif // SHARED_CYCLIC_QUEUE_HPP