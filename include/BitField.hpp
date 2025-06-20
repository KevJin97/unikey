#ifndef BITFIELD_HPP
#define BITFIELD_HPP

#include <cstdint>
#include <stdint.h>
#include <vector>

class BitField
{
	private:
		std::vector<uint64_t> bits;
	
	public:
		BitField();
		BitField(std::size_t size);
		BitField(const BitField& bit);
		BitField(const std::vector<uint64_t>& value_list);

		void insert(uint64_t index);
		bool contains(uint64_t index);
		void remove(uint64_t index);
		void clear();
		std::size_t size() const;

		BitField& operator=(const BitField& bit);
		bool operator==(const BitField& bit);
		bool operator!=(const BitField& bit);
		
		BitField operator&(const BitField& bit);
		BitField operator|(const BitField& bit);
		BitField operator^(const BitField& bit);

		BitField& operator&=(const BitField& bit);
		BitField& operator|=(const BitField& bit);
		BitField& operator^=(const BitField& bit);
};

#endif // BITFIELD_HPP