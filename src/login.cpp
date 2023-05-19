#include "login.h"

namespace login
{
	const std::string LOGGER_SRC {"login"};
	
	struct dbutil 
	{
		PGconn* conn;
		std::string m_dbconnstr;
		
		dbutil()
		{
			m_dbconnstr = env::get_str("CPP_LOGINDB");
			conn = PQconnectdb(m_dbconnstr.c_str());
			if (PQstatus(conn) != CONNECTION_OK)
				logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " " + std::string(PQerrorMessage(conn)), true);
			else
				prepare_sql();
		}
		
		~dbutil() {
			if (conn) {
				PQfinish(conn);
			}
		}

		inline void reset_connection() noexcept
		{
			if ( PQstatus(conn) == CONNECTION_BAD ) {
				logger::log(LOGGER_SRC, "warn", std::string(__PRETTY_FUNCTION__) + " connection to database " + std::string(PQdb(conn)) + " no longer valid, reconnecting... ", true);
				PQfinish(conn);
				conn = PQconnectdb(m_dbconnstr.c_str());
				if (PQstatus(conn) != CONNECTION_OK)
					logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " error reconnecting to database " + std::string(PQdb(conn)) + " - " + std::string(PQerrorMessage(conn)), true);
				else {
					logger::log(LOGGER_SRC, "info", std::string(__PRETTY_FUNCTION__) + " connection to database " +  std::string(PQdb(conn)) + " restored", true);
					prepare_sql();
				}
			}
		}
		
		void prepare_sql() 
		{
			const std::string sql1{"select * from cpp_dblogin($1 , $2)"};
			PGresult *res1 = PQprepare(conn, "cpp_login", sql1.c_str(), 1, NULL);
			PQclear(res1);
		}
	};

	thread_local  dbutil db;

	constexpr int max_retries{3};
	
	struct user_info
	{
		std::string email{""};
		std::string display_name{""};
		std::string roles{""};
		user_info() { email.reserve(50); display_name.reserve(50); roles.reserve(255);}
	};
	
	thread_local user_info m_user;
	
	std::string get_email() noexcept {
		return m_user.email;
	}

	std::string get_display_name() noexcept {
		return m_user.display_name;
	}

	std::string get_roles() noexcept {
		return m_user.roles;
	}

		
	bool bind(const std::string& login, const std::string& password)
	{
		constexpr int EXPECTED_COLS{3};
		int retries {0};
		bool flag{false};
		int nParams = 2;
		const char *const paramValues[] = { login.c_str(), password.c_str() };
		const int paramLengths[] = { (int)login.size(), (int)password.size() };
		const int paramFormats[] = {0, 0};
		int resultFormat = 0;

	  retry:
		PGresult *res = PQexecPrepared(db.conn, "cpp_login", nParams, paramValues, paramLengths, paramFormats, resultFormat);		
		
		if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
			PQclear(res);
			if ( PQstatus(db.conn) == CONNECTION_BAD ) {
				if (retries == max_retries) {
					logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " connection retries exhausted, cannot connect to database", true);
					throw std::runtime_error("login::bind() failed, check logs for details");
				} else {
					retries++;
					db.reset_connection();
					goto retry;
				}
			} else {
				logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " " + std::string(PQerrorMessage(db.conn)), true);
				throw std::runtime_error("login::bind() failed, check logs for details");
			}
		}

		if(PQnfields(res) != EXPECTED_COLS) {
			logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " the SQL function CPP_DBLOGIN returned an invalid number of columns, the public schema in the login database is outdated", true);
			PQclear(res);
			throw std::runtime_error("login::bind() failed, check logs for details");
		}

		if ( PQntuples(res) > 0) {
			m_user.email.clear(); 
			m_user.display_name.clear();
			m_user.roles.clear();
			m_user.email.append( PQgetvalue(res, 0, 0) );
			m_user.display_name.append( PQgetvalue(res, 0, 1 ) );
			m_user.roles.append( PQgetvalue(res, 0, 2 ) );
			flag = true;
		}
		PQclear(res);
		return flag;
	}
}