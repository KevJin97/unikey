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

std::size_t BitField::size() const
{
	return this->bits.size() * 8;
}

BitField& BitField::operator= (const BitField& bitfield)
{
	this->bits = bitfield.bits;
	return *this;
}

bool BitField::operator==(const BitField& bitfield)
{
	std::size_t n = 0;
	const std::vector<uint64_t>* compare[2] = { &this->bits, &bitfield.bits };
	bool larger = (this->bits.size() >= bitfield.bits.size()) ? true : false;

	for (; n < compare[!larger]->size(); ++n)
	{
		if ((*compare[larger])[n] != (*compare[!larger])[n])
		{
			return false;
		}
	}
	for (; n < compare[larger]->size(); ++n)
	{
		if ((*compare[larger])[n] != 0)
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
	const BitField* fields[2];
	bool same_size;
	if (this->bits.size() == bit.bits.size())
	{
		fields[0] = this;
		fields[1] = &bit;
		same_size = true;
	}
	else
	{
		fields[0] = (this->bits.size() > bit.bits.size()) ? this : &bit;
		fields[1] = (this->bits.size() > bit.bits.size()) ? &bit : this;
		same_size = false;
	}

	BitField new_field(fields[0]->bits.size());
	if (same_size)
	{
		for (std::size_t n = 0; n < this->bits.size(); ++n)
		{
			new_field.bits[n] = this->bits[n] & bit.bits[n];
		}
	}
	else
	{
		std::size_t n = 0;
		for (; n < fields[1]->bits.size(); ++n)
		{
			new_field.bits[n] = fields[0]->bits[n] & fields[1]->bits[n];
		}
	}
	return new_field;
}

BitField BitField::operator|(const BitField& bit)
{
	const BitField* fields[2];
	bool same_size;
	if (this->bits.size() == bit.bits.size())
	{
		fields[0] = this;
		fields[1] = &bit;
		same_size = true;
	}
	else
	{
		fields[0] = (this->bits.size() > bit.bits.size()) ? this : &bit;
		fields[1] = (this->bits.size() > bit.bits.size()) ? &bit : this;
		same_size = false;
	}

	BitField new_field(fields[0]->bits.size());
	if (same_size)
	{
		for (std::size_t n = 0; n < this->bits.size(); ++n)
		{
			new_field.bits[n] = this->bits[n] | bit.bits[n];
		}
	}
	else
	{
		std::size_t n = 0;
		for (; n < fields[1]->bits.size(); ++n)
		{
			new_field.bits[n] = fields[0]->bits[n] | fields[1]->bits[n];
		}
		for (; n < fields[0]->bits.size(); ++n)
		{
			new_field.bits[n] = (fields[0]->bits[n]) ? fields[0]->bits[n] : 0;
		}
	}
	return new_field;
}

BitField BitField::operator^(const BitField& bit)
{
	const BitField* fields[2];
	bool same_size;
	if (this->bits.size() == bit.bits.size())
	{
		fields[0] = this;
		fields[1] = &bit;
		same_size = true;
	}
	else
	{
		fields[0] = (this->bits.size() > bit.bits.size()) ? this : &bit;
		fields[1] = (this->bits.size() > bit.bits.size()) ? &bit : this;
		same_size = false;
	}

	BitField new_field(fields[0]->bits.size());
	if (same_size)
	{
		for (std::size_t n = 0; n < this->bits.size(); ++n)
		{
			new_field.bits[n] = this->bits[n] ^ bit.bits[n];
		}
	}
	else
	{
		std::size_t n = 0;
		for (; n < fields[1]->bits.size(); ++n)
		{
			new_field.bits[n] = fields[0]->bits[n] ^ fields[1]->bits[n];
		}
		for (; n < fields[0]->bits.size(); ++n)
		{
			new_field.bits[n] = 0 ^ fields[0]->bits[n];
		}
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