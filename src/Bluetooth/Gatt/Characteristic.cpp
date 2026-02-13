#include "Bluetooth/Gatt/Characteristic.hpp"
#include "Bluetooth/Gatt/Base_App_Obj.hpp"

void Characteristic::register_object()
{
	if (this->dbus_object == nullptr)
	{
		this->attempted_registration = true;
		return;
	}

	this->dbus_object->registerProperty("UUID")
		.onInterface("org.bluez.GattCharacteristic1")
			.withGetter([this](){ return this->uuid; });

	this->dbus_object->registerProperty("Service")
		.onInterface("org.bluez.GattCharacteristic1")
			.withGetter([this](){ return sdbus::ObjectPath(this->get_parent_path()); });

	this->dbus_object->registerProperty("Flags")
		.onInterface("org.bluez.GattCharacteristic1")
			.withGetter([this](){ return this->flags; });

	this->dbus_object->registerProperty("Descriptors")
		.onInterface("org.bluez.GattCharacteristic1")
			.withGetter(
				[this]()
				{
	    			std::vector<sdbus::ObjectPath> path;
					for (const auto& descriptor : this->subelements)
						path.push_back(sdbus::ObjectPath(descriptor->get_full_path()));
					
					return path;
				}
			);

	this->dbus_object->registerMethod("ReadValue")
		.onInterface("org.bluez.GattCharacteristic1")
			.implementedAs([this](OptionsMap options){ return this->on_read_value(options); });

	this->dbus_object->registerMethod("WriteValue")
		.onInterface("org.bluez.GattCharacteristic1")
			.implementedAs([this](ByteArray bytes, OptionsMap options){ this->on_write_value(bytes, options); });

	this->dbus_object->registerMethod("StartNotify")
		.onInterface("org.bluez.GattCharacteristic1")
			.implementedAs([this](){ this->on_start_notify(); });

	this->dbus_object->registerMethod("StopNotify")
		.onInterface("org.bluez.GattCharacteristic1")
			.implementedAs([this](){ this->on_stop_notify(); });

	this->dbus_object->finishRegistration();
}

Characteristic::Characteristic(const std::string& uuid, const std::vector<std::string>& flags) : Base_App_Obj("/char", uuid)
{
	this->flags = flags;
}

void Characteristic::emit_properties_changed(const std::string& interface, const std::map<std::string, sdbus::Variant>& changed)
{
	if (this->dbus_object)
	{
		this->dbus_object->emitSignal("PropertiesChanged")
			.onInterface("org.freedesktop.DBus.Properties")
				.withArguments(interface, changed, std::vector<std::string>{});
	}
}

ByteArray Characteristic::on_read_value(OptionsMap options) const
{
	throw sdbus::Error("org.bluez.Error.NotSupported", "Not supported");
}

void Characteristic::on_write_value(ByteArray, OptionsMap)
{
	throw sdbus::Error("org.bluez.Error.NotSupported", "Not supported");
}

void Characteristic::on_start_notify() const
{

}

void Characteristic::on_stop_notify() const
{

}