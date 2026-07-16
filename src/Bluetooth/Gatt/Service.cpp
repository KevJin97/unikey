#include "Bluetooth/Gatt/Base_App_Obj.hpp"
#include "Bluetooth/Gatt/Service.hpp"

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/Types.h>

void Service::register_object()
{
	if (this->dbus_object == nullptr)
	{
		this->attempted_registration = true;
		return;
	}

	this->dbus_object->registerProperty("UUID")
		.onInterface("org.bluez.GattService1")
			.withGetter([this](){ return this->uuid; });

	this->dbus_object->registerProperty("Primary")
		.onInterface("org.bluez.GattService1")
			.withGetter([this](){ return this->primary; });

	this->dbus_object->registerProperty("Characteristics")
		.onInterface("org.bluez.GattService1")
			.withGetter(
				[this]()
				{
					std::vector<sdbus::ObjectPath> char_list;

					for (const auto& characteristic : this->subelements)
						char_list.push_back(sdbus::ObjectPath(characteristic->get_full_path()));
					
					return char_list;
				}
			);
	
	this->dbus_object->finishRegistration();
}

Service::Service(const std::string& uuid, bool primary) : Base_App_Obj("/service", uuid)
{
	this->primary = primary;
}

const bool Service::get_primary() const
{
	return this->primary;
}
