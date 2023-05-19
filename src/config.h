/*
 * config - load and parse /etc/cppserver/config.json - depends on json.h, logger
 *
 *  Created on: Feb 27, 2023
 *      Author: Martín Córdova cppserver@martincordova.com - https://cppserver.com
 *      Disclaimer: some parts of this library may have been taken from sample code publicly available
 *		and written by third parties. Free to use in commercial projects, no warranties and no responsibilities assumed 
 *		by the author, use at your own risk. By using this code you accept the forementioned conditions.
 */
#ifndef CONFIG_H_
#define CONFIG_H_

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <sys/stat.h>
#include <fcntl.h>
#include "json.h"
#include "logger.h"

namespace config
{

	//supported data types from http clients
	enum class inputFieldType {
							FIELD_INTEGER = 1,
							FIELD_DOUBLE = 2,
							FIELD_STRING = 3,
							FIELD_DATE = 4 //yyyy-mm-dd
						};

	struct requestParameters {

		public:

			struct inputParam {
				std::string name;
				bool required;
				inputFieldType datatype;
				std::string value;

				inputParam(std::string n, bool r, inputFieldType d): name{n}, required{r}, datatype{d}, value{""} {  }
				inputParam(std::string n, bool r, inputFieldType d, std::string v): name{n}, required{r}, datatype{d}, value{v} { }
				inputParam(inputParam&& other) : name{other.name}, required{other.required}, datatype{other.datatype} { 	}

				inputParam(const inputParam& other) {
					name = other.name;
					required = other.required;
					datatype = other.datatype;
					value = other.value;
				}

			};

			requestParameters( ) { }
			requestParameters(std::vector<inputParam>& inputs): params{std::move(inputs)} { }

			inline std::string get(const std::string& name) const 
			{
				for (const inputParam& p:params) if (p.name==name) return p.value;
					throw std::runtime_error("requestParameters.get: parameter not found: " + name);
			}

			inline void set(const std::string& name, const std::string& value) 
			{
				for (inputParam& p:params) if (p.name==name) { p.value = value; return;}
				throw std::runtime_error("requestParameters.set: parameter not found: " + name);
			}

			inline std::vector<inputParam>& list() 
			{
				return params;
			}

			inline std::string sql(const std::string& sqlTemplate, const std::string& userlogin = "Undefined") const 
			{
				std::string sql = sqlTemplate;
				if (std::size_t pos = sql.find("$userlogin"); pos != std::string::npos)
					sql.replace(pos, std::string("$userlogin").length(), "'" + userlogin + "'");
				for (const auto& p : params) 
				{
					std::string paramName = "$" + p.name;
					while (true) 
					{
						if (std::size_t pos = sql.find(paramName); pos != std::string::npos) {
							if (p.value.empty()) {
								sql.replace(pos, paramName.length(), "NULL");
								continue;
							}
							switch (p.datatype) {
								case inputFieldType::FIELD_INTEGER:
								case inputFieldType::FIELD_DOUBLE:
									sql.replace(pos, paramName.length(), p.value);
									break;
								default:
									sql.replace(pos, paramName.length(), "'" + p.value + "'");
									break;
							}
						} else
							break;
					}
				}
				return sql;
			}

			inline std::string get_audit_msg(const std::string& audit_template) const 
			{
				std::string record = audit_template;
				for (const auto& p : params) 
				{
					std::string paramName = "$" + p.name;
					while (true) 
					{
						if (std::size_t pos = record.find(paramName); pos != std::string::npos) {
							if (p.value.empty()) 
								record.replace(pos, paramName.length(), "NULL");
							else
								record.replace(pos, paramName.length(), p.value);
						} else
							break;
					}
				}
				return record;
			}

		private:
			std::vector<inputParam> params;

	};
	
	struct microService 
	{
		std::string db;
		std::string sql;
		bool secure {true};
		requestParameters reqParams;
		std::vector<std::string> varNames; //array names when returning multiple arrays
		std::vector<std::string> roleNames; //authorized roles
		struct validator {
			std::string sql;
			std::string id;
			std::string description;
		} validatorConfig;
		std::string func_service;
		std::string func_validator;
		std::function<void(std::string& jsonResp, microService&)> serviceFunction;
		std::function<void(std::string& jsonResp, microService&)> customValidator;
		bool audit_enabled {false};
		std::string audit_record;
	};
	
	void parse();
	std::unordered_map<std::string, microService> get_config_map() noexcept;
}

#endif /* LOGIN_H_ */
