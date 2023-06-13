/*
 * logger - json log output to stderr and loki (optional using loki module), depends on env, loki
 *
 *  Created on: Feb 21, 2023
 *      Author: Martín Córdova cppserver@martincordova.com - https://cppserver.com
 *      Disclaimer: some parts of this library may have been taken from sample code publicly available
 *		and written by third parties. Free to use in commercial projects, no warranties and no responsabilities assumed 
 *		by the author, use at your own risk. By using this code you accept the forementioned conditions.
 */
#ifndef LOGGER_H_
#define LOGGER_H_

#include <string>
#include <cstdio>
#include <algorithm>
#include <thread>
#include "env.h"
#include "loki.h"

namespace logger
{
	void log(const std::string& source, const std::string& level, std::string msg, bool add_thread_id = false) noexcept;
	void print_env() noexcept;
	void set_request_id(const std::string& id) noexcept;
	std::string get_request_id() noexcept;
}

#endif /* LOGGER_H_ */
