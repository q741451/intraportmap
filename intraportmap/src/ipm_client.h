#ifndef _IPM_CLIENT_H
#define _IPM_CLIENT_H

// �ͻ��ˣ�һ����
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
	bool dns_query_server();				// ���Է���DNS��ѯ
	bool client_connect_to_server(const struct sockaddr* addr, int addr_length);	// �������ӵ�������
	bool send_alloc(struct bufferevent* bev);	// ���Ϳ��ٶ˿���Ϣ
	bool client_exit();						// ����֮ǰ׼��
	void client_reset();					// ����֮ǰ׼��
	bool set_reconnect_timmer();				// �����ӳ�

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
	// ת����ĵ�ַ
	struct sockaddr_storage server_addr;
	unsigned int server_addr_len;
	struct sockaddr_storage to_server_addr;
	unsigned int to_server_addr_len;
	struct sockaddr_storage from_server_addr;
	unsigned int from_server_addr_len;
	// ���ͷŵı���
	struct event_base* root_event_base;		// �����ⲿ
	// ���������������
	struct event* timer_event;				// ��ʱ��
	struct evdns_base* server_evdns_base;	// ��������ֻ��һ��dns
	std::map<unsigned long long, std::shared_ptr<ipm_client_tunnel>> mst_tunnel;
	// ������������(client)
	struct bufferevent* server_bufferevent;	// ���ӵ�������
};


#endif