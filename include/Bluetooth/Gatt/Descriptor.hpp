#ifndef GATT_DESCRIPTOR_HPP
#define GATT_DESCRIPTOR_HPP

#include "Base_App_Obj.hpp"

#include <sdbus-c++/IConnection.h>

class Descriptor : public Base_App_Obj
{
	protected:
		std::vector<std::string> flags;

		void register_object() override;

	public:
		Descriptor(const std::string& uuid, const std::vector<std::string>& flags);
		virtual ~Descriptor() = default;

		virtual ByteArray on_read_value(OptionsMap options) const;
};

#endif	// GATT_DESCRIPTOR_HPP