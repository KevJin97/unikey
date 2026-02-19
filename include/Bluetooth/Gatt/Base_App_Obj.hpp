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
	// PRIVATE MEMBER DATA
		Base_App_Obj* root_obj = this;
		std::unique_ptr<sdbus::IConnection> new_connection = nullptr;
		sdbus::IConnection* dbus_connection = nullptr;

	// PRIVATE INTERFACE
		Base_App_Obj* get_root();

	protected:
	// PROTECED MEMBER DATA
		bool attempted_registration = false;
		std::unique_ptr<sdbus::IObject> dbus_object;
		std::string uuid;
		std::string path;
		std::vector<Base_App_Obj*> subelements;

	// PROTECTED INTERFACE
		bool initialize();
		void initialize_subtree();
		bool has_connection() const;
		virtual void register_object() = 0;

	// PROTECTED CONSTRUCTOR
		Base_App_Obj(const std::string& obj_path="/", const std::string& uuid="", sdbus::IConnection* connection=nullptr);
	
	public:
	// PUBLIC DESTRUCTOR
		virtual ~Base_App_Obj();

	// PUBLIC INTERFACE
		void add_subelement(Base_App_Obj* base_obj);
		std::string get_full_path() const;
		std::string get_parent_path() const;
		std::string get_root_path() const;
		const std::string& get_uuid() const;

	// OPERATOR OVERLOADS
		const std::vector<Base_App_Obj*>& operator()() const;
		Base_App_Obj* operator[](std::size_t index) const;
};

#endif	// BASE_APP_OBJ_HPP