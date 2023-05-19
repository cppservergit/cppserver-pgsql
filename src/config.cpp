#include "config.h"

namespace config 
{
	const std::string LOGGER_SRC {"config"};
	static std::unordered_map<std::string, microService> m_services;
	
	std::unordered_map<std::string, microService> get_config_map() noexcept
	{
		return m_services;
	}

	std::string read_file() {
		std::string filename {"/etc/cppserver/config.json"};
		int read_fd = open(filename.c_str(), O_RDONLY);
		if ( read_fd > 0 ) {
			struct stat stat_buf;
			fstat(read_fd, &stat_buf);
			std::vector<char> buffer(stat_buf.st_size);
			size_t bytes_read = read(read_fd, buffer.data(), stat_buf.st_size);
			close(read_fd);
			return std::string(buffer.data(), bytes_read);
		} else {
			logger::log(LOGGER_SRC, "error", "cannot open /etc/cppserver/config.json");
			exit(-1);
		}
	}
	
	void parse() {
		logger::log(LOGGER_SRC, "info", "parsing /etc/cppserver/config.json");
		m_services.reserve(50);
		
		json::value json;
		try {
			json = json::parse( read_file() );
		} catch (const json::exception& e) {
			std::stringstream err{"error parsing config.json file; line: "};
			err << e.line << " column: " << e.column;
			logger::log(LOGGER_SRC, "error", err.str());
			exit(-1);
		}

		const json::array &a = as_array(json["services"]);
		for(auto it = a.begin(); it != a.end(); ++it) {
			const json::value &v = *it;

			std::string db{ "" };
			if (json::has_key(v, "db"))
					db = json::to_string(v["db"]);
			std::string uri{  json::to_string(v["uri"]) };
			std::string func{  json::to_string(v["function"]) };
			std::string sql{ "" };
			if (json::has_key(v, "sql"))
				sql = json::to_string(v["sql"]);

			if (uri.empty() || func.empty()) {
				throw std::runtime_error("Bad format in service.json -> uri and function cannot be undefined");
			}
			if ( (func=="dbget" || func=="dbgetm" || func=="dbexec" ) && sql.empty() ) {
				throw std::runtime_error("Bad format in service.json -> sql cannot be undefined if using generic microservices (dbget, dbgetm or dbexec)");
			}

			bool secure {true};
			if (json::has_key(v, "secure")) {
				std::string val { json::to_string(v["secure"]) };
				if (val == "0")
					secure = false;
			}
				
			std::string valFuncName{""};
			microService::validator validator;
			if (json::has_key(v, "validator")) {
				valFuncName = json::to_string(v["validator"]["function"]);
				if (json::has_key(v["validator"], "sql"))
					validator.sql= json::to_string(v["validator"]["sql"]);
				validator.id = json::to_string(v["validator"]["id"]);
				validator.description = json::to_string(v["validator"]["description"]);
			}

			std::vector<std::string> varNames;
			if ( func=="dbgetm") {
				const json::array &arr = as_array( v["tags"] );
				for(auto it2 = arr.begin(); it2 != arr.end(); ++it2) {
					const json::value &v2 = *it2;
					varNames.emplace_back( json::to_string( v2["tag"] ) );
				}
			}

			std::vector<std::string> roleNames;
			if ( json::has_key(v, "roles") ) {
				const json::array &arr = as_array(v["roles"]);
				for(auto it2 = arr.begin(); it2 != arr.end(); ++it2) {
					const json::value &v2 = *it2;
					roleNames.emplace_back( json::to_string( v2["name"] ) );
				}
			}

			std::vector<requestParameters::inputParam> params;
			params.reserve(10);
			if (json::has_key(v, "fields")) {
				auto fields = v["fields"];
				if (!fields.is_null()) {
					const json::array &arr = as_array(fields);
					for(auto it2 = arr.begin(); it2 != arr.end(); ++it2) {
						const json::value &v2 = *it2;
						std::string name{  json::to_string(v2["name"]) };
						std::string type{  json::to_string(v2["type"]) };
						std::string required{  json::to_string(v2["required"]) };
						if (name.empty() || type.empty() || required.empty() ) {
							throw std::runtime_error("Bad format in service.json -> field attributes cannot be undefined");
						}
						inputFieldType dt;
						if (type=="string") dt  = inputFieldType::FIELD_STRING;
						else if (type=="date") dt  = inputFieldType::FIELD_DATE;
						else if (type=="double") dt  = inputFieldType::FIELD_DOUBLE;
						else if (type=="int" || type=="integer") dt  = inputFieldType::FIELD_INTEGER;
						else throw std::runtime_error("Invalid data type in service.json: " + type);
						params.emplace_back(name, (required=="true"), dt);
					}
				}
			}
			
			bool audit_enabled{false};
			std::string audit_record{""};
			if (json::has_key(v, "audit")) {
				auto audit =  v["audit"];
				audit_enabled = (json::to_string(audit["enabled"]) == "true") ? true : false;
				audit_record = json::to_string(audit["record"]);
			}

			m_services.emplace( uri, microService {db, sql, secure, requestParameters{params}, varNames, roleNames, validator , func, valFuncName, nullptr, nullptr, audit_enabled, audit_record} );
			
			if (!secure) {
				logger::log(LOGGER_SRC, "warn", "microservice " + uri + " is not secure");
			}
		}
		
		logger::log(LOGGER_SRC, "info", "config.json parsed");
	}	

}

