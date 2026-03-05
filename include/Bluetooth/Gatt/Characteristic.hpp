#ifndef GATT_CHARACTERISTIC_HPP
#define GATT_CHARACTERISTIC_HPP

#include "Base_App_Obj.hpp"

#include <vector>

#include <sdbus-c++/IObject.h>
#include <sdbus-c++/Types.h>

class Characteristic : public Base_App_Obj
{
	protected:
		std::vector<std::string> flags;
	
		void register_object() override;

	public:
		Characteristic(const std::string& uuid, const std::vector<std::string>& flags);
		virtual ~Characteristic() = default;

		virtual ByteArray on_read_value(OptionsMap options) const;
		virtual void on_write_value(ByteArray, OptionsMap);
		virtual void on_start_notify() const;
		virtual void on_stop_notify() const;

		virtual void update_value(const ByteArray& new_value);
};

#endif	// GATT_CHARACTERISTIC_HPP