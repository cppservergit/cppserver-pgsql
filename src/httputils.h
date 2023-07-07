
/*
 * httputils - provides http utility abstractions for epoll server and microservice engine
 *
 *  Created on: Feb 21, 2023
 *      Author: Martin Cordova cppserver@martincordova.com - https://cppserver.com
 *      Disclaimer: some parts of this library may have been taken from sample code publicly available
 *		and written by third parties. Free to use in commercial projects, no warranties and no responsabilities assumed 
 *		by the author, use at your own risk. By using this code you accept the forementioned conditions.
 */
#ifndef HTTPUTILS_H_
#define HTTPUTILS_H_

#include <string>
#include <string_view>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <random>
#include <cstdio>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <sys/socket.h>
#include "logger.h"

namespace http
{
	const std::string blob_path {"/var/blobs/"};
	
	std::string get_content_type(const std::string& filename) noexcept;
	std::string get_response_date() noexcept;
	
	struct form_field 
	{
		std::string name;
		std::string filename;
		std::string content_type;
		std::string data;
	};

	struct response_stream {
	  public:	
		response_stream(int size);
		response_stream();
		response_stream& operator <<(std::string data);
		response_stream& operator <<(const char* data);
		response_stream& operator <<(size_t data);
		std::string_view view() noexcept;
		size_t size() noexcept;
		const char* c_str() noexcept;
		void append(const char* data, size_t len) noexcept;
		const char* data() noexcept;
		void clear() noexcept;
		bool write(int fd) noexcept; 
	  private:
		int _pos1 {0};
		std::string _buffer{""};
	};
	
	struct request {
	  public:
		int fd{0}; //socket fd
		size_t bodyStartPos{0};
		size_t contentLength{0};
		bool isMultipart{false};
		std::string method{""};
		std::string queryString{""};
		std::string path{""};
		std::string boundary{""};
		std::string cookie{""};
		int errcode{0};
		std::string errmsg{""};
		std::string remote_ip;
		std::string origin{"null"};
		std::string payload;
		std::unordered_map<std::string, std::string> headers;
		std::unordered_map<std::string, std::string> params;
		response_stream response;
		bool is_clear {true};
		request();
		void clear();
		void parse();
		bool eof();
		std::string get_header(const std::string& name) const;
	  private:
		std::string_view get_cookie(std::string_view cookieHdr);
		std::string lowercase(std::string s) noexcept;	
		std::string decode_param(const std::string &value) noexcept;
		void parse_query_string(std::string_view qs) noexcept;	
		std::string get_part_content_type(std::string value);
		std::pair<std::string, std::string> get_part_field(std::string value);
		std::vector<form_field> parse_multipart();
	};	
}

#endif /* HTTPUTILS_H_ */

