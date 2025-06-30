#include <cstdint>
#include <vector>

#include "BitField.hpp"
#define BITSIZE 64

BitField::BitField() {}
BitField::BitField(std::size_t size)
{
	this->bits.resize(size / BITSIZE, 0);
}

BitField::BitField(const BitField& bit)
{
	this->bits = bit.bits;
}

BitField::BitField(const std::vector<uint64_t>& value_list)
{
	for (std::size_t n = 0; n < value_list.size(); ++n)
	{
		this->insert(value_list[n]);
	}
}

void BitField::insert(uint64_t index)
{
	if (this->bits.size() > index / BITSIZE)
	{
		this->bits[index / BITSIZE] |= (1 << (index % BITSIZE));
	}
	else
	{
		while (this->bits.size() < (index / BITSIZE))
		{
			this->bits.push_back(0);
		}
		this->bits.push_back(1 << (index % BITSIZE));
	}
}

bool BitField::contains(uint64_t index)
{
	return (this->bits.size() <= index / BITSIZE) ? false : (this->bits[index / BITSIZE] & (1 << (index % BITSIZE))) != 0;
}

void BitField::remove(uint64_t index)
{
	if (this->bits.size() > index / BITSIZE)
	{
		this->bits[index / BITSIZE] &= ~(1 << (index % BITSIZE));
	}
}

void BitField::clear()
{
	this->bits.clear();
}

void BitField::wipe()
{
	for (std::size_t n = 0; n < this->bits.size(); ++n)
	{
		this->bits[n] = 0;
	}
}

std::size_t BitField::max_bit_size() const
{
	return this->bits.size() * BITSIZE;
}

std::size_t BitField::max_byte_size() const
{
	return this->bits.size() * BITSIZE / 8;
}

std::size_t BitField::vector_size() const
{
	return this->bits.size();
}

BitField& BitField::operator= (const BitField& bitfield)
{
	this->bits = bitfield.bits;
	return *this;
}

bool BitField::operator==(const BitField& bitfield)
{
	std::size_t n = 0;
	const BitField* compare[2] = { this, &bitfield };
	bool larger = (this->bits.size() >= bitfield.bits.size()) ? false : true;

	for (; n < compare[!larger]->bits.size(); ++n)
	{
		if (compare[larger]->bits[n] != compare[!larger]->bits[n])
		{
			return false;
		}
	}
	for (; n < compare[larger]->bits.size(); ++n)
	{
		if (compare[larger]->bits[n] != 0)
		{
			return false;
		}
	}
	return true;
}

bool BitField::operator!=(const BitField& bitfield)
{
	return !(*this == bitfield);
}

BitField BitField::operator&(const BitField& bit)
{
	const BitField* fields[2] = { this, &bit };
	bool larger = (this->bits.size() >= bit.bits.size()) ? false : true;
	std::size_t n = 0;
	BitField new_field;
	new_field.bits.resize(fields[larger]->bits.size());

	for (; n < fields[!larger]->bits.size(); ++n)
	{
		new_field.bits[n] = (fields[larger]->bits[n] & fields[!larger]->bits[n]);
	}
	for (; n < fields[larger]->bits.size(); ++n)
	{
		new_field.bits[n] = 0;
	}
	
	return new_field;
}

BitField BitField::operator|(const BitField& bit)
{
	const BitField* fields[2] = { this, &bit };
	bool larger = (this->bits.size() >= bit.bits.size()) ? false : true;
	BitField new_field = *fields[larger];

	for (std::size_t n = 0; n < fields[!larger]->bits.size(); ++n)
	{
		new_field.bits[n] |= fields[!larger]->bits[n];
	}

	return new_field;
}

BitField BitField::operator^(const BitField& bit)
{
	const BitField* fields[2] = { this, &bit };
	bool larger = (this->bits.size() >= bit.bits.size()) ? false : true;
	BitField new_field = *fields[larger];

	for (std::size_t n = 0; n < fields[!larger]->bits.size(); ++n)
	{
		new_field.bits[n] ^= fields[!larger]->bits[n];
	}

	return new_field;
}

BitField& BitField::operator&=(const BitField& bit)
{
	return *this = *this & bit;
}

BitField& BitField::operator|=(const BitField& bit)
{
	return *this = *this | bit;
}

BitField& BitField::operator^=(const BitField& bit)
{
	return *this = *this ^ bit;
}


#undef BITSIZE