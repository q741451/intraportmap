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

// �����ɺ��ת��tunnel�����������յ�������������
class ipm_tunnel_server
{
public:
	// ��һͷ
	// ��һͷ

	// ״̬ ��cδ֪ͨ c���� bδ���� b������

	// ��һͷ(��д��)������������������ͷ��
	// ��һͷ(��д��)
};

class interface_ipm_agent_server
{
public:
	virtual ~interface_ipm_agent_server() {}
};

// �������˵��û�fd����ģ�飬�����listen���ÿ���������
class ipm_agent_server
{
public:
	// �ͻ���������
	// ����������listen

	// listen�����֪ͨ�ϲ����¿ͻ�
	// �ͻ��˽��(��д��)

	// ����tunnel tunnel������Ҫ�ϲ�
};

// �������ˣ�һ����
class ipm_server
{
public:
	// ������listen

	// listen�������д�ң�

	// ������ipm_agent_server����tunnel

	// ��һ��ipm_agent_server
	// ��һ��ipm_tunnel���������
};

// �����ɺ��ת��tunnel���ͻ����յ������źű����������
class ipm_tunnel_client
{
public:
	// ��һͷ
	// ��һͷ

	// ״̬ ��aδ���� a���� bδ���� bδЭ�� b������

	// ��һͷ(��д��)������������������ͷ��
	// ��һͷ(��д��)
};

// �ͻ��ˣ�һ����
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
	bool dns_query_server();				// ���Է���DNS��ѯ
	bool client_connect_to_server(const struct sockaddr* addr, int addr_length);	// �������ӵ�������
	bool send_alloc(struct bufferevent* bev);	// ���Ϳ��ٶ˿���Ϣ
	bool client_exit();						// ����֮ǰ׼��
	void client_reset();					// ����֮ǰ׼��
	bool set_reconnect_timmer();				// �����ӳ�

	// ��������IP
	// ����

	// DNS������йң�
	// �������������д�ң���������������ȷ��tunnel�õģ�
	// ״̬ ��δ���� δ���� ������ ���ߵȴ���

	// ������ipm_tunnel���ڲ�����

	// ��һ��tunnel
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
	struct sockaddr_storage to_server_addr;
	struct sockaddr_storage from_server_addr;
	// �ò��ͷŵı���
	struct event_base* root_event_base;		// �����ⲿ
	// ���������������
	struct event* timer_event;				// ��ʱ��
	struct evdns_base* server_evdns_base;	// ��������ֻ��һ��dns
	// ������������(client)
	struct bufferevent* server_bufferevent;	// ���ӵ�������
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

public: // libevent�������¼�
	// �ж�
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
