#ifndef _IPM_CLIENT_H
#define _IPM_CLIENT_H

// 客户端（一个）
class interface_ipm_client
{
public:
	virtual ~interface_ipm_client() {}
	virtual void on_interface_ipm_client_fail() = 0;
};

class ipm_client : public interface_ipm_client_tunnel
{
public:
	enum class CLIENT_STATE : UINT32
	{
		IDLE,
		DNS_QUERYING,
		SERVER_CONNECTING,
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
	virtual void on_interface_ipm_tunnel_client_fail(unsigned long long index);

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
	struct sockaddr_storage server_addr;
	unsigned int server_addr_len;
	struct sockaddr_storage to_server_addr;
	unsigned int to_server_addr_len;
	struct sockaddr_storage from_server_addr;
	unsigned int from_server_addr_len;
	// 不释放的变量
	struct event_base* root_event_base;		// 来自外部
	// 整个类的生命周期
	struct event* timer_event;				// 定时器
	struct evdns_base* server_evdns_base;	// 整个程序只查一个dns
	std::map<unsigned long long, std::shared_ptr<ipm_client_tunnel>> mst_tunnel;
	// 重连生命周期(client)
	struct bufferevent* server_bufferevent;	// 连接到服务器
};


#endif