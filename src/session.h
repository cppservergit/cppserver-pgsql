/*
 * session - security session manager using pgsql functions and stored procs - depends on libpq (pgsql native client API)
 *
 *  Created on: Feb 21, 2023
 *      Author: Martín Córdova cppserver@martincordova.com - https://cppserver.com
 *      Disclaimer: some parts of this library may have been taken from sample code publicly available
 *		and written by third parties. Free to use in commercial projects, no warranties and no responsabilities assumed 
 *		by the author, use at your own risk. By using this code you accept the forementioned conditions.
 */
#ifndef SESSION_H_
#define SESSION_H_

#include <string>
#include <iostream>
#include <unordered_map>
#include <libpq-fe.h>
#include "env.h"
#include "logger.h"

namespace session
{
	bool create(std::string& sessionid, const std::string& userlogin, const std::string& mail, const std::string& ip, const std::string& roles) noexcept;
	bool update(const std::string& sessionid, std::string& userlogin, std::string& email, std::string& roles) noexcept;
	int get_total() noexcept;
	void remove(const std::string& sessionid) noexcept;
}

#endif /* SESSION_H_ */
