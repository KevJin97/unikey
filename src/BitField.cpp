#include <cstdint>
#include <vector>

#include "BitField.hpp"

BitField BitField::zero()
{
	static BitField bitfield;
	return bitfield;
}

BitField::BitField() {}

BitField::BitField(std::size_t size)
{
	this->bits.resize((size / BITSIZE) + 1, 0);
}

BitField::BitField(const BitField& bitfield)
{
	this->bits = bitfield.bits;
}

BitField::BitField(const std::vector<uint64_t>& value_list)
{
	for (std::size_t n = 0; n < value_list.size(); ++n)
	{
		this->insert(value_list[n]);
	}
}

bool BitField::insert(uint64_t index)
{
	if (this->bits.size() <= index / BITSIZE)
	{
		this->bits.resize(index / BITSIZE + 1, 0);
	}

	uint64_t orig_val = this->bits[index / BITSIZE];
	this->bits[index / BITSIZE] |= (1ULL << (index % BITSIZE));
	return orig_val ^ this->bits[index / BITSIZE];
}

bool BitField::contains(uint64_t index) const
{
	return (this->bits.size() <= index / BITSIZE) ? false : ((this->bits[index / BITSIZE] & (1ULL << (index % BITSIZE))) != 0);
}

bool BitField::remove(uint64_t index)
{
	if (this->bits.size() > index / BITSIZE)
	{
		uint64_t orig_val = this->bits[index / BITSIZE];
		this->bits[index / BITSIZE] &= ~(1ULL << (index % BITSIZE));
		return orig_val ^ this->bits[index / BITSIZE];
	}
	return false;
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

const std::vector<uint64_t>& BitField::return_vector() const
{
	return this->bits;
}

void BitField::copy_bit_vector(const std::vector<uint64_t>& bits)
{
	this->bits = bits;
}

BitField& BitField::operator=(const BitField& bitfield)
{
	this->bits = bitfield.bits;
	return *this;
}

bool BitField::operator==(const BitField& bitfield) const
{
	std::size_t n = 0;
	const BitField* compare[2] = { this, &bitfield };
	bool larger = (this->bits.size() < bitfield.bits.size());

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

bool BitField::operator!=(const BitField& bitfield) const
{
	return !(*this == bitfield);
}

BitField BitField::operator&(const BitField& bitfield) const
{
	const BitField* fields[2] = { this, &bitfield };
	bool larger = (this->bits.size() < bitfield.bits.size());
	BitField new_field;
	new_field.bits.resize(fields[!larger]->bits.size());

	for (std::size_t n = 0; n < fields[!larger]->bits.size(); ++n)
	{
		new_field.bits[n] = (fields[larger]->bits[n] & fields[!larger]->bits[n]);
	}
	
	return new_field;
}

BitField BitField::operator|(const BitField& bitfield) const
{
	const BitField* fields[2] = { this, &bitfield };
	bool larger = (this->bits.size() < bitfield.bits.size());
	BitField new_field = *fields[larger];

	for (std::size_t n = 0; n < fields[!larger]->bits.size(); ++n)
	{
		new_field.bits[n] |= fields[!larger]->bits[n];
	}

	return new_field;
}

BitField BitField::operator^(const BitField& bitfield) const
{
	const BitField* fields[2] = { this, &bitfield };
	bool larger = (this->bits.size() < bitfield.bits.size());
	BitField new_field = *fields[larger];

	for (std::size_t n = 0; n < fields[!larger]->bits.size(); ++n)
	{
		new_field.bits[n] ^= fields[!larger]->bits[n];
	}

	return new_field;
}

BitField& BitField::operator&=(const BitField& bitfield)
{
	if (this->bits.size() > bitfield.bits.size())
	{
		this->bits.resize(bitfield.bits.size());
	}

	for (std::size_t n = 0; n < this->bits.size(); ++n)
	{
		this->bits[n] &= bitfield.bits[n];
	}
	
	return *this;
}

BitField& BitField::operator|=(const BitField& bitfield)
{
	const std::size_t initial_size = this->bits.size();

	if (this->bits.size() < bitfield.bits.size())
	{
		this->bits.resize(bitfield.bits.size(), 0);
	}

	for (std::size_t n = 0; n < this->bits.size(); ++n)
	{
		if (n < initial_size)
		{
			this->bits[n] |= bitfield.bits[n];
		}
		else
		{
			this->bits[n] = bitfield.bits[n];
		}
	}

	return *this;
}

BitField& BitField::operator^=(const BitField& bitfield)
{
	if (this == &bitfield)
	{
		this->clear();
		return *this;
	}
	
	std::size_t overlap = (this->bits.size() <= bitfield.bits.size()) ? this->bits.size() : bitfield.bits.size();
	for (std::size_t n = 0; n < overlap; ++n)
	{
		this->bits[n] ^= bitfield.bits[n];
	}

	if (this->bits.size() < bitfield.bits.size())
	{
		this->bits.resize(bitfield.bits.size());
		for (std::size_t n = overlap; n < this->bits.size(); ++n)
		{
			this->bits[n] = bitfield.bits[n];
		}
	}
	return *this;
}