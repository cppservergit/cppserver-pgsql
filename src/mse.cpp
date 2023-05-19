#include "mse.h"

namespace mse
{
	std::atomic<long> 		g_counter{0};
	std::atomic<double> 	g_total_time{0};
	std::atomic<int> 		g_active_threads{0};
	std::atomic<size_t> 	g_connections{0};

	void update_connections(int n) noexcept
	{
		g_connections += n;
	}
	
	inline void fileservice(http::request& req);
	inline void microservice(http::request& req);
	inline std::string get_timestamp();
	
	const std::string LOGGER_SRC {"mse"};
	const std::string m_startedOn {get_timestamp()};

	using httpRequestParameters = std::unordered_map<std::string, std::string>;

	thread_local std::unordered_map<std::string, std::vector<char>> t_page_cache; //fileserver

	/* utility functions */
	inline std::string trim(const std::string & source) {
		std::string s(source);
		s.erase(0,s.find_first_not_of(" "));
		s.erase(s.find_last_not_of(" ")+1);
		return s;
	}

	inline bool isInteger(const std::string& s)
	{
		return std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
	}

	inline bool isDouble(const std::string& s)
	{
		if (auto d = s.find("."); d!=std::string::npos) {
			std::string part1 = s.substr(0, d);
			std::string part2 = s.substr(d+1);
			if ( isInteger(part1) && isInteger(part2) )
				return true;
			else
				return false;
		} else
			return isInteger(s);
	}

	// check for valid date in format yyyy-mm-dd
	inline bool isDate(const std::string & value) {

		if (value.length() != 10)
			return false;

		std::string sd = value.substr(8, 2);
		if (!isInteger(sd)) return false;
		std::string sm = value.substr(5, 2);
		if (!isInteger(sm)) return false;
		std::string sy = value.substr(0, 4);
		if (!isInteger(sy)) return false;

		int d = std::stoi(sd);
		int m = std::stoi(sm);
		int y = std::stoi(sy);

		if (! (1<= m && m<=12) )
		 return false;
		if (! (1<= d && d<=31) )
		 return false;
		if ( (d==31) && (m==2 || m==4 || m==6 || m==9 || m==11) )
		 return false;
		if ( (d==30) && (m==2) )
		 return false;
		if ( (m==2) && (d==29) && (y%4!=0) )
		 return false;
		if ( (m==2) && (d==29) && (y%400==0) )
		 return true;
		if ( (m==2) && (d==29) && (y%100==0) )
		 return false;
		if ( (m==2) && (d==29) && (y%4==0)  )
		 return true;

		return true;

	}

	inline void replaceString(std::string &str, const std::string& from, const std::string& to) {
		if (from.empty() || to.empty())
			return;
		size_t start_pos = 0;
		while((start_pos = str.find(from, start_pos)) != std::string::npos) {
			str.replace(start_pos, from.length(), to);
			start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
		}
	}


	inline std::string get_timestamp() {
		std::time_t t = std::time(nullptr);
		char dtbuffer[21] = {'\0'};
		std::strftime(dtbuffer, 21, "%Y-%m-%d %H:%M:%S",  std::localtime(&t));
		return std::string{dtbuffer};
	}

	inline std::string replaceParam(std::string src, const std::string& oldStr, const std::string& newStr) {
		src.replace(src.find(oldStr), oldStr.length(), newStr);
		return src;
	}

	inline std::string replaceParam(std::string src, const std::vector<std::string>& oldStr, const std::vector<std::string>& newStr) {
		for (std::size_t i=0;i<oldStr.size();++i) {
			src.replace(src.find(oldStr[i]), oldStr[i].length(), newStr[i]);
		}
		return src;
	}

	/* end utility functions */

	class LoginRequiredException : public std::exception {
	public:
		const char * what () {
			return "security session not found.";
		}
	};

	struct userThreadInfo {
		void clear() noexcept {
			sessionID = "";
			ipAddr = "";
			userLogin = "";
			userMail = "";
			contentType = "";
			fileName = "";
			roles = "";
		}
		std::string sessionID{""};
		std::string ipAddr{""};
		std::string userLogin{""};
		std::string userMail{""};
		std::string contentType{""};
		std::string fileName{""};
		std::string roles{""};
	} thread_local t_user_info;

	//security session support
	inline bool sessionUpdate() noexcept {
		if (t_user_info.sessionID.empty())
			return false;

		if ( session::update(t_user_info.sessionID, t_user_info.userLogin, t_user_info.userMail, t_user_info.roles) ) {
			return true;
		} else {
			return false;
		}
	}

	inline void sessionCreate(const std::string& userid, const std::string& mail,  const std::string& roles) {
		std::string sessionid{""};
		if ( session::create(sessionid, userid, mail, t_user_info.ipAddr, roles) ) {
			t_user_info.sessionID = sessionid;
			t_user_info.userLogin = userid;
		}  else {
			t_user_info.sessionID = "";
				throw std::runtime_error("mse::sessionCreate() failed.");
		}
	}

	//generic JSON microservices

	//returns a single resultset
	void dbget(std::string& jsonBuffer, config::microService& ms) 
	{
		sql::get_json(ms.db, jsonBuffer, ms.reqParams.sql(ms.sql, t_user_info.userLogin));
	}

	//returns multiple resultsets from a single query
	void dbgetm(std::string& jsonBuffer, config::microService& ms) {
		sql::get_json(ms.db, jsonBuffer, ms.reqParams.sql(ms.sql, t_user_info.userLogin), ms.varNames);
	}

	//execute data modification query (insert, update, delete) with no resultset returned
	void dbexec(std::string& jsonBuffer, config::microService& ms) {

		constexpr char STATUS_OK[] = "{\"status\": \"OK\"}";
		constexpr char STATUS_ERROR[] = "{\"status\": \"ERROR\",\"description\" : \"System error\"}";

		if (sql::exec_sql(ms.db, ms.reqParams.sql(ms.sql, t_user_info.userLogin)))
			jsonBuffer.append(STATUS_OK);
		else
			jsonBuffer.append(STATUS_ERROR);
	}

	//login and create session
	void login(std::string& jsonBuffer, config::microService& ms) 
	{
		const std::string errInvalidLogin = R"({"status": "INVALID", "validation": {"id": "login", "description": "$err.invalidcredentials"}})";
		const std::string loginOK = R"({"status": "OK", "data":[{"displayname":"$displayname"}]})";

		std::string login{ ms.reqParams.get("login") };
		std::string password{ ms.reqParams.get("password") };

		if (login::bind( login, password)) {
			sessionCreate(login, login::get_email(), login::get_roles());
			jsonBuffer.append(replaceParam(loginOK, "$displayname", login::get_display_name()));
			if (env::login_log_enabled())
				logger::log("security", "info", "login OK - user: " + login + " IP: " + t_user_info.ipAddr + " sessionid: " + t_user_info.sessionID + " roles: " + login::get_roles(), true);
		} else {
			logger::log("security", "warn", "login failed - user: " + login + " IP: " + t_user_info.ipAddr, true);
			t_user_info.sessionID = "";
			jsonBuffer.append(  errInvalidLogin );
		}
	}

	//kill session
	void logout(std::string& jsonBuffer, config::microService& ms) {
		constexpr char STATUS_OK[] = "{\"status\": \"OK\"}";
		session::remove(t_user_info.sessionID);
		jsonBuffer.append( STATUS_OK );
	}

	//return server status
	void getServerInfo(std::string& jsonBuffer, config::microService& ms) {

		char hostname[100]; gethostname(hostname, sizeof(hostname));
		const double avg{ ( g_counter > 0 ) ? g_total_time / g_counter : 0 };
		char str1[20] = {'\0'}; std::to_chars(&str1[0], &str1[19], g_counter);
		char str2[20] = {'\0'}; std::to_chars(&str2[0], &str2[19], avg, std::chars_format::fixed, 8);
		char str3[20] = {'\0'}; std::to_chars(&str3[0], &str3[19], g_connections);
		char str4[20] = {'\0'}; std::to_chars(&str4[0], &str4[19], g_active_threads);
		
		char outputBuffer[512]= {'\0'};
		constexpr char json[] = "{\"status\": \"OK\", \"data\":[{\"pod\": \"%s\", \"totalRequests\": %s, \"avgTimePerRequest\": %s, \"startedOn\": \"%s\", \"connections\": %s, \"activeThreads\": %s}]}";
		sprintf( outputBuffer, json, hostname, str1, str2, m_startedOn.c_str(), str3, str4 );
		jsonBuffer.append( outputBuffer );

	}

	//return server metrics for Prometheus
	void getMetrics(std::string& jsonBuffer, config::microService& ms) {

		t_user_info.contentType = "text/plain; version=0.0.4";
		char hostname[100]; gethostname(hostname, sizeof(hostname));
		const double avg{ ( g_counter > 0 ) ? g_total_time / g_counter : 0 };
		char str1[20] = {'\0'}; std::to_chars(&str1[0], &str1[19], g_counter);
		char str2[20] = {'\0'}; std::to_chars(&str2[0], &str2[19], avg, std::chars_format::fixed, 8);
		char str3[20] = {'\0'}; std::to_chars(&str3[0], &str3[19], g_connections);
		char str4[20] = {'\0'}; std::to_chars(&str4[0], &str4[19], g_active_threads);
		
		char help1[] = "# HELP cpp_requests_total The number of HTTP requests processed by this container.\n";
		char type1[] = "# TYPE cpp_requests_total counter\n";
		char metric1[] = "cpp_requests_total{pod=\"%s\"} %s\n";
		char output1[128]= {'\0'};
		sprintf(output1, metric1, hostname, str1);

		char help2[] = "# HELP cpp_connections Client tcp-ip connections.\n";
		char type2[] = "# TYPE cpp_connections counter\n";
		char metric2[] = "cpp_connections{pod=\"%s\"} %s\n";
		char output2[128]= {'\0'};
		sprintf(output2, metric2, hostname, str3);

		char help3[] = "# HELP cpp_active_threads Active threads.\n";
		char type3[] = "# TYPE cpp_active_threads counter\n";
		char metric3[] = "cpp_active_threads{pod=\"%s\"} %s\n";
		char output3[128]= {'\0'};
		sprintf(output3, metric3, hostname, str4);

		char help4[] = "# HELP cpp_avg_time Average request processing time in milliseconds.\n";
		char type4[] = "# TYPE cpp_avg_time counter\n";
		char metric4[] = "cpp_avg_time{pod=\"%s\"} %s\n";
		char output4[128]= {'\0'};
		sprintf(output4, metric4, hostname, str2);

		char help5[] = "# HELP sessions Number of logged in users.\n";
		char type5[] = "# TYPE sessions counter\n";
		char metric5[] = "sessions{pod=\"%s\"} %d\n";
		char output5[128]= {'\0'};
		sprintf(output5, metric5, hostname, session::get_total());
		
		jsonBuffer.append(help1); jsonBuffer.append(type1); jsonBuffer.append(output1);
		jsonBuffer.append(help2); jsonBuffer.append(type2); jsonBuffer.append(output2);
		jsonBuffer.append(help3); jsonBuffer.append(type3); jsonBuffer.append(output3);
		jsonBuffer.append(help4); jsonBuffer.append(type4); jsonBuffer.append(output4);
		jsonBuffer.append(help5); jsonBuffer.append(type5); jsonBuffer.append(output5);

	}

	//active sessions in table cpp_session
	void getSessionCount(std::string& jsonBuffer, config::microService& ms) {

		char outputBuffer[64]= {'\0'};
		constexpr char json[] = "{\"status\": \"OK\", \"data\":[{\"total\": %d}]}";
		sprintf( outputBuffer, json, session::get_total() );
		jsonBuffer.append( outputBuffer );

	}

	//download file from filesystem given its ID from DB 
	void downloadFile(std::string& jsonBuffer, config::microService& ms) {
		
		auto rec = sql::get_record(ms.db, ms.reqParams.sql(ms.sql, t_user_info.userLogin));
		
		if ( rec.size() ) {
			t_user_info.fileName = rec.at("filename");
			t_user_info.contentType = rec.at("content_type");
			std::string path{http::blob_path + rec.at("document")};
			int read_fd = open(path.c_str(), O_RDONLY);
			if ( read_fd > 0 ) {
				struct stat stat_buf;
				fstat(read_fd, &stat_buf);
				std::vector<char> buffer(stat_buf.st_size);
				size_t bytes_read = read(read_fd, buffer.data(), stat_buf.st_size);
				jsonBuffer.append(buffer.data(), bytes_read);
				close(read_fd);
			} else {
				logger::log(LOGGER_SRC, "error", "downloadFile()/open() failed - user: " + t_user_info.userLogin 
					+ " uri: " + path, true); 
				std::string error{"Error downloading file: " + t_user_info.fileName + " with ID: " + rec.at("document")};
				t_user_info.fileName = "error.txt";
				t_user_info.contentType = "text/plain";
				jsonBuffer.append(error);
			}
		}

	}

	//delete blob's record from database and the blob's file from filesystem
	void deleteFile(std::string& jsonBuffer, config::microService& ms) {
		
		constexpr char STATUS_OK[] = "{\"status\": \"OK\"}";
		constexpr char STATUS_ERROR[] = "{\"status\": \"ERROR\",\"description\" : \"System error\"}";
		
		std::string sql{"select document from demo.blob where blob_id = $blob_id"};
		auto rec = sql::get_record(ms.db, ms.reqParams.sql(sql, t_user_info.userLogin));
		if ( rec.size() ) {
			std::string path{http::blob_path + rec.at("document")};
			if ( sql::exec_sql(ms.db, ms.reqParams.sql(ms.sql, t_user_info.userLogin))) {
				std::remove( path.c_str() );
				jsonBuffer.append( STATUS_OK );
			} else {
				jsonBuffer.append( STATUS_ERROR );
			}
		} else {
			jsonBuffer.append( STATUS_ERROR );
		}
	}

	//minimal service used to measure performance overhead of the dispatching mechanism
	void get_version(std::string& jsonBuffer, config::microService& ms) {
		char outputBuffer[512]= {'\0'};
		constexpr char json[] = "{\"status\": \"OK\", \"data\":[{\"pod\": \"%s\", \"server\": \"%s-%d\"}]}";
		char hostname[100];
		gethostname(hostname, sizeof(hostname));
		sprintf(outputBuffer, json, hostname, SERVER_VERSION, CPP_BUILD_DATE);
		jsonBuffer.append(outputBuffer);
	}

	//minimal service used to ping/keepalive all database connections held by this thread
	void ping(std::string& jsonBuffer, config::microService& ms) {
		constexpr char json[] = "{\"status\": \"OK\"}";
		jsonBuffer.append(json);
	}

	//generic custom validators

	//if the resultset return 1+ row then fail (jsonResp will contain something)
	void dbfind(std::string &jsonResp, config::microService& ms) {

		const std::string STATUS_ERROR = R"(
		{
			"status": "INVALID",
			"validation": {
				"id": "$id",
				"description": "$description"
			}
		}
		)";

		if (sql::has_rows(ms.db, ms.reqParams.sql(ms.validatorConfig.sql)))
			jsonResp = replaceParam( STATUS_ERROR, { "$id", "$description" }, { ms.validatorConfig.id, ms.validatorConfig.description } );

	}

	inline auto getFunctionPointer(const std::string& funcName) {
		if (funcName=="dbget")
			return dbget;
		if (funcName=="dbgetm")
			return dbgetm;
		if (funcName=="dbexec")
			return dbexec;
		if (funcName=="login")
			return login;
		if (funcName=="logout")
			return logout;
		if (funcName=="getServerInfo")
			return getServerInfo;
		if (funcName=="download")
			return downloadFile;
		if (funcName=="deleteFile")
			return deleteFile;
		if (funcName=="get_version")
			return get_version;
		if (funcName=="ping")
			return ping;
		if (funcName=="getSessionCount")
			return getSessionCount;
		if (funcName=="getMetrics")
			return getMetrics;

		throw std::runtime_error("getFunctionPointer -> invalid function name: " + funcName);
	}

	inline auto getValidatorFunctionPointer(const std::string& funcName) {
		if (funcName=="dbfind")
			return dbfind;

		throw std::runtime_error("getValidatorFunctionPointer -> invalid function name: " + funcName);
	}

	inline void validateRequestParams( std::string &jsonResp,  const httpRequestParameters& httpReq, config::requestParameters& params ) {

		const std::string errMsgRequired = R"(
		{
			"status": "INVALID",
			"validation": {
				"id": "$1",
				"description": "$err.required"
			}
		}
		)";

		const std::string errMsgDataType = R"(
		{
			"status": "INVALID",
			"validation": {
				"id": "$1",
				"description": "$err.invalidtype"
			}
		}
		)";

		for (auto &p : params.list()) {

			std::string value{""};

			if (auto ptr = httpReq.find(p.name); ptr != httpReq.end())
				value = ptr->second;

			if (!value.empty())
				value = trim(value);

			if ( p.required && value.empty() ) {
				jsonResp = replaceParam(errMsgRequired, "$1", p.name);
				return;
			}

			if (!value.empty()) {
				std::string errDataType = errMsgDataType;
				switch (p.datatype) {
					case config::inputFieldType::FIELD_INTEGER:
						if (!isInteger(value)) {
							jsonResp = replaceParam(errDataType, "$1", p.name);
							return;
						}
						break;

					case config::inputFieldType::FIELD_DOUBLE:
						if (!isDouble(value)) {
							jsonResp = replaceParam(errDataType, "$1", p.name);
							return;
						}
						break;

					case config::inputFieldType::FIELD_DATE:
						if (!isDate(value)) {
							jsonResp = replaceParam(errDataType, "$1", p.name);
							return;
						}
						break;

					default: //string type
						replaceString(value, "'", "''");
						replaceString(value, "\\", "");
						break;
				}
			}

			p.value = value;

		}

		return;
	}

	inline bool is_user_in_role(const std::vector<std::string>& authorized_roles, const std::string& user_roles) noexcept
	{
		if (user_roles.empty()) return false;
		for (const auto& r: authorized_roles) {
			if (user_roles.find(r) != std::string::npos)
				return true;
		}
		return false;
	}

	inline std::string validateInputs(const std::string& path, const httpRequestParameters& httpReq, config::microService& ms) {

		std::string validationErrors;

		if ( ms.roleNames.size() && !t_user_info.userLogin.empty()) {
			if (!is_user_in_role(ms.roleNames, t_user_info.roles)) {
				logger::log("security", "warn", "access denied, insufficient security credentials - user: " + t_user_info.userLogin 
					+ " uri: " +  path 
					+ " user roles: " + t_user_info.roles 
					+ " IP: " + t_user_info.ipAddr, true);
				validationErrors = R"({"status": "INVALID", "validation": {"id": "_dialog_", "description": "$err.accessdenied"}})";
				return validationErrors;
			}
		}

		if (ms.reqParams.list().empty())
			return "";

		validateRequestParams(validationErrors, httpReq, ms.reqParams);

		if ( !validationErrors.empty() ) {
			return validationErrors;
		} else {
			if ( ms.customValidator ) {
				ms.customValidator( validationErrors, ms );
				if ( !validationErrors.empty() ) {
					return validationErrors;
				}
			}
			return "";
		}
	}

	struct service_engine {
	  public:
		service_engine() {
			m_json_buffer.reserve(32767);
			m_service_map = config::get_config_map();
			for (auto& m: m_service_map) 
			{
				m.second.serviceFunction = getFunctionPointer(m.second.func_service);
				if (!m.second.func_validator.empty())
					m.second.customValidator = getValidatorFunctionPointer(m.second.func_validator);
			}
		}

		inline std::string& run(http::request& req) 
		{
			if (auto m = m_service_map.find( req.path ); m != m_service_map.end() ) {
				if ( m->second.secure ) {
					if ( !sessionUpdate() )
						throw LoginRequiredException();
				}
				m_json_buffer.clear();
				m_json_buffer.append( validateInputs( req.path, req.params, m->second ) );
				if (m_json_buffer.empty() ) {
					m->second.serviceFunction( m_json_buffer, m->second );
					if ( m->second.audit_enabled )
						audit::save(req.path, t_user_info.userLogin, req.remote_ip, m->second);
				}
				return m_json_buffer;
			} else {
				throw std::runtime_error("microservice path not found");
			}
		}

	  private:
		std::unordered_map<std::string, config::microService> m_service_map;
		std::string m_json_buffer;
		
	}; 
	thread_local service_engine t_service;

	inline void set_trace_headers(const http::request& req, http::response_stream& res) noexcept
	{
		if (req.headers.contains("x-request-id"))
			res << "x-request-id: " << req.get_header("x-request-id") << "\r\n";
	}

	inline void send400(http::request& req) 
	{
		logger::log(LOGGER_SRC, "error", "bad http request - IP: " + req.remote_ip + " error: " + req.errmsg, true);
		std::string msg {"Bad request"};
		http::response_stream& res = req.response;
		res << "HTTP/1.1 400 Bad request" << "\r\n"
			<< "Content-Length: " << msg.size() << "\r\n"
			<< "Content-Type: " << "text/plain" << "\r\n" 
			<< "Keep-Alive: timeout=5, max=200" << "\r\n"
			<< "Date: " << http::get_response_date() << "\r\n"
			<< "Access-Control-Allow-Origin: " << req.origin << "\r\n"
			<< "Access-Control-Allow-Credentials: true" << "\r\n"
			<< "Strict-Transport-Security: max-age=31536000; includeSubDomains; preload;" << "\r\n"
			<< "X-Frame-Options: SAMEORIGIN" << "\r\n"
			<< "\r\n"					
			<< msg;
	}

	inline void send401(http::request& req) 
	{
		std::string msg {"Please login with valid credentials"};
		http::response_stream& res = req.response;
		res << "HTTP/1.1 401 Unauthorized" << "\r\n"
			<< "Content-Length: " << msg.size() << "\r\n"
			<< "Content-Type: " << "text/plain" << "\r\n" 
			<< "Keep-Alive: timeout=5, max=200" << "\r\n"
			<< "Date: " << http::get_response_date() << "\r\n"
			<< "Access-Control-Allow-Origin: " << req.origin << "\r\n"
			<< "Access-Control-Allow-Credentials: true" << "\r\n"
			<< "Strict-Transport-Security: max-age=31536000; includeSubDomains; preload;" << "\r\n"
			<< "X-Frame-Options: SAMEORIGIN" << "\r\n"
			<< "\r\n"					
			<< msg;
	}

	inline void send404(http::request& req) {
		
		std::string msg {"Resource not found"};
		http::response_stream& res = req.response;
		res << "HTTP/1.1 404 Not found" << "\r\n"
			<< "Content-Length: " << msg.size() << "\r\n"
			<< "Content-Type: " << "text/plain" << "\r\n" 
			<< "Keep-Alive: timeout=5, max=200" << "\r\n"
			<< "Date: " << http::get_response_date() << "\r\n"
			<< "Access-Control-Allow-Origin: " << req.origin << "\r\n"
			<< "Access-Control-Allow-Credentials: true" << "\r\n"
			<< "Strict-Transport-Security: max-age=31536000; includeSubDomains; preload;" << "\r\n"
			<< "X-Frame-Options: SAMEORIGIN" << "\r\n"
			<< "\r\n"					
			<< msg;

	}

	inline void sendRedirect(http::request& req, std::string newPath) {
		std::string msg {"301 Moved permanently"};
		http::response_stream& res = req.response;
		res << "HTTP/1.1 301 Moved permanently" << "\r\n"
			<< "Location: " << newPath << "\r\n" 
			<< "Keep-Alive: timeout=5, max=200" << "\r\n"
			<< "Content-Length: " << msg.size() << "\r\n"
			<< "Content-Type: " << "text/plain" << "\r\n" 
			<< "Date: " << http::get_response_date() << "\r\n"
			<< "Access-Control-Allow-Origin: " << req.origin << "\r\n"
			<< "Access-Control-Allow-Credentials: true" << "\r\n"
			<< "Strict-Transport-Security: max-age=31536000; includeSubDomains; preload;" << "\r\n"
			<< "X-Frame-Options: SAMEORIGIN" << "\r\n"
			<<  "\r\n"
			<< msg;
	}

	inline void microservice(http::request& req) {

		if (req.errcode == -1) {
			send400(req);
			return;
		}

		http::response_stream& res = req.response;
		const std::string cookieName{"CPPSESSIONID="};
		constexpr int CPPSERVER_PATHSIZE_ALERT{75};
		const std::string SYSERROR_RUNTIME	{R"({"status": "ERROR", "description": "Server runtime error"})"};
		const std::string json_encoding {"application/json"};

		t_user_info.clear();
		t_user_info.ipAddr = req.remote_ip;
		t_user_info.sessionID = req.cookie;

		try {

			if (req.path.size() > CPPSERVER_PATHSIZE_ALERT)
				throw std::runtime_error("Invalid path length - buffer overflow attack?");

			std::string& jsonOutput = t_service.run( req );
			std::string contentType{ (t_user_info.contentType.empty()) ? json_encoding : t_user_info.contentType };
			
			res	<< "HTTP/1.1 200 OK" << "\r\n"
				<< "Content-Length: " << jsonOutput.size() << "\r\n" 
				<< "Content-Type: " << contentType << "\r\n" 
				<< "Date: " << http::get_response_date() << "\r\n" 
				<< "Keep-Alive: timeout=5, max=200" << "\r\n"
				<< "Access-Control-Allow-Origin: " << req.origin << "\r\n"
				<< "Access-Control-Allow-Credentials: true" << "\r\n"
				<< "Access-Control-Expose-Headers: content-disposition" << "\r\n"
				<< "Strict-Transport-Security: max-age=31536000; includeSubDomains; preload;" << "\r\n"
				<< "X-Frame-Options: SAMEORIGIN" << "\r\n";
			
			if (!t_user_info.fileName.empty()) {
				std::string disposition{"attachment; filename=\"" + t_user_info.fileName + "\";"};
				res << "Content-Disposition: " << disposition << "\r\n";
			}
			
			if ( !t_user_info.sessionID.empty() && req.path=="/ms/login" ) {
				std::string cookieHdr = cookieName + t_user_info.sessionID + "; Path=/; SameSite=None; Secure; HttpOnly";
				res << "Set-Cookie: " << cookieHdr << "\r\n";
			}
			
			set_trace_headers(req, res);
			res << "\r\n" << jsonOutput;
			
		} catch (const LoginRequiredException&) {
			logger::log("security", "error", "security session not found - IP: " + req.remote_ip + " cookie: " + req.cookie + " uri: " + req.path, true);
			send401(req);
		} catch (const std::exception& e) {
			if (!req.path.ends_with(".ico"))
				logger::log(LOGGER_SRC, "error", std::string(e.what()) + " uri: " + req.path + " user: " + t_user_info.userLogin, true);
					
			res << "HTTP/1.1 200 OK" << "\r\n"
				<< "Content-Length: " << SYSERROR_RUNTIME.size() << "\r\n" 
				<< "Content-Type: " << json_encoding << "\r\n" 
				<< "Date: " << http::get_response_date() << "\r\n" 
				<< "Keep-Alive: timeout=5, max=200" << "\r\n"
				<< "Access-Control-Allow-Origin: " << req.origin << "\r\n"
				<< "Access-Control-Allow-Credentials: true" << "\r\n"
				<< "Strict-Transport-Security: max-age=31536000; includeSubDomains; preload;" << "\r\n"
				<< "X-Frame-Options: SAMEORIGIN" << "\r\n";		

			set_trace_headers(req, res);
			res << "\r\n" << SYSERROR_RUNTIME;
		}

	}

	inline void fileservice(http::request& req) 
	{
		if (req.errcode == -1) {
			send400(req);
			return;
		}
		
		std::string root_dir{ "/var/www" };
		std::string target = root_dir + req.path;
		
		if (target.back()=='/')
			target += "index.html";
		
		if (std::filesystem::is_directory(std::filesystem::status(target))) {
			target = req.path + "/index.html";
			sendRedirect(req, target);
			return;
		}

		if (!t_page_cache.contains(target)) {
			int read_fd;
			struct stat stat_buf;
			read_fd = open(target.c_str(), O_RDONLY);
			if (read_fd == -1) {
				send404(req);
				return;
			}
			if (t_page_cache.size()==0) {
				t_page_cache.reserve(50);
			}
			fstat(read_fd, &stat_buf); 
			std::vector<char> buffer(stat_buf.st_size);	
			size_t bytes_read = read(read_fd, buffer.data(), stat_buf.st_size);
			if (bytes_read < (size_t)stat_buf.st_size)
				logger::log(LOGGER_SRC, "error", "fileserver - read() did not retrieve the whole file, file size: " + std::to_string(stat_buf.st_size) + " bytes read: " + std::to_string(bytes_read), true);
			close(read_fd);
			t_page_cache.insert({target, std::move(buffer)});
		}
		std::vector<char>& page = t_page_cache[target];
		
		http::response_stream& res = req.response;
		res	<< "HTTP/1.1 200 OK" << "\r\n"
			<< "Content-Length: " << page.size() << "\r\n" 
			<< "Content-Type: " << http::get_content_type(target) << "\r\n"
			<< "Date: " << http::get_response_date() << "\r\n"
			<< "Keep-Alive: timeout=5, max=200" << "\r\n"
			<< "Cache-Control: max-age=3600" << "\r\n" 
			<< "Access-Control-Allow-Origin: " << req.origin << "\r\n"
			<< "Access-Control-Allow-Credentials: true" << "\r\n"
			<< "Access-Control-Expose-Headers: content-disposition" << "\r\n"
			<< "Strict-Transport-Security: max-age=31536000; includeSubDomains; preload;" << "\r\n"
			<< "X-Frame-Options: SAMEORIGIN" << "\r\n";
			set_trace_headers(req, req.response);
		res	<< "\r\n";
		res.append(page.data(), page.size());
	}

	void init() noexcept
	{
		logger::log(LOGGER_SRC, "info", "starting microservice engine", true);
		for (int i = 1; i <= 10; i++) 
		{
			std::string connstr{env::get_str("CPP_DB" + std::to_string(i))};
			if (!connstr.empty())
				sql::connect("db" + std::to_string(i), connstr);
			else
				break;
		}
	}

	void http_server(int fd, http::request& req) noexcept
	{
		++g_active_threads;	

		logger::set_request_id(req.get_header("x-request-id"));

		auto start = std::chrono::high_resolution_clock::now();

		if (req.path.starts_with("/ms/"))
			microservice(req);
		else
			fileservice(req);

		auto finish = std::chrono::high_resolution_clock::now();
		std::chrono::duration <double>elapsed = finish - start;				

		if (env::http_log_enabled())
			logger::log("access-log", "info", "fd=" + std::to_string(fd) + " remote-ip=" + req.remote_ip + " path=" + req.path + " elapsed-time=" + std::to_string(elapsed.count()) + " cookie=" + req.cookie, true);

		logger::set_request_id("");

		g_total_time += elapsed.count();
		++g_counter;
		--g_active_threads;
	}
	
}
