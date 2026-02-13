#ifndef GATT_CHARACTERISTIC_HPP
#define GATT_CHARACTERISTIC_HPP

#include "Base_App_Obj.hpp"

#include <map>
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

		void emit_properties_changed(const std::string& interface, const std::map<std::string, sdbus::Variant>& changed);
};

#endif	// GATT_CHARACTERISTIC_HPP