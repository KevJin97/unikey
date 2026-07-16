#ifndef GATT_SERVICE_HPP
#define GATT_SERVICE_HPP

#include "Base_App_Obj.hpp"

#include <sdbus-c++/IConnection.h>

class Service : public Base_App_Obj
{
	protected:
		bool primary;

		void register_object() override;

	public:
		Service(const std::string& uuid, bool primary);
		virtual ~Service() = default;

		const bool get_primary() const;
};

#endif	// GATT_SERVICE_HPP