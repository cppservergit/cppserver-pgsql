#include "sql.h"

namespace sql 
{
	const std::string LOGGER_SRC {"sql"};

	struct dbutil 
	{
		
		PGconn* conn;
		std::string m_dbconnstr;
		
		dbutil() 
		{
		}
		
		dbutil(const std::string& conn_info): m_dbconnstr{conn_info}
		{
			conn = PQconnectdb(m_dbconnstr.c_str());
			if (PQstatus(conn) != CONNECTION_OK)
				logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " " + std::string(PQerrorMessage(conn)), true);
		}
		
		dbutil(dbutil &&source) : m_dbconnstr{source.m_dbconnstr}, conn{source.conn}
		{
			source.conn = nullptr;
		}

		dbutil(const dbutil &source) : m_dbconnstr{source.m_dbconnstr}, conn{source.conn}
		{
		}
		
		~dbutil() {
			if (conn) 
				PQfinish(conn);
		}
		
		inline void reset_connection() noexcept
		{
			if ( PQstatus(conn) == CONNECTION_BAD ) {
				logger::log(LOGGER_SRC, "warn", std::string(__PRETTY_FUNCTION__) + " connection to database " + std::string(PQdb(conn)) + " no longer valid, reconnecting... ", true);
				PQfinish(conn);
				conn = PQconnectdb(m_dbconnstr.c_str());
				if (PQstatus(conn) != CONNECTION_OK)
					logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " error reconnecting to database " + std::string(PQdb(conn)) + " - " + std::string(PQerrorMessage(conn)), true);
				else
					logger::log(LOGGER_SRC, "info", std::string(__PRETTY_FUNCTION__) + " connection to database " +  std::string(PQdb(conn)) + " restored", true);
			}
		}
	
	};

	thread_local  std::unordered_map<std::string, dbutil> dbconns;
	constexpr int max_retries{3};
	constexpr char DBLIB_ERROR[] = "{\"status\": \"ERROR\",\"description\" : \"database error, the details were stored in the stderr log\"}";
	constexpr int PG_DATE = 1082;
	constexpr int PG_TIMESTAMP = 1114;
	constexpr int PG_VARCHAR = 1043;
	constexpr int PG_TEXT = 25;
	
	inline void get_json_array(std::string& json, PGresult *res) noexcept 
	{
		json.append("[");
		int rows {PQntuples(res)};
		int cols {PQnfields(res)};
		for(int i=0; i<rows; i++) {
			json.append("{");
			for(int j=0; j<cols; j++) {
				json.append("\"").append(PQfname(res, j)).append("\":");
				Oid coltype = PQftype(res, j);
				switch (coltype) 
				{
					case PG_VARCHAR:
					case PG_TEXT:
					case PG_DATE:
					case PG_TIMESTAMP:
						json.append("\"").append(PQgetvalue(res, i, j)).append("\",");
						break;
					default:
						json.append(PQgetvalue(res, i, j)).append(",");
						break;
				}
			}
			json.pop_back();
			json.append("},");
		}
		if (rows > 0) 
			json.pop_back();
		json.append("]");
	}


	void connect(const std::string& dbname, const std::string& conn_info)
	{
		if (!dbconns.contains(dbname)) { 
			dbconns.insert({dbname, dbutil(conn_info)});
		}
		else {
			std::string error{std::string(__PRETTY_FUNCTION__) + " duplicated dbname: " + dbname};
			throw std::runtime_error(error.c_str());
		}
	}


	PGconn* getdb(const std::string& dbname)
	{
		if (!dbconns.contains(dbname)) {
			std::string error{std::string(__PRETTY_FUNCTION__) + " invalid dbname: " + dbname};
			throw std::runtime_error(error.c_str());
		}
		return dbconns[dbname].conn;
	}


	void reset(const std::string& dbname) noexcept
	{
		dbconns[dbname].reset_connection();
	}

	
	void get_json(const std::string& dbname, std::string &json, const std::string &sql, bool useDataPrefix, const std::string &prefixName)
	{
		int retries {0};
		
	retry:
		PGconn *conn = getdb(dbname);
		PGresult *res = PQexec(conn, sql.c_str());
		if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
			PQclear(res);
			if ( PQstatus(conn) == CONNECTION_BAD ) {
				if (retries == max_retries) {
					logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " connection retries exhausted, cannot connect to database", true);
					json.append(DBLIB_ERROR);
					return;
				} else {
					retries++;
					reset(dbname);
					goto retry;
				}
			} else {
				logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " " + std::string(PQerrorMessage(conn)), true);
				json.append(DBLIB_ERROR);
				return;
			}
		}

		if (useDataPrefix) {
			json.append( "{\"status\":\"OK\"," );
			json.append("\"");
			json.append(prefixName);
			json.append("\":");
		}
		get_json_array( json, res );
		if (useDataPrefix)
			json.append("}");
		PQclear(res);
	}
	
	void get_json(const std::string& dbname, std::string &json, const std::string &sql, const std::vector<std::string> &varNames, const std::string &prefixName) 
	{
		int retries {0};
		std::vector<std::string> qry;
		size_t newpos{0};
		while (true) {
			if (auto pos = sql.find(";", newpos); pos != std::string::npos) {
				qry.push_back(sql.substr(newpos, pos));
				newpos = pos + 1;
			} else {
				if (newpos < sql.size())
					qry.push_back(sql.substr(newpos));
				break;
			}
		}
		int i{0};
		json.append( "{\"status\":\"OK\"," );
		json.append("\"");
		json.append(prefixName);
		json.append("\":{");
		for (auto& q: qry) {
		  retry:
			PGconn *conn = getdb(dbname);
			PGresult *res = PQexec(conn, q.c_str());
			if (PQresultStatus(res) != PGRES_TUPLES_OK)	{
				PQclear(res);
				if ( PQstatus(conn) == CONNECTION_BAD ) {
					if (retries == max_retries) {
						logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " connection retries exhausted, cannot connect to database", true);
						json.append(DBLIB_ERROR);
						return;
					} else {
						retries++;
						reset(dbname);
						goto retry;
					}
				} else {
					logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " " + std::string(PQerrorMessage(conn)), true);
					json.clear(); json.append(DBLIB_ERROR);
					return;
				}
			}
			json.append( "\"");
			json.append(varNames[i]);
			json.append("\":");
			get_json_array( json, res );
			json.append(",");
			PQclear(res);
			i++;
		}
		json.pop_back();
		json.append("}}");
	}	
	
	bool exec_sql(const std::string& dbname, const std::string& sql)
	{
		int retries {0};
		constexpr int max_retries{3};
	retry:
		PGconn *conn = getdb(dbname);
		PGresult *res = PQexec(conn, sql.c_str());
		if (PQresultStatus(res) != PGRES_COMMAND_OK) {
			PQclear(res);
			if ( PQstatus(conn) == CONNECTION_BAD ) {
				if (retries == max_retries) {
					logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " connection retries exhausted, cannot connect to database", true);
					return false;
				} else {
					retries++;
					reset(dbname);
					goto retry;
				}
			} else {
				logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " " + std::string(PQerrorMessage(conn)), true);
				return false;
			}
		}
		PQclear(res);
		return true;
	}
	
	//returns true if the query retuned 1+ row
	bool has_rows(const std::string& dbname, const std::string &sql)
	{
		int retries {0};
		constexpr int max_retries{3};
	
	retry:
		PGconn *conn = getdb(dbname);
		PGresult *res = PQexec(conn, sql.c_str());
		
		if (PQresultStatus(res) != PGRES_TUPLES_OK) {
			PQclear(res);
			if ( PQstatus(conn) == CONNECTION_BAD ) {
				if (retries == max_retries) {
					logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " connection retries exhausted, cannot connect to database", true);
					throw std::runtime_error("database connection error");
				} else {
					retries++;
					reset(dbname);
					goto retry;
				}
			} else {
				logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " " + std::string(PQerrorMessage(conn)), true);
				throw std::runtime_error("database query execution error");
			}
		}
		
		bool result {true};
		if (PQntuples(res) == 0)
			result = false;
		PQclear(res);
		return result;
	}	
	
	//returns only the first rows of a resultset, use of "limit 1" or "where col=pk" in the query is recommended
	std::unordered_map<std::string, std::string> get_record(const std::string& dbname, const std::string& sql)
	{
		int retries {0};
		constexpr int max_retries{3};
		std::unordered_map<std::string, std::string> rec;
		rec.reserve(5);
		
	retry:
		PGconn *conn = getdb(dbname);	
		PGresult *res = PQexec(conn, sql.c_str());
		
		if (PQresultStatus(res) != PGRES_TUPLES_OK) {
			PQclear(res);
			if ( PQstatus(conn) == CONNECTION_BAD ) {
				if (retries == max_retries) {
					logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " connection retries exhausted, cannot connect to database", true);
					throw std::runtime_error("SQL database error");
				} else {
					retries++;
					reset(dbname);
					goto retry;
				}
			} else {
				logger::log(LOGGER_SRC, "error", std::string(__PRETTY_FUNCTION__) + " " + std::string(PQerrorMessage(conn)), true);
				throw std::runtime_error("SQL database error");
			}
		}		
		
		int rows {PQntuples(res)};
		int cols {PQnfields(res)};
		if (rows) {
			for(int j=0; j<cols; j++) {
				rec.emplace(PQfname(res, j), PQgetvalue(res, 0, j));
			}
		} else {
			logger::log(LOGGER_SRC, "warn", "get_record() resultset is empty: " + sql, true);
		}
		PQclear(res);
		return rec;
	}
	
}
