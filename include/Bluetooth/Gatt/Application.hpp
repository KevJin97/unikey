#ifndef GATT_APPLICATION_HPP
#define GATT_APPLICATION_HPP

#include "Bluetooth/Gatt/Base_App_Obj.hpp"

#include <sdbus-c++/IConnection.h>

class Application : public Base_App_Obj
{
	private:
		void register_object() override
		{
			if (this->dbus_object == nullptr)
			{
				this->attempted_registration = true;
				return;
			}
			
			this->dbus_object->addObjectManager();
			this->dbus_object->finishRegistration();
		}
		
	public:
		Application(const std::string& path="/", sdbus::IConnection* connection=nullptr)
		: Base_App_Obj(path, "", connection)
		{
			this->initialize();
			this->register_object();
		}

		virtual ~Application() = default;
};

#endif	// GATT_APPLICATION_HPP