#include "env.h"

namespace env 
{
	struct env_vars {
			env_vars();
			int read_env(const char* name, int default_value) noexcept;
			std::string loki_server{""};
			int loki_port{read_env("CPP_LOKI_PORT", 3100)};
			int port{read_env("CPP_PORT", 8080)};
			int http_log{read_env("CPP_HTTP_LOG", 0)};
			int stderr_log{read_env("CPP_STDERR_LOG", 1)};
			int login_log{read_env("CPP_LOGIN_LOG", 0)};
			int pool_size{read_env("CPP_POOL_SIZE", 4)};
	};	
	
	env_vars ev;
	
	env_vars::env_vars() 
	{
		if (const char* env_p = std::getenv("CPP_LOKI_SERVER"))
			loki_server.append(env_p);				
	}

	int env_vars::read_env(const char* name, int default_value) noexcept
	{
		if (const char* env_p = std::getenv(name)) 
			return atoi(env_p);
		else
			return default_value;
	}

	std::string get_str(std::string name) noexcept
	{
		if (const char* env_p = std::getenv(name.c_str()))
			return std::string(env_p);
		else 
			return "";
	}

	bool loki_enabled() noexcept
	{ return (ev.loki_server.empty() ? false : true); }

	std::string loki_server() noexcept
	{ return ev.loki_server; }

	int loki_port() noexcept 
	{ return ev.loki_port; }

	int port() noexcept 
	{ return ev.port; }

	int http_log_enabled() noexcept 
	{ return ev.http_log; }

	int stderr_log_enabled() noexcept 
	{ return ev.stderr_log; }

	int pool_size() noexcept 
	{ return ev.pool_size; }

	int login_log_enabled() noexcept 
	{ return ev.login_log; }
}
