#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/signalfd.h>
#include <netinet/tcp.h>
#include <iomanip>
#include <cstring> 
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <stop_token>
#include "mse.h"

int get_signalfd() noexcept;
int get_listenfd(int port) noexcept;
void start_epoll(int port) noexcept ;
void start_server() noexcept;
void consumer(std::stop_token tok) noexcept;
bool read_request(http::request& req, const char* data, int bytes) noexcept;
void http_server(int fd) noexcept;
std::string get_ip_addr(int fd) noexcept;
void print_server_info(std::string pod_name) noexcept;

struct worker_params {
	int epoll_fd;
	int fd;
	http::request& req;
};

std::queue<worker_params> m_queue;
std::condition_variable m_cond;
std::mutex m_mutex;
int m_signal;

inline std::string get_ip_addr(int fd) noexcept
{
	struct sockaddr_in addr;
	socklen_t addrlen {sizeof(addr)};
	getpeername(fd, (struct sockaddr*)&addr, &addrlen);
	char* remote_ip = inet_ntoa(addr.sin_addr);
	return std::string(remote_ip);
}

inline bool read_request(int fd, http::request& req, const char* data, int bytes) noexcept
{
	bool first_packet { (req.payload.size() > 0) ? false : true };
	req.payload.append(data, bytes);
	if (first_packet) {
		req.remote_ip = get_ip_addr(fd);
		req.parse();
		if (req.method == "GET" || req.errcode ==  -1)
			return true;
	}
	if (req.eof())
		return true;
	return false;
}

inline int get_signalfd() noexcept 
{
	signal(SIGPIPE, SIG_IGN);
	sigset_t sigset;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigaddset(&sigset, SIGTERM);
	sigaddset(&sigset, SIGQUIT);
	sigprocmask(SIG_BLOCK, &sigset, NULL);
	int sfd { signalfd(-1, &sigset, 0) };
	return sfd;
}

inline int get_listenfd(int port) noexcept 
{
	int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	fcntl(fd, F_SETFD, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
    struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);
	
	int rc = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
	if (rc == -1) {
		logger::log("epoll", "error", "bind() failed  port: " + std::to_string(port) + " " + std::string(strerror(errno)));
		exit(-1);
	}
	logger::log("epoll", "info", "listen socket FD: " + std::to_string(fd) + " port: " + std::to_string(port));
	return fd;
}

void consumer(std::stop_token tok) noexcept 
{
	//start microservice engine on this thread
	mse::init();
	
	while(!tok.stop_requested())
	{
		//prepare lock
		std::unique_lock lock{m_mutex}; 
		//release lock, reaquire it if conditions met
		m_cond.wait(lock, [&tok] { return (!m_queue.empty() || tok.stop_requested()); }); 
		
		//stop requested?
		if (tok.stop_requested()) { lock.unlock(); break; }
		
		//get task
		auto params = m_queue.front();
		m_queue.pop();
		lock.unlock();
		
		//---processing task (run microservice)
		mse::http_server(params.fd, params.req);
		
		//request ready, set epoll fd for output
		epoll_event event;
		event.data.fd = params.fd;
		event.events = EPOLLOUT | EPOLLET | EPOLLRDHUP;
		epoll_ctl(params.epoll_fd, EPOLL_CTL_MOD, params.fd, &event);
	}
	
	//ending task - free resources
	logger::log("pool", "info", "stopping worker thread", true);
}

void start_epoll(int port) noexcept 
{
	int epoll_fd {epoll_create1(0)};
	logger::log("epoll", "info", "starting epoll FD: " + std::to_string(epoll_fd));

	int listen_fd {get_listenfd(port)};
	listen(listen_fd, SOMAXCONN);
	
	epoll_event event_listen;
	event_listen.data.fd = listen_fd;
	event_listen.events = EPOLLIN;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event_listen);
	  
	epoll_event event_signal;
	event_signal.data.fd = m_signal;
	event_signal.events = EPOLLIN;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, m_signal, &event_signal);

	char data[131072];
	std::map<int, http::request> buffers;
	const int MAXEVENTS = 64;
	epoll_event events[MAXEVENTS];
	bool exit_loop {false};
	while (true)
	{
		int n_events = epoll_wait(epoll_fd, events, MAXEVENTS, -1);
		for (int i = 0; i < n_events; i++)
		{
			if (((events[i].events & EPOLLRDHUP) || (events[i].events & EPOLLHUP) || events[i].events & EPOLLERR))
			{
				int rc = close(events[i].data.fd);
				mse::update_connections(-1);
				if (rc == -1)
					logger::log("epoll", "error", "close FAILED for FD: " + std::to_string(events[i].data.fd) + " " + std::string(strerror(errno)));
			}
			else if (m_signal == events[i].data.fd) //shutdown
			{
				logger::log("signal", "info", "stop signal received for epoll FD: " + std::to_string(epoll_fd) + " SFD: " + std::to_string(m_signal));
				exit_loop = true;
				break;
			}
			else if (listen_fd == events[i].data.fd) // new connection.
			{
				struct sockaddr addr;
				socklen_t len;
				len = sizeof addr;
				int fd { accept4(listen_fd, &addr, &len, SOCK_NONBLOCK) };
				if (fd == -1) {
					logger::log("epoll", "error", "connection accept FAILED for epoll FD: " + std::to_string(epoll_fd) + " " + std::string(strerror(errno)));
					continue;
				}

				int rcvbuf {131072}, sndbuf {262144};
				setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof rcvbuf);
				setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof sndbuf);
				
				if (!buffers.contains(fd))
					 buffers.insert({fd, http::request()});
				else {
					 http::request& req = buffers[fd];
					 if (!req.is_clear) {
						logger::log("epoll", "warn", "invoking request.clear() on FD: " + std::to_string(fd) + " path: " + req.path);
						req.clear();
					 }
				}
								
				mse::update_connections(1);
				epoll_event event;
				event.data.fd = fd;
				event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
				epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
			}
			else // read/write
			{
				int fd {events[i].data.fd};
				http::request& req = buffers[fd];
				if (events[i].events & EPOLLIN) {
					bool run_task {false};
					int count {0};
					while ((count = read(fd, data, sizeof(data)))) {
						if (count == -1 && errno == EAGAIN) {
							break;
						}
						if (count > 0) {
							if (read_request(fd, req, data, count)) {
								run_task = true;
								break;
							}
						}
						else {
							logger::log("epoll", "error", "read error FD: " + std::to_string(fd) + " " + std::string(strerror(errno)));
							buffers[fd].clear();
							break;
						}
					}
					if (run_task) {
						//producer
						worker_params wp {epoll_fd, fd, req};
						{
							std::scoped_lock lock {m_mutex};
							m_queue.push(wp);
							m_cond.notify_all();
						}
					}
				} else if (events[i].events & EPOLLOUT) {
					//send response
					if (req.response.write(fd)) {
						req.clear();
						epoll_event event;
						event.data.fd = fd;
						event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
						epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
					}
				}
			}
		}
		if (exit_loop)
			break;
	}

	close(listen_fd);
	logger::log("epoll", "info", "closing listen socket FD: " + std::to_string(listen_fd));
	close(epoll_fd);
	logger::log("epoll", "info", "closing epoll FD: " + std::to_string(epoll_fd));
}

void print_server_info(std::string pod_name) noexcept 
{
	logger::log("env", "info", "port: " + std::to_string(env::port()));
	logger::log("env", "info", "pool size: " + std::to_string(env::pool_size()));
	logger::log("env", "info", "login log: " + std::to_string(env::login_log_enabled()));
	logger::log("env", "info", "http log: " + std::to_string(env::http_log_enabled()));
	
	std::string msg1; msg1.reserve(255);
	std::string msg2; msg1.reserve(255);
	msg1.append("Pod: " + pod_name).append(" PID: ").append(std::to_string(getpid())).append(" starting ").append(mse::SERVER_VERSION).append("-").append(std::to_string(CPP_BUILD_DATE));
	msg2.append("hardware threads: ").append(std::to_string(std::thread::hardware_concurrency())).append(" GCC: ").append(__VERSION__);
	logger::log("server", "info", msg1);
	logger::log("server", "info", msg2);
}

void start_server() noexcept
{
	const int pool_size {env::pool_size()};
	const int port {env::port()};

	//create workers pool - consumers
	std::vector<std::stop_source> stops(pool_size);
	std::vector<std::jthread> pool(pool_size);
	for (int i = 0; i < pool_size; i++) {
		stops[i] = std::stop_source();
		pool[i] = std::jthread(consumer, stops[i].get_token());
	}
	
	start_epoll(port);
	
	//shutdown workers
	for (auto s: stops) {
		s.request_stop();
		{
			std::scoped_lock lock {m_mutex};
			m_cond.notify_all();
		}
	}
}

int main()
{
	m_signal = get_signalfd();
	logger::log("signal", "info", "signal interceptor registered");
	
	char hostname[100]; gethostname(hostname, sizeof(hostname));
	std::string pod_name(hostname);

	print_server_info(pod_name);
	config::parse();
	start_server();

	logger::log("server", "info", pod_name + " shutting down...");
}
