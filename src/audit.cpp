#include "audit.h"

namespace audit
{
	const std::string LOGGER_SRC {"audit"};
	
	void save(const std::string& path, const std::string& user_login, const std::string& ip_address, config::microService& ms) noexcept
	{
		std::string record {ms.reqParams.get_audit_msg(ms.audit_record)};
		logger::log(LOGGER_SRC, "info", "path: " + path + " user: " + user_login + " remote-ip: " + ip_address + " " + record, true);
	}
}
