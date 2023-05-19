/*
 * env - read environment variables for cppserver program
 *
 *  Created on: Feb 21, 2023
 *      Author: Martín Córdova cppserver@martincordova.com - https://cppserver.com
 *      Disclaimer: some parts of this library may have been taken from sample code publicly available
 *		and written by third parties. Free to use in commercial projects, no warranties and no responsabilities assumed 
 *		by the author, use at your own risk. By using this code you accept the forementioned conditions.
 */
#ifndef ENV_H_
#define ENV_H_

#include <string>
#include <cstdlib>

namespace env 
{
	bool loki_enabled() noexcept;
	std::string loki_server() noexcept;
	int loki_port() noexcept ;
	int port() noexcept;
	int http_log_enabled() noexcept;
	int stderr_log_enabled() noexcept;
	int pool_size() noexcept;
	int login_log_enabled() noexcept;
	std::string get_str(std::string name) noexcept;
}

#endif /* ENV_H_ */
