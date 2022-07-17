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

// �������˵��û�fd����ģ�飬�����listen���ÿ���������
class ipm_agent_server
{
public:
	// �ͻ���������
	// ����������listen

	// listen���
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
class ipm_client
{
public:
	// ��������IP
	// ����

	// DNS������йң�
	// �������������д�ң���������������ȷ��tunnel�õģ�

	// ������ipm_tunnel���ڲ�����

	// ��һ��tunnel
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
	bool exit();
	bool is_init();
	void reset();

	void exec();

public: // libevent�������¼�
	// DNS
	void on_evdns_getaddrinfo(int err, struct evutil_addrinfo* result);

private:
	bool init_config(int argc, char* argv[]);
	bool dns_query_server();
	bool client_connect_to_server(const struct sockaddr* addr, int addr_length);

private:
	bool is_state_init;
	bool is_server;
	struct event_base* event_base;
	struct evdns_base* evdns_base;	// ��������ֻ��һ��dns
	std::string server_name;
	std::string server_port_name;
	std::string to_server_name;
	std::string to_server_port_name;
	std::string from_server_name;
	std::string from_server_port_name;
};

#endif
