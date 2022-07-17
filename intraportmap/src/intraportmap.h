#ifndef _INTRAPORTMAP_H
#define _INTRAPORTMAP_H

// 配对完成后的转发tunnel，服务器端收到立即自增建立
class ipm_tunnel_server
{
public:
	// 这一头
	// 那一头

	// 状态 （c未通知 c就绪 b未连接 b就绪）

	// 这一头(读写挂)（用两个函数区分两头）
	// 那一头(读写挂)
};

class interface_ipm_agent_server
{
public:
	virtual ~interface_ipm_agent_server() {}
};

// 服务器端的用户fd服务模块，多个，listen来访客立即建立
class ipm_agent_server
{
public:
	// 客户端主连接
	// 服务器代理listen

	// listen结果，通知上层有新客户
	// 客户端结果(读写挂)

	// 不存tunnel tunnel查找需要上层
};

// 服务器端（一个）
class ipm_server
{
public:
	// 服务器listen

	// listen结果（读写挂）

	// 主动跑ipm_agent_server，或tunnel

	// 存一堆ipm_agent_server
	// 存一堆ipm_tunnel，方便查找
};

// 配对完成后的转发tunnel，客户端收到控制信号编号立即建立
class ipm_tunnel_client
{
public:
	// 这一头
	// 那一头

	// 状态 （a未连接 a就绪 b未连接 b未协商 b就绪）

	// 这一头(读写挂)（用两个函数区分两头）
	// 那一头(读写挂)
};

// 客户端（一个）
static void ipm_client_evdns_getaddrinfo_callback(int err, struct evutil_addrinfo* ai, void* arg);
class ipm_client
{
public:
	ipm_client(struct event_base* base);

	bool init(const char* server_name_c, const char* server_port_name_c, const char* to_server_name_c,
		const char* to_server_port_name_c, const char* from_server_name_c, const char* from_server_port_name_c);
	bool is_init();
	bool exit();
	void reset();

public:
	void on_evdns_getaddrinfo(int err, struct evutil_addrinfo* result);				// 尝试发起DNS查询
	bool client_connect_to_server(const struct sockaddr* addr, int addr_length);	// 尝试连接到服务器

private:
	bool dns_query_server();

	// 服务器的IP
	// 连接

	// DNS结果（有挂）
	// 服务器结果（读写挂）（挂了重连不退确保tunnel好的）

	// 主动跑ipm_tunnel，内部即可

	// 存一堆tunnel
private:
	bool is_state_init;
	std::string server_name;
	std::string server_port_name;
	std::string to_server_name;
	std::string to_server_port_name;
	std::string from_server_name;
	std::string from_server_port_name;
	struct event_base* root_event_base;
	struct evdns_base* server_evdns_base;	// 整个程序只查一个dns
};

class intraportmap
{
public:
	enum class CLIENT_STATE : UINT32
	{
		IDLE,
		DNS_QUERYING,
		SERVER_CONNECTING,
		RUNNING,
	};
	enum class SERVER_STATE : UINT32
	{
		IDLE,
		RUNNING,
	};

public:
	intraportmap();

	bool init(int argc, char* argv[]);
	bool is_init();
	bool exit();
	void reset();

	void exec();

public: // libevent过来的事件
	// DNS
	

private:
	bool init_config(int argc, char* argv[]);

private:
	bool is_state_init;
	bool is_server;
	struct event_base* root_event_base;
	std::shared_ptr<ipm_client> sp_ipm_client;
	std::string server_name;
	std::string server_port_name;
	std::string to_server_name;
	std::string to_server_port_name;
	std::string from_server_name;
	std::string from_server_port_name;
};

#endif
