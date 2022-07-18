#ifndef _INTRAPORTMAP_H
#define _INTRAPORTMAP_H

#ifdef WIN32
#pragma pack(1)
#endif
typedef struct _alloc_agent_package
{
	unsigned int alloc_zero;
	unsigned int is_ipv6;
	char ip[16];
	unsigned int port;
	unsigned int checksum;
}
#ifndef WIN32
__attribute__((packed))
#endif
alloc_agent_package_t;
#ifdef WIN32
#pragma pack()
#endif

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
class interface_ipm_client
{
public:
	virtual ~interface_ipm_client() {}
	virtual void on_interface_ipm_client_fail() = 0;
};
void ipm_client_evdns_getaddrinfo_callback(int err, struct evutil_addrinfo* ai, void* arg);
void ipm_client_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx);
void ipm_client_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx);
void ipm_client_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx);
void ipm_client_timer_event_callback(evutil_socket_t sig, short events, void* user_data);
class ipm_client
{
public:
	enum class CLIENT_STATE : UINT32
	{
		IDLE,
		DNS_QUERYING,
		SERVER_CONNECTING,
		SENDING_ALLOC,
		RUNNING,
		WAITING,
	};

public:
	ipm_client(struct event_base* base, interface_ipm_client* ptr_interface_p);

	bool init(const char* server_name_c, const char* server_port_name_c, const char* to_server_name_c,
		const char* to_server_port_name_c, const char* from_server_name_c, const char* from_server_port_name_c, unsigned int client_reconn_time_u);
	bool is_init();
	bool exit();
	void reset();

public:
	void on_fail();
	void on_fatal_fail();
	void on_evdns_getaddrinfo(int err, struct evutil_addrinfo* result);
	void on_bufferevent_data_read(struct bufferevent* bev);
	void on_bufferevent_data_write(struct bufferevent* bev);
	void on_bufferevent_event(struct bufferevent* bev, short flag);
	void on_timer_event(evutil_socket_t sig, short events);

private:
	bool dns_query_server();				// 尝试发起DNS查询
	bool client_connect_to_server(const struct sockaddr* addr, int addr_length);	// 尝试连接到服务器
	bool send_alloc(struct bufferevent* bev);	// 发送开辟端口信息
	bool client_exit();						// 重连之前准备
	void client_reset();					// 重连之前准备
	bool set_reconnect_timmer();				// 重连延迟

	// 服务器的IP
	// 连接

	// DNS结果（有挂）
	// 服务器结果（读写挂）（挂了重连不退确保tunnel好的）
	// 状态 （未解析 未连接 已连接 掉线等待）

	// 主动跑ipm_tunnel，内部即可

	// 存一堆tunnel
private:
	interface_ipm_client* ptr_interface;
	bool is_state_init;
	unsigned int client_reconn_time;
	CLIENT_STATE client_state;
	std::string server_name;
	std::string server_port_name;
	std::string to_server_name;
	std::string to_server_port_name;
	std::string from_server_name;
	std::string from_server_port_name;
	// 转换后的地址
	struct sockaddr_storage to_server_addr;
	struct sockaddr_storage from_server_addr;
	// 用不释放的变量
	struct event_base* root_event_base;		// 来自外部
	// 整个类的生命周期
	struct event* timer_event;				// 定时器
	struct evdns_base* server_evdns_base;	// 整个程序只查一个dns
	// 重连生命周期(client)
	struct bufferevent* server_bufferevent;	// 连接到服务器
};

void intraportmap_signal_event_callback(evutil_socket_t sig, short events, void* user_data);

class intraportmap : public interface_ipm_client
{
public:
	intraportmap();

	bool init(int argc, char* argv[]);
	bool is_init();
	bool exit();
	void reset();

	void exec();

public:
	virtual void on_interface_ipm_client_fail();

public: // libevent过来的事件
	// 中断
	void on_signal_event(evutil_socket_t sig, short events);

private:
	bool init_config(int argc, char* argv[]);
	bool register_signal_event();

private:
	bool is_state_init;
	bool is_server;
	struct event* signal_event;
	struct event_base* root_event_base;
	std::shared_ptr<ipm_client> sp_ipm_client;
	unsigned int client_reconn_time;
	std::string server_name;
	std::string server_port_name;
	std::string to_server_name;
	std::string to_server_port_name;
	std::string from_server_name;
	std::string from_server_port_name;
};

#endif
