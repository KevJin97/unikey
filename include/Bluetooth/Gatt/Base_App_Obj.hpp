#ifndef BASE_APP_OBJ_HPP
#define BASE_APP_OBJ_HPP

#include <memory>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IObject.h>

using ByteArray = std::vector<uint8_t>;
using OptionsMap = std::map<std::string, sdbus::Variant>;
using PropertiesMap = std::map<std::string, sdbus::Variant>;
using InterfacesMap = std::map<std::string, PropertiesMap>;
using ManagedObjectsMap = std::map<sdbus::ObjectPath, InterfacesMap>;

class Application;
class Characteristic;
class Descriptor;
class Service;

class Base_App_Obj
{
	friend Application;

	private:
		Base_App_Obj* root_obj = this;
		std::unique_ptr<sdbus::IConnection> new_connection = nullptr;
		sdbus::IConnection* dbus_connection = nullptr;

	protected:
		bool attempted_registration = false;
		std::unique_ptr<sdbus::IObject> dbus_object;
		std::string uuid;
		std::string path;
		std::vector<Base_App_Obj*> subelements;

		bool initialize();
		virtual void register_object() = 0;

		Base_App_Obj(const std::string& obj_path="/", const std::string& uuid="", sdbus::IConnection* connection=nullptr);
	
	public:
		virtual ~Base_App_Obj();

		void add_subelement(Base_App_Obj* base_obj);
		std::string get_full_path() const;
		std::string get_parent_path() const;
		std::string get_root_path() const;
		const std::string& get_uuid() const;
};

#endif	// BASE_APP_OBJ_HPP