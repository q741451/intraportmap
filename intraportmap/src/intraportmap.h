#ifndef _INTRAPORTMAP_H
#define _INTRAPORTMAP_H

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
	void on_evdns_getaddrinfo(int err, struct evutil_addrinfo* result);				// ���Է���DNS��ѯ
	bool client_connect_to_server(const struct sockaddr* addr, int addr_length);	// �������ӵ�������

private:
	bool dns_query_server();

	// ��������IP
	// ����

	// DNS������йң�
	// �������������д�ң���������������ȷ��tunnel�õģ�

	// ������ipm_tunnel���ڲ�����

	// ��һ��tunnel
private:
	bool is_state_init;
	std::string server_name;
	std::string server_port_name;
	std::string to_server_name;
	std::string to_server_port_name;
	std::string from_server_name;
	std::string from_server_port_name;
	struct event_base* root_event_base;
	struct evdns_base* server_evdns_base;	// ��������ֻ��һ��dns
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

public: // libevent�������¼�
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
