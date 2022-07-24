#ifndef _IPM_TUNNEL_CLIENT_H
#define _IPM_TUNNEL_CLIENT_H

// 配对完成后的转发tunnel，客户端收到控制信号编号立即建立
// 客户端（一个）
class interface_ipm_client_tunnel
{
public:
	virtual ~interface_ipm_client_tunnel() {}
	virtual void on_interface_ipm_tunnel_client_fail(unsigned long long index) = 0;
};

class ipm_client_tunnel
{
public:
	enum class CONN_STATE : unsigned int
	{
		IDLE,
		CONNECTING,
		RUNNING,
		BROKEN,
	};
	enum class FLUSH_STATE : unsigned int // 非RUNNING时也有效，CONNECTED后跑
	{
		NORMAL,
		FLUSH_AND_REREAD,
		FLUSH_AND_EXIT,
	};

	ipm_client_tunnel(struct event_base* base, interface_ipm_client_tunnel* ptr_interface_p, unsigned long long index_u);

	bool init(struct sockaddr_storage& server_addr_ss, unsigned int server_addr_len_u, struct sockaddr_storage& from_server_addr_ss, unsigned int from_server_addr_len_u, size_t max_buffer_sz);
	bool is_init();
	bool exit();
	void reset();

public:
	void on_fail();
	void on_server_fail();
	void on_from_fail();
	void on_server_bufferevent_data_read_callback(struct bufferevent* bev);
	void on_server_bufferevent_data_write_callback(struct bufferevent* bev);
	void on_server_bufferevent_event_callback(struct bufferevent* bev, short what);
	void on_from_bufferevent_data_read_callback(struct bufferevent* bev);
	void on_from_bufferevent_data_write_callback(struct bufferevent* bev);
	void on_from_bufferevent_event_callback(struct bufferevent* bev, short what);

private:
	bool connect_to_server();		// 尝试连接到服务器
	bool connect_to_from();			// 尝试连接到被代理主机
	bool send_penetrate(struct bufferevent* bev);	// 发送开辟端口信息

private:
	bool is_state_init;
	interface_ipm_client_tunnel* ptr_interface;
	// 不释放的变量
	struct event_base* root_event_base;		// 来自外部
	// 预置的地址
	unsigned long long index;
	struct sockaddr_storage server_addr;
	unsigned int server_addr_len;
	struct sockaddr_storage from_server_addr;
	unsigned int from_server_addr_len;
	size_t max_buffer;
	// 2连接状态，清空状态
	CONN_STATE from_state;
	CONN_STATE server_state;
	FLUSH_STATE from_flush_state;
	FLUSH_STATE server_flush_state;
	// 转发的buffer
	struct bufferevent* server_bufferevent;	// 连接到服务器
	struct bufferevent* from_bufferevent;	// 连接到被代理主机
};

#endif