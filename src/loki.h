/*
 * loki - send log to loki using http push api, depends on env
 *
 *  Created on: Feb 21, 2023
 *      Author: Martín Córdova cppserver@martincordova.com - https://cppserver.com
 *      Disclaimer: some parts of this library may have been taken from sample code publicly available
 *		and written by third parties. Free to use in commercial projects, no warranties and no responsabilities assumed 
 *		by the author, use at your own risk. By using this code you accept the forementioned conditions.
 */
#ifndef LOKI_H_
#define LOKI_H_

#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <netinet/tcp.h>
#include <string>
#include <cstdio>
#include <thread>
#include <chrono>
#include <sstream>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <stop_token>
#include "env.h"

namespace loki
{
	void push_log_record(std::string msg) noexcept;
	void start_task() noexcept;
	void stop_task() noexcept;
}

#endif /* LOKI_H_ */
