#include "Bluetooth/Gatt/Base_App_Obj.hpp"
#include <sdbus-c++/IConnection.h>

#include <string>

bool Base_App_Obj::initialize()
{
	if (this->dbus_connection == nullptr)
	{
		if (this->root_obj->dbus_connection == nullptr)
		{
			// Convert path name into bus name (Example: /org/bluez -> org.bluez)
			std::string bus_name = this->path;
			for (std::size_t n = 0; n < bus_name.size(); ++n)
			{
				if (bus_name[n] == '/')
				{
					bus_name[n] = '.';
				}
			}
			bus_name = bus_name.substr(1, bus_name.size() - 1);

			this->new_connection = sdbus::createSystemBusConnection(bus_name);
			this->dbus_connection = this->new_connection.get();
			this->root_obj = this;
		}
		else
		{
			this->dbus_connection = this->root_obj->dbus_connection;
		}
	}
	
	this->dbus_object = sdbus::createObject(*this->dbus_connection, this->get_full_path());

	this->attempted_registration = true;
	return this->attempted_registration;
}

Base_App_Obj::Base_App_Obj(const std::string& obj_path, const std::string& uuid, sdbus::IConnection* connection)
{	
	this->dbus_connection = connection;
	this->path = obj_path;
	this->uuid = uuid;
}

void Base_App_Obj::add_subelement(Base_App_Obj* base_obj)
{
	if (base_obj != nullptr)
	{
		/*
			Add an index to the relative path name, add it to the list, and set
			the root to be the object creating it.
		*/ 
		base_obj->root_obj = this;
		base_obj->path = base_obj->path + std::to_string(this->subelements.size());
		this->subelements.push_back(base_obj);
		
		if (base_obj->initialize())
		{
			base_obj->register_object();
		}
	}
}

std::string Base_App_Obj::get_full_path() const
{
	if (this->root_obj == this)
		return this->path;
	/*
		this->path only holds the relative path name. When the full path name is
		needed, it will generate it recursively by accessing the root path variable.
	*/
	return this->root_obj->get_full_path() + this->path;
}

std::string Base_App_Obj::get_parent_path() const
{
	if (this->root_obj == this)
		return "/";

	return this->root_obj->get_full_path();
}

std::string Base_App_Obj::get_root_path() const
{
	if (this == this->root_obj)
		return this->root_obj->path;

	return this->root_obj->get_root_path();
}

const std::string& Base_App_Obj::get_uuid() const
{
	return this->uuid;
}

Base_App_Obj::~Base_App_Obj()
{
	for (std::size_t n = 0; n < this->subelements.size(); ++n)
		delete this->subelements[n];
}