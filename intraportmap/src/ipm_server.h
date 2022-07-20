#ifndef _IPM_SERVER_H
#define _IPM_SERVER_H

// �������ˣ�һ����

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

	bool init(const char* server_name_c, const char* server_port_name_c);
	bool is_init();
	bool exit();
	void reset();

public:
	virtual void on_interface_ipm_server_agent_fail(bufferevent* bev);
	virtual bool on_interface_ipm_server_agent_new_fd_tunnel(bufferevent* bev, bufferevent* to_bev, evutil_socket_t to_fd);
	virtual void on_interface_ipm_server_tunnel_fail(evutil_socket_t to_fd);

public:
	void on_fail();
	void on_listener(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr* sa, int socklen);
	void on_bufferevent_data_read(struct bufferevent* bev);
	void on_bufferevent_data_write(struct bufferevent* bev);
	void on_bufferevent_event(struct bufferevent* bev, short flag);

private:
	bool start_listener(const struct sockaddr* addr, int addr_length);		// ����������
	bool remove_bufferevent(bufferevent* buf_event);
	bool close_and_remove_bufferevent(bufferevent* buf_event);
	bool alloc_agent(struct bufferevent* bev, alloc_agent_package_t* pkg);
	bool join_tunnel_client(struct bufferevent* bev, unsigned long long index);

private:
	interface_ipm_server* ptr_interface;
	bool is_state_init;
	SERVER_STATE server_state;
	std::string server_name;
	std::string server_port_name;
	// ת����ĵ�ַ
	struct sockaddr_storage server_addr;
	unsigned int server_addr_len;
	// ���ͷŵı���
	struct event_base* root_event_base;		// �����ⲿ
	// ���������������
	struct evconnlistener* listener;
	struct evconnlistener* listener6;
	std::set<bufferevent*>	sbe_bufferevent;		// ��һ��������ģ����Է��䵽tunnel��agent
	std::map<bufferevent*, std::shared_ptr<ipm_server_agent>> msa_agent;		// ����+����listen
	std::map<evutil_socket_t, std::shared_ptr<ipm_server_tunnel>> mst_tunnel;		// evutil_socket_t to_fd
};

#endif