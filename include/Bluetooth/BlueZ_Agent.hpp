#ifndef BLUEZ_AGENT_HPP
#define BLUEZ_AGENT_HPP

#include "Bluetooth/org_bluez_agent_interface.hpp"

#include <sdbus-c++/AdaptorInterfaces.h>
#include <sdbus-c++/IConnection.h>

class BlueZ_Agent : public sdbus::AdaptorInterfaces<org::bluez::Agent1_adaptor>
{
	private:
		std::string capability;

		void Release() override;
    	std::string RequestPinCode(const sdbus::ObjectPath& arg0) override;
    	void DisplayPinCode(const sdbus::ObjectPath& arg0, const std::string& arg1) override;
    	uint32_t RequestPasskey(const sdbus::ObjectPath& arg0) override;
    	void DisplayPasskey(const sdbus::ObjectPath& arg0, const uint32_t& arg1, const uint16_t& arg2) override;
    	void RequestConfirmation(const sdbus::ObjectPath& arg0, const uint32_t& arg1) override;
    	void RequestAuthorization(const sdbus::ObjectPath& arg0) override;
    	void AuthorizeService(const sdbus::ObjectPath& arg0, const std::string& arg1) override;
    	void Cancel() override;
	
	public:
		BlueZ_Agent(sdbus::IConnection& connection, const std::string& path, const std::string& capability="NoInputNoOutput");
		~BlueZ_Agent();

		const std::string& get_path() const;
		const std::string& get_capability() const;
};

#endif	// BLUEZ_AGENT_HPP