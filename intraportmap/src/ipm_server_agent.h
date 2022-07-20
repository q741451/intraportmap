#ifndef _IPM_AGENT_SERVER_H
#define _IPM_AGENT_SERVER_H


class interface_ipm_server_agent
{
public:
	virtual ~interface_ipm_server_agent() {}
	virtual void on_interface_ipm_server_agent_fail(bufferevent* bev) = 0;
	virtual bool on_interface_ipm_server_agent_new_fd_tunnel(bufferevent* bev, bufferevent* to_bev, evutil_socket_t to_fd) = 0;
};

// 服务器端的用户fd服务模块，多个，listen来访客立即建立
class ipm_server_agent
{
public:
	enum class SERVER_STATE : UINT32
	{
		IDLE,
		STARTING,
		RUNNING,
	};

public:
	ipm_server_agent(struct event_base* base, interface_ipm_server_agent* ptr_interface_p);
	bool init(struct sockaddr_storage& agent_addr_ss, unsigned int agent_addr_len_u, struct bufferevent* client_bev);
	bool is_init();
	bool exit();
	void reset();

public:
	void on_fail();
	void on_client_bufferevent_data_read_callback(struct bufferevent* bev);
	void on_client_bufferevent_data_write_callback(struct bufferevent* bev);
	void on_client_bufferevent_event_callback(struct bufferevent* bev, short what);
	void on_listener(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr* sa, int socklen);

private:
	bool start_listener(const struct sockaddr* addr, int addr_length);		// 开启服务器
	bool send_penetrate(struct bufferevent* bev, unsigned long long index);	// 发送开辟端口信息

private:
	bool is_state_init;
	interface_ipm_server_agent* ptr_interface;
	// 传入的地址
	struct sockaddr_storage agent_addr;
	unsigned int agent_addr_len;
	// 不释放的变量
	struct event_base* root_event_base;		// 来自外部
	// 整个类的生命周期
	struct evconnlistener* listener;
	struct evconnlistener* listener6;
	// 保留的连接，外部传入，本类释放
	struct bufferevent* client_bufferevent;	// 客户端
};

#endif