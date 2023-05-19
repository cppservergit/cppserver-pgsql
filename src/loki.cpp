#include "loki.h"

namespace loki 
{
	std::condition_variable m_cond;
	std::mutex m_mutex;  
	std::queue<std::string> m_queue;
	std::jthread m_worker;
	std::stop_source m_stop_src;
	
	void consumer(std::stop_token tok) noexcept;
	
	void start_task() noexcept
	{
		m_stop_src = std::stop_source();
		m_worker = std::jthread(consumer, m_stop_src.get_token());
	}
	
	void stop_task() noexcept
	{
		m_stop_src.request_stop();
		{
			std::scoped_lock lock {loki::m_mutex};
			loki::m_cond.notify_one();
		}
		m_worker.join();
	}
	
	void push_log_record(std::string msg) noexcept
	{
		std::scoped_lock lock {m_mutex};
		m_queue.push(msg);
		m_cond.notify_one();
	}

	inline void replace(std::string &str, const std::string& from, const std::string& to) {
		if (from.empty() || to.empty())
			return;
		size_t start_pos = 0;
		while((start_pos = str.find(from, start_pos)) != std::string::npos) {
			str.replace(start_pos, from.length(), to);
			start_pos += to.length();
		}
	}

	void post(int fd, std::string& msg) noexcept
	{
		replace(msg, "\"", "\\\"");
		long int ts {std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count()};
		std::string json; json.reserve(4095);
		json.append("{\"streams\": [{\"stream\": {\"app\": \"cppserver\"},\"values\": [");
	
		json.append("[ \"" + std::to_string(ts)  + "\",\"" + msg + "\"]");
		json.append("]}]}");
		
		std::stringstream payload;
		payload << "POST /loki/api/v1/push HTTP/1.1" << "\r\n"
				<< "Host: demodb.mshome.net:3100" << "\r\n"
				<< "User-Agent: cppserver/1.01" << "\r\n"
				<< "Accept: */*" << "\r\n"
				<< "Content-Type: application/json" << "\r\n"
				<< "Content-Length: " << json.size() << "\r\n"
				<< "\r\n"
				<< json;

		int bytes = write(fd, payload.str().c_str(), payload.str().size());
		if ((size_t)bytes != payload.str().size())
			fprintf(stderr, "loki write() failed\n");
		else {
			char resp[2048];
			int bytes_read = read(fd, resp, sizeof(resp));
			if (bytes_read) {
				std::string httpresp(resp, bytes_read);
				if (auto newpos = httpresp.find("HTTP/1.1 204", 0); newpos == std::string::npos) 
					fprintf(stderr, "error sending push to loki: %s\n", httpresp.c_str());
			}
		}
		close(fd);
	}

	int open_connection(hostent *hp, int port) noexcept
	{
		struct sockaddr_in addr;
		int on = 1, sock;     

		bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
		addr.sin_port = htons(port);
		addr.sin_family = AF_INET;
		sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));

		if(sock == -1){
			perror("Loki setsockopt()");
			return 0;
		}
		
		if(connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1){
			perror("Loki connect()");
			return 0;
		}
		return sock;
	}

	void consumer(std::stop_token tok) noexcept 
	{
		//do name resolution once, it's expensive
		struct hostent *hp;
		if ((hp = gethostbyname(env::loki_server().c_str())) == NULL){
			herror("Loki gethostbyname()");
			return;
		}
		
		while(true)
		{
			//prepare lock
			std::unique_lock lock{m_mutex}; 
			//release lock, reaquire it if conditions met
			m_cond.wait(lock, [&tok] { return (!m_queue.empty() || tok.stop_requested()); }); 
			
			//stop requested?
			if (tok.stop_requested() && m_queue.empty()) 
			{
				lock.unlock(); 
				break; 
			}
			
			//get task
			auto msg = m_queue.front();
			m_queue.pop();
			lock.unlock();
			
			//execute task (send log to loki)
			int fd {open_connection(hp, env::loki_port())};
			if (fd)
				post(fd, msg);
		}
	}
	
}