#ifndef _IPM_CLIENT_UDP_H
#define _IPM_CLIENT_UDP_H

class ipm_client_udp;

// 客户端侧的一条 UDP 会话：会话号 <-> 一个连到被代理主机的 socket。
// 每会话必须独占一个 socket —— 被代理主机总是从同一个地址回包，共享 socket
// 就没法把回包分辨回是哪条会话。这也是我们自己那套 NAT 表的客户端半边
class ipm_client_udp_session
{
public:
	ipm_client_udp_session()
		: id(0), fd(-1), read_event(NULL), timer(NULL), owner(NULL)
	{
	}

	// 在自己的定时器回调里被析构是允许的：event_free 对正在执行的 event
	// 会做延迟释放
	~ipm_client_udp_session()
	{
		if (read_event)
			event_free(read_event);
		if (timer)
			event_free(timer);
		if (fd != -1)
			evutil_closesocket(fd);
	}

	unsigned long long id;
	evutil_socket_t fd;				// 到被代理主机
	struct event* read_event;
	struct event* timer;			// 空闲老化，每次收发都重置
	ipm_client_udp* owner;
};

class interface_ipm_client_udp
{
public:
	virtual ~interface_ipm_client_udp() {}
	virtual void on_interface_ipm_client_udp_fail() = 0;
};

// 客户端 UDP（一个）。与 TCP 侧完全独立：不借道控制连接，自己解析 DNS、
// 自己重试。心跳和数据必须共用 server_fd 这一个 socket —— 见 ipm_types.h 的
// socket 约束 2，会话正是靠蹭心跳维持的那条 NAT 映射才免于各自保活
class ipm_client_udp
{
public:
	// 与 ipm_client::CLIENT_STATE 一一对应。UDP 没有 connect() 失败这种内核信号，
	// 所以用 REGISTERING 超时来充当 TCP 的 SERVER_CONNECTING 失败
	enum class CLIENT_STATE : unsigned int
	{
		IDLE,
		DNS_QUERYING,
		REGISTERING,
		RUNNING,
		WAITING,
	};

public:
	ipm_client_udp(struct event_base* base, interface_ipm_client_udp* ptr_interface_p);

	bool init(const char* server_name_c, const char* server_port_name_c, const char* to_server_name_c,
		const char* to_server_port_name_c, const char* from_server_name_c, const char* from_server_port_name_c,
		unsigned int client_reconn_time_u, const char* key_c, unsigned int session_timeout_u);
	bool is_init();
	bool exit();
	void reset();

public:
	void on_fail();
	void on_fatal_fail();
	void on_evdns_getaddrinfo(int err, struct evutil_addrinfo* result);
	void on_server_readable(evutil_socket_t fd);
	void on_session_readable(evutil_socket_t fd, ipm_client_udp_session* session);
	void on_session_timeout(ipm_client_udp_session* session);
	void on_tick();

private:
	bool dns_query_server();
	bool open_server_socket();
	bool send_register();
	std::shared_ptr<ipm_client_udp_session> new_session(unsigned long long id);
	void client_exit();
	void client_reset();

private:
	interface_ipm_client_udp* ptr_interface;
	bool is_state_init;
	CLIENT_STATE client_state;
	unsigned int client_reconn_time;
	unsigned int session_timeout;
	std::string server_name;
	std::string server_port_name;
	std::string to_server_name;
	std::string to_server_port_name;
	std::string from_server_name;
	std::string from_server_port_name;
	std::string key;
	// 要求服务端开的公网端口，以及本地被代理主机
	struct sockaddr_storage server_addr;
	unsigned int server_addr_len;
	struct sockaddr_storage to_server_addr;
	unsigned int to_server_addr_len;
	struct sockaddr_storage from_server_addr;
	unsigned int from_server_addr_len;
	// 不释放的变量
	struct event_base* root_event_base;		// 来自外部
	// 整个类的生命周期
	struct event* tick_event;				// 1 秒一跳，驱动注册重试与心跳
	// 会话空闲超时。所有会话共用同一时长，用 libevent 的 common timeout 装进
	// 按时长分桶的有序队列，每包重置退化成 O(1) 队尾插入，不碰最小堆
	const struct timeval* session_tv;
	// 重连生命周期
	struct evdns_base* server_evdns_base;
	evutil_socket_t server_fd;				// 心跳和数据共用，不可拆
	struct event* server_read_event;
	time_t last_recv;						// 最后一次收到服务端任何包
	time_t last_send;
	time_t wait_since;
	unsigned int reg_tries;
	unsigned int hb_miss;
	bool hb_pending;						// 心跳已发出，正在等 ACK
	std::map<unsigned long long, std::shared_ptr<ipm_client_udp_session>> mis_session;
};

#endif
