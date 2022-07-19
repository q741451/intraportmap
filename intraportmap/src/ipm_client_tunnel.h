#ifndef _IPM_TUNNEL_CLIENT_H
#define _IPM_TUNNEL_CLIENT_H

// �����ɺ��ת��tunnel���ͻ����յ������źű����������
// �ͻ��ˣ�һ����
class interface_ipm_client_tunnel
{
public:
	virtual ~interface_ipm_client_tunnel() {}
	virtual void on_interface_ipm_tunnel_client_fail(unsigned int index) = 0;
};

class ipm_client_tunnel
{
public:
	enum class FROM_STATE : UINT32
	{
		IDLE,
		CONNECTING,
		RUNNING,
	};
	enum class SERVER_STATE : UINT32
	{
		IDLE,
		CONNECTING,
		RUNNING,
	};

	ipm_client_tunnel(struct event_base* base, interface_ipm_client_tunnel* ptr_interface_p, unsigned int index_u);

	bool init(struct sockaddr_storage& server_addr_ss, unsigned int server_addr_len_u, struct sockaddr_storage& from_server_addr_ss, unsigned int from_server_addr_len_u);
	bool is_init();
	bool exit();
	void reset();

public:
	void on_fail();
	void on_server_bufferevent_data_read_callback(struct bufferevent* bev);
	void on_server_bufferevent_data_write_callback(struct bufferevent* bev);
	void on_server_bufferevent_event_callback(struct bufferevent* bev, short what);
	void on_from_bufferevent_data_read_callback(struct bufferevent* bev);
	void on_from_bufferevent_data_write_callback(struct bufferevent* bev);
	void on_from_bufferevent_event_callback(struct bufferevent* bev, short what);

private:
	bool connect_to_server();		// �������ӵ�������
	bool connect_to_from();			// �������ӵ�����������
	bool send_penetrate(struct bufferevent* bev);	// ���Ϳ��ٶ˿���Ϣ
	bool flush_server_data();
	bool flush_from_data();

private:
	bool is_state_init;
	interface_ipm_client_tunnel* ptr_interface;
	// ���ͷŵı���
	struct event_base* root_event_base;		// �����ⲿ
	// Ԥ�õĵ�ַ
	unsigned int index;
	struct sockaddr_storage server_addr;
	unsigned int server_addr_len;
	struct sockaddr_storage from_server_addr;
	unsigned int from_server_addr_len;
	// 2����״̬
	FROM_STATE from_state;
	SERVER_STATE server_state;
	// ת����buffer
	struct bufferevent* server_bufferevent;	// ���ӵ�������
	struct bufferevent* from_bufferevent;	// ���ӵ�����������
};

#endif