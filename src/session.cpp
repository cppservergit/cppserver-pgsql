#include "session.h"

namespace session
{
	const std::string LOGGER_SRC {"session"};
	
	struct dbutil 
	{
		PGconn* conn;
		std::string m_dbconnstr;
		
		dbutil()
		{
			m_dbconnstr = env::get_str("CPP_SESSIONDB");
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
			const std::string sql1{"select * from cpp_session_update($1, $2)"};
			PGresult *res1 = PQprepare(conn, "update_session", sql1.c_str(), 1, NULL);
			PQclear(res1);
			
			const std::string sql2{ "select * from cpp_session_create($1, $2, $3, $4)" };
			PGresult *res2 = PQprepare(conn, "create_session", sql2.c_str(), 1, NULL);
			PQclear(res2);				
		}
	};

	thread_local  dbutil db;
	constexpr int max_retries{3};
	constexpr char DBLIB_ERROR[] = "{\"status\": \"ERROR\",\"description\" : \"database error, the details were stored in the stderr log\"}";

	bool create(std::string& sessionid, const std::string& userlogin, const std::string& mail, const std::string& ip, const std::string& roles) noexcept
	{
		bool flag{false};
		int retries {0};
		sessionid.reserve(63); sessionid.clear();

		int nParams = 4;
		const char *const paramValues[] = { userlogin.c_str(), mail.c_str(), ip.c_str(), roles.c_str() };
		const int paramLengths[] = { (int)userlogin.size(), (int)mail.size(), (int)ip.size(), (int)roles.size() };
		const int paramFormats[] = {0, 0, 0, 0};
		int resultFormat = 0;
	  
	  retry:
		PGresult *res = PQexecPrepared(db.conn, "create_session", nParams, paramValues, paramLengths, paramFormats, resultFormat);
		if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
			PQclear(res);
			if ( PQstatus(db.conn) == CONNECTION_BAD ) {
				if (retries == max_retries) {
					logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " connection retries exhausted, cannot connect to database", true);
					return false;
				} else {
					retries++;
					db.reset_connection();
					goto retry;
				}
			} else {
				logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " " + std::string(PQerrorMessage(db.conn)), true);
				return false;
			}
		}
		
		if ( PQntuples(res) > 0) {
			sessionid.append( PQgetvalue(res, 0, 0) );
			sessionid.append( "-" );
			sessionid.append( PQgetvalue(res, 0, 1) );
			flag = true;
		} 
		PQclear(res);
		return flag;
	}

	bool update(const std::string& sessionid, std::string& userlogin, std::string& email, std::string& roles)  noexcept
	{
		bool flag{false};
		int retries {0};
		
		userlogin.clear(); email.clear();
		std::string id, uuid;
		if (auto pos = sessionid.find("-"); pos != std::string::npos) 
		{
			id = sessionid.substr(0, pos);
			uuid = sessionid.substr(pos + 1);
		} else
		{
			logger::log("security", "error", "malformed sessionid passed to session::update(): " + sessionid, true);
			return false;
		}
		
		int nParams = 2;
		const char *const paramValues[] = { id.c_str(), uuid.c_str() };
		const int paramLengths[] = { (int)id.size(), (int)uuid.size() };
		const int paramFormats[] = {0, 0};
		int resultFormat = 0;
	  
	  retry:
		PGresult *res = PQexecPrepared(db.conn, "update_session", nParams, paramValues, paramLengths, paramFormats, resultFormat);
		if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
			PQclear(res);
			if ( PQstatus(db.conn) == CONNECTION_BAD ) {
				if (retries == max_retries) {
					logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " connection retries exhausted, cannot connect to database", true);
					return false;
				} else {
					retries++;
					db.reset_connection();
					goto retry;
				}
			} else {
				logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " " + std::string(PQerrorMessage(db.conn)), true);
				return false;
			}
		}		
		
		if ( PQntuples(res) > 0) 
		{
			userlogin.append( PQgetvalue(res, 0, 0) );
			email.append( PQgetvalue(res, 0, 1) );
			roles.append( PQgetvalue(res, 0, 2) );
			flag = true;		
		}
		PQclear(res);
		return flag;
		
	}

	void remove(const std::string& sessionid) noexcept
	{
		std::string id{""};
		if (auto pos = sessionid.find("-"); pos != std::string::npos) {
			id = sessionid.substr(0, pos);
			const std::string sql{"call cpp_session_delete(" + id + ")"};
			PGresult *res = PQexec(db.conn, sql.c_str());
			if (PQresultStatus(res) != PGRES_COMMAND_OK)
				logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " " + std::string(PQerrorMessage(db.conn)), true);
			PQclear(res);
		}
		else {
			logger::log("security", "error", "malformed sessionid passed to session::remove(): " + sessionid, true);
		}
	}

	int get_total() noexcept
	{
		int total{-1};
		const std::string sql{"select * from cpp_session_count()"};
		PGresult *res = PQexec(db.conn, sql.c_str());
		if (PQresultStatus(res) != PGRES_TUPLES_OK)
			logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " " + std::string(PQerrorMessage(db.conn)), true);
		else if ( PQntuples(res) > 0) 
			total = std::atoi( PQgetvalue(res, 0, 0) );
		PQclear(res);
		return total;
	}

}


