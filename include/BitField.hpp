#ifndef BITFIELD_HPP
#define BITFIELD_HPP

#include <cstdint>
#include <stdint.h>
#include <vector>

class BitField
{
	static constexpr uint64_t BITSIZE = 64;

	private:
		std::vector<uint64_t> bits;
	
	public:
	// CONSTRUCTORS
		BitField();
		BitField(std::size_t size);
		BitField(const BitField& bitfield);
		BitField(const std::vector<uint64_t>& value_list);

	// PUBLIC INTERFACE
		static BitField zero();

		bool insert(uint64_t index);
		bool contains(uint64_t index) const;
		bool remove(uint64_t index);
		void clear();	// Erases all elements
		void wipe();	// Zeros out all elements
		std::size_t max_bit_size() const;
		std::size_t max_byte_size() const;
		std::size_t vector_size() const;
		const std::vector<uint64_t>& return_vector();
		void copy_bit_vector(const std::vector<uint64_t>& bits);

	// OPERATOR OVERLOADS
		BitField& operator=(const BitField& bitfield);
		bool operator==(const BitField& bitfield) const;
		bool operator!=(const BitField& bitfield) const;
		
		BitField operator&(const BitField& bitfield) const;
		BitField operator|(const BitField& bitfield) const;
		BitField operator^(const BitField& bitfield) const;

		BitField& operator&=(const BitField& bitfield);
		BitField& operator|=(const BitField& bitfield);
		BitField& operator^=(const BitField& bitfield);
};

#endif // BITFIELD_HPP