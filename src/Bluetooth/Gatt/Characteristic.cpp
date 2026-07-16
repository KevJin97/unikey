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

	this->dbus_object->registerProperty("Value")
		.onInterface("org.bluez.GattCharacteristic1")
			.withGetter([this](){ return this->on_read_value({}); })
			.withSetter([this](const ByteArray& val){ this->on_write_value(val, {}); });

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

void Characteristic::update_value(const ByteArray& new_value)
{
	this->on_write_value(new_value, {});
	this->dbus_object->emitPropertiesChangedSignal("org.bluez.GattCharacteristic1", { "Value" });
}