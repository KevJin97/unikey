#include "Bluetooth/BlueZ_Agent.hpp"
#include "Bluetooth/org_bluez_agent_interface.hpp"

#include <iostream>
#include <string>

#include <sdbus-c++/AdaptorInterfaces.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/Types.h>

void BlueZ_Agent::Release()
{
	std::cout << "Agent released" << std::endl;
}

std::string BlueZ_Agent:: RequestPinCode(const sdbus::ObjectPath& arg0)
{
	std::cout << "RequestPinCode from: " << arg0 << std::endl;
	return "0000";
}

void BlueZ_Agent::DisplayPinCode(const sdbus::ObjectPath& arg0, const std::string& arg1)
{
	std::cout << "DisplayPinCode for " << arg0 << ": " << arg1 << std::endl;
}

uint32_t BlueZ_Agent::RequestPasskey(const sdbus::ObjectPath& arg0)
{
	std::cout << "RequestPasskey from: " << arg0 << std::endl;
	return 0;
}

void BlueZ_Agent::DisplayPasskey(const sdbus::ObjectPath& arg0, const uint32_t& arg1, const uint16_t& arg2)
{
	std::cout << "DisplayPasskey for " << arg0 << ": " << arg1 << "(entered: " << arg2 << ")" << std::endl;
}

void BlueZ_Agent::RequestConfirmation(const sdbus::ObjectPath& arg0, const uint32_t& arg1)
{
	std::cout << "RequestConfirmation from " << arg0 << " passkey: " << arg1 << " -> auto-accepting" << std::endl;
}

void BlueZ_Agent::RequestAuthorization(const sdbus::ObjectPath& arg0)
{
	std::cout << "RequestAuthorization from " << arg0 << " -> auto-accepting" << std::endl;
}

void BlueZ_Agent::AuthorizeService(const sdbus::ObjectPath& arg0, const std::string& arg1)
{
	std::cout << "AuthorizeService from " << arg0 << " uuid: " << arg1 << " -> auto-accepting" << std::endl;
}

void BlueZ_Agent::Cancel()
{
	std::cout << "Agent pairing cancelled" << std::endl;
}

BlueZ_Agent::BlueZ_Agent(sdbus::IConnection& connection, const std::string& path, const std::string& capability) :  sdbus::AdaptorInterfaces<org::bluez::Agent1_adaptor>(connection, path), capability(capability)
{
	this->registerAdaptor();
}

BlueZ_Agent::~BlueZ_Agent()
{
	this->unregisterAdaptor();
}

const std::string& BlueZ_Agent::get_path() const
{
	return this->getObjectPath();
}

const std::string& BlueZ_Agent::get_capability() const
{
	return this->capability;
}
