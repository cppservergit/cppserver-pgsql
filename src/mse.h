/*
 * mse - microservice engine - depends on logger env session sql login httputils json
 *
 *  Created on: Feb 26, 2023
 *      Author: Martin Cordova cppserver@martincordova.com - https://cppserver.com
 *      Disclaimer: some parts of this library may have been taken from sample code publicly available
 *		and written by third parties. Free to use in commercial projects, no warranties and no responsabilities assumed 
 *		by the author, use at your own risk. By using this code you accept the forementioned conditions.
 */
#ifndef MSE_H_
#define MSE_H_

#include <iostream>
#include <vector>
#include <unordered_map>
#include <charconv>
#include <functional>
#include <filesystem>
#include <sys/stat.h>
#include <fcntl.h>
#include "env.h"
#include "logger.h"
#include "login.h"
#include "sql.h"
#include "session.h"
#include "httputils.h"
#include "config.h"
#include "audit.h"
#include "email.h"

namespace mse
{
	constexpr char SERVER_VERSION[] = "cppserver-pgsql v1.2.5";
	void init() noexcept;
	void http_server(int fd, http::request& req) noexcept;
	void update_connections(int n) noexcept;
}

#endif /* MSE_H_ */
