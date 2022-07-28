#ifndef _IPM_TUNNEL_SERVER_H
#define _IPM_TUNNEL_SERVER_H

class interface_ipm_server_tunnel
{
public:
	virtual ~interface_ipm_server_tunnel() {}
	virtual void on_interface_ipm_server_tunnel_fail(evutil_socket_t to_fd) = 0;
};

class ipm_server_tunnel
{
public:
	enum class CONN_STATE : unsigned int
	{
		IDLE,
		RUNNING,
		BROKEN,
	};
	enum class FLUSH_STATE : unsigned int // ��RUNNINGʱҲ��Ч��CONNECTED����
	{
		NORMAL,
		FLUSH_AND_REREAD,
		FLUSH_AND_EXIT,
	};

	ipm_server_tunnel(struct event_base* base, interface_ipm_server_tunnel* ptr_interface_p, evutil_socket_t to_fd_u);

	bool init(struct bufferevent* to_bev, size_t max_buffer_sz);
	bool is_init();
	bool exit();
	void reset();

	bool client_connected(struct bufferevent* bev);

public:
	void on_fail();
	void on_client_fail();
	void on_to_fail();
	void on_client_bufferevent_data_read_callback(struct bufferevent* bev);
	void on_client_bufferevent_data_write_callback(struct bufferevent* bev);
	void on_client_bufferevent_event_callback(struct bufferevent* bev, short what);
	void on_to_bufferevent_data_read_callback(struct bufferevent* bev);
	void on_to_bufferevent_data_write_callback(struct bufferevent* bev);
	void on_to_bufferevent_event_callback(struct bufferevent* bev, short what);

private:
	bool flush_client_data(bool is_event = true);
	bool flush_to_data(bool is_event = true);

private:
	bool is_state_init;
	interface_ipm_server_tunnel* ptr_interface;
	// ���
	evutil_socket_t to_fd;
	// ����
	size_t max_buffer;
	// ���ͷŵı���
	struct event_base* root_event_base;		// �����ⲿ
	// 2����״̬�����״̬
	CONN_STATE client_state;
	CONN_STATE to_state;
	FLUSH_STATE client_flush_state;
	FLUSH_STATE to_flush_state;
	// ���������ӣ��ⲿ���룬�����ͷ�
	struct bufferevent* client_bufferevent;	// �ͻ���
	struct bufferevent* to_bufferevent;	// ���ӵ�����������
};

#endif