#ifndef _IPM_SERVER_H
#define _IPM_SERVER_H

// 服务器端（一个）

class interface_ipm_server
{
public:
	virtual ~interface_ipm_server() {}
	virtual void on_interface_ipm_server_fail() = 0;
};

class ipm_server : public interface_ipm_server_agent, public interface_ipm_server_tunnel
{
public:
	enum class SERVER_STATE : unsigned int
	{
		IDLE,
		STARTING,
		RUNNING,
	};

public:
	ipm_server(struct event_base* base, interface_ipm_server* ptr_interface_p);

	bool init(const char* server_name_c, const char* server_port_name_c, const char* key_c, size_t max_buffer_sz);
	bool is_init();
	bool exit();
	void reset();

public:
	virtual void on_interface_ipm_server_agent_fail(bufferevent* bev);
	virtual unsigned long long on_interface_ipm_server_agent_next_tunnel_index();
	virtual bool on_interface_ipm_server_agent_new_tunnel(bufferevent* bev, bufferevent* to_bev, unsigned long long index);
	virtual void on_interface_ipm_server_tunnel_fail(unsigned long long index);

public:
	void on_fail();
	void on_listener(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr* sa, int socklen);
	void on_bufferevent_data_read(struct bufferevent* bev);
	void on_bufferevent_data_write(struct bufferevent* bev);
	void on_bufferevent_event(struct bufferevent* bev, short flag);

private:
	bool start_listener(const struct sockaddr* addr, int addr_length);		// 开启服务器
	bool remove_bufferevent(bufferevent* buf_event);
	bool close_and_remove_bufferevent(bufferevent* buf_event);
	bool alloc_agent(struct bufferevent* bev, alloc_agent_package_t* pkg, bool& is_fatal);
	bool join_tunnel_client(struct bufferevent* bev, unsigned long long index);

private:
	interface_ipm_server* ptr_interface;
	bool is_state_init;
	SERVER_STATE server_state;
	std::string server_name;
	std::string server_port_name;
	std::string key;
	size_t max_buffer;
	// 转换后的地址
	struct sockaddr_storage server_addr;
	unsigned int server_addr_len;
	// 不释放的变量
	struct event_base* root_event_base;		// 来自外部
	// 整个类的生命周期
	struct evconnlistener* listener;
	struct evconnlistener* listener6;
	std::set<bufferevent*>	sbe_bufferevent;		// 第一步，缓存的，可以分配到tunnel或agent
	std::map<bufferevent*, std::shared_ptr<ipm_server_agent>> msa_agent;		// 连接+代理listen
	std::map<addr_pkg_idx, std::shared_ptr<ipm_server_agent>> asa_agent;		// 连接+代理listen
	// 隧道编号，随机播种的单调计数器。不能用访客的 fd：隧道冲刷时 fd 已经关掉，
	// 下一个访客立刻拿到同一个号，会把还在冲刷的隧道顶掉，迟到的 join 也会接进别人的隧道
	unsigned long long tunnel_seq;
	std::map<unsigned long long, std::shared_ptr<ipm_server_tunnel>> mst_tunnel;		// 隧道编号
};

#endif