#include "Bluetooth/Gatt/Descriptor.hpp"
#include "Bluetooth/Gatt/Base_App_Obj.hpp"

#include <sdbus-c++/Types.h>

void Descriptor::register_object()
{
	if (this->dbus_object == nullptr)
	{
		this->attempted_registration = true;
		return;
	}

	this->dbus_object->registerProperty("UUID")
		.onInterface("org.bluez.GattDescriptor1")
			.withGetter([this](){ return this->uuid; });

	this->dbus_object->registerProperty("Characteristic")
		.onInterface("org.bluez.GattDescriptor1")
			.withGetter([this](){ return sdbus::ObjectPath(this->get_parent_path()); });
	
	this->dbus_object->registerProperty("Flags")
		.onInterface("org.bluez.GattDescriptor1")
			.withGetter([this](){ return this->flags; });
	
	this->dbus_object->registerMethod("ReadValue")
		.onInterface("org.bluez.GattDescriptor1")
			.implementedAs([this](OptionsMap options){ return this->on_read_value(options); });
	
	this->dbus_object->finishRegistration();
}

Descriptor::Descriptor(const std::string& uuid, const std::vector<std::string>& flags) : Base_App_Obj("/desc", uuid)
{
	this->flags = flags;
}

ByteArray Descriptor::on_read_value(OptionsMap options) const
{
	throw sdbus::Error("org.bluez.Error.NotSupported", "Not supported");
}