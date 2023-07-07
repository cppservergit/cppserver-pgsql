#include "config.h"

namespace config 
{
	const std::string LOGGER_SRC {"config"};
	static std::unordered_map<std::string, microService> m_services;
	
	std::unordered_map<std::string, microService> get_config_map() noexcept
	{
		return m_services;
	}

	struct line_reader {
	  public:
		bool eof() { return _eof; }
		int get_line_number() { return line_number; }
		line_reader(std::string_view str) : buffer{str} { }
		
		std::string_view getline() {
			if (auto newpos = buffer.find(line_sep, pos); newpos != std::string::npos && newpos!= 0) {
				std::string_view line { buffer.substr( pos, newpos - pos ) };
				pos = newpos + line_sep.size();
				line_number++;
				return line;
			} else {
				_eof = true;
				return "";
			}
		}
		
	  private:
		std::string_view buffer;
		bool _eof{false};
		int pos{0};
		int line_number{0};
		const std::string line_sep{"\n"};
	};

	std::string_view get_value(std::string_view s)
	{
		std::string_view value;
		auto pos {s.find(":")};
		if (pos != std::string::npos) {
			auto pos1 {s.find("\"", pos)};
			if (pos1 != std::string::npos) {
				auto pos2 {s.find("\"", pos1 + 1)};
				value = s.substr(pos1 + 1, pos2 - pos1 - 1);
			} else {
				auto v {s.substr(pos + 1)};
				if (v.back() == ',')
					value = v.substr(0, v.size() - 1);
				else
					value = v;
			}
		}
		
		auto pos1 {value.find_first_not_of(" ")};
		auto pos2 {value.find_last_not_of(" ")};
		if (pos1 == std::string::npos || pos2 == std::string::npos)
			return "";
		else
			return value.substr(pos1, pos2 - pos1 + 1);
	}

	std::string_view get_attribute(std::string_view s, const std::string& name)
	{
		auto key {"\"" + name + "\":"};
		std::string_view value;
		auto pos {s.find(key)};
		if (pos != std::string::npos) {
			pos += key.size();
			auto pos1 {s.find("\"", pos)};
			if (pos1 != std::string::npos) {
				auto pos2 {s.find("\"", pos1 + 1)};
				value = s.substr(pos1 + 1, pos2 - pos1 - 1);
			}
		}
		return value;
	}

	std::vector<std::string> get_names(std::string_view s, std::string key_value)
	{
		std::vector<std::string> items;
		std::string key {"\"" + key_value + "\":"};
		std::string_view value;
		size_t pos {0};
		while (true) {
			pos = s.find(key, pos);
			if (pos != std::string::npos) {
				pos += key.size();
				auto pos1 {s.find("\"", pos)};
				if (pos1 != std::string::npos) {
					auto pos2 {s.find("\"", pos1 + 1)};
					value = s.substr(pos1 + 1, pos2 - pos1 - 1);
					items.push_back(std::string(value));
					pos = pos2;
				}
			} else
				break;
		}
		return items;
	}

	void parse() {
		try {
			logger::log(LOGGER_SRC, "info", "parsing /etc/cppserver/config.json");
			m_services.reserve(50);

			std::stringstream buffer;
			{
				std::ifstream file{"/etc/cppserver/config.json"};
				if (file.is_open())
					buffer << file.rdbuf();
				else {
					logger::log(LOGGER_SRC, "error", "aborting process, cannot open config.json");
					exit(-1);
				}
			}
			
			line_reader lr{buffer.view()};
			lr.getline(); lr.getline(); lr.getline();
			while (!lr.eof()) {
				auto s = lr.getline();
				if (s.starts_with("\t\t{")) {
					microService m;
					std::string uri{""};
					while (true) {
						auto s = lr.getline();
						if (s.starts_with("\t\t\t\"db\":"))
							m.db = get_value(s);
						if (s.starts_with("\t\t\t\"uri\":"))
							uri = get_value(s);
						if (s.starts_with("\t\t\t\"sql\":"))
							m.sql = get_value(s);
						if (s.starts_with("\t\t\t\"function\":"))
							m.func_service = get_value(s);
						if (s.starts_with("\t\t\t\"secure\":"))
							m.secure = (get_value(s) == "0") ? false : true;
						if (s.starts_with("\t\t}"))
							break;
						if (s.starts_with("\t\t\t\"fields\":")) {
							std::vector<requestParameters::inputParam> params;
							params.reserve(10);
							std::string name{""}, type{""}, required{""};
							while (true) 
							{
								auto s = lr.getline();
								if (s.contains("]")) break;
								if (s.contains("\"name\":")) 
									name = get_attribute(s, "name");	
								if (s.contains("\"type\":")) 
									type = get_attribute(s, "type");
								if (s.contains("\"required\":")) 
									required = get_attribute(s, "required");
								inputFieldType dt;
								if (type=="string") dt = inputFieldType::FIELD_STRING;
								else if (type=="date") dt = inputFieldType::FIELD_DATE;
								else if (type=="double") dt = inputFieldType::FIELD_DOUBLE;
								else if (type=="int" || type=="integer") dt = inputFieldType::FIELD_INTEGER;
								else throw std::runtime_error("Invalid data type in config.json: " + type + " uri: " + uri + " line: " + std::to_string(lr.get_line_number()));
								params.emplace_back(name, (required=="true"), dt);						
							}
							m.reqParams.set_params(params);
						}
						if (s.starts_with("\t\t\t\"validator\":")) {
								if (s.contains("\"function\":")) 
									m.func_validator =  get_attribute(s, "function");				
								if (s.contains("\"sql\":"))
									m.validatorConfig.sql = get_attribute(s, "sql");
								if (s.contains("\"id\":"))
									m.validatorConfig.id = get_attribute(s, "id");
								if (s.contains("\"description\":")) 
									m.validatorConfig.description = get_attribute(s, "description");				
						}
						if (s.starts_with("\t\t\t\"roles\":")) {
							m.roleNames = get_names(s, "name");
						}
						if (s.starts_with("\t\t\t\"tags\":")) {
							m.varNames = get_names(s, "tag");
						}
						if (s.starts_with("\t\t\t\"audit\":")) {
								if (s.contains("\"enabled\":"))
									m.audit_enabled = (get_attribute(s, "enabled") == "true") ? true : false;
								if (s.contains("\"record\":"))
									m.audit_record = get_attribute(s, "record");
						}
						if (s.starts_with("\t\t\t\"email\":")) {
							while (true) {
								auto s = lr.getline();
								if (s.contains("\"enabled\":")) 
									m.email_config.enabled = (get_value(s) == "true") ? true : false;
								if (s.contains("\"to\":")) 
									m.email_config.to = get_value(s);
								if (s.contains("\"cc\":"))
									m.email_config.cc = get_value(s);
								if (s.contains("\"template\":"))
									m.email_config.body_template = get_value(s);
								if (s.contains("\"subject\":"))
									m.email_config.subject = get_value(s);
								if (s.contains("\"attachment\":")) 
									m.email_config.attachment = get_value(s);
								if (s.contains("\"attachment-filename\":")) 
									m.email_config.attachment_filename = get_value(s);				
							if (s.contains("}")) 
									break;
							}
						}
					}
					m_services.emplace(uri, m);
				}
			}				
			
			for (auto& s: m_services)
				if (!s.second.secure) 
					logger::log(LOGGER_SRC, "info", "security disabled for service " + s.first);
			
			logger::log(LOGGER_SRC, "info", "config.json parsed");
			
		} catch (const std::exception& e) {
			logger::log(LOGGER_SRC, "error", "aborting process, cannot parse config.json: " + std::string(e.what()));
			exit(-1);
		}
	}	

}

