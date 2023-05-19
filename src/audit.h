/*
 * audit - default implementation os audit recorder, saves to logger
 *
 *  Created on: May 17, 2023
 *      Author: Martín Córdova cppserver@martincordova.com - https://cppserver.com
 *      Disclaimer: some parts of this library may have been taken from sample code publicly available
 *		and written by third parties. Free to use in commercial projects, no warranties and no responsabilities assumed 
 *		by the author, use at your own risk. By using this code you accept the forementioned conditions.
 */
#ifndef AUDIT_H_
#define AUDIT_H_

#include <string>
#include "logger.h"
#include "config.h"

namespace audit
{
	void save(const std::string& path, const std::string& user_login, const std::string& ip_address, config::microService& ms) noexcept;
}

#endif /* AUDIT_H_ */
