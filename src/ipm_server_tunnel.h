#ifndef _IPM_TUNNEL_SERVER_H
#define _IPM_TUNNEL_SERVER_H

class interface_ipm_server_tunnel
{
public:
	virtual ~interface_ipm_server_tunnel() {}
	virtual void on_interface_ipm_server_tunnel_fail(unsigned long long index) = 0;
};

// 等 join 的期限。客户端一连上服务端就发 join，正常几毫秒就到
#ifndef IPM_JOIN_TIMEOUT
#define IPM_JOIN_TIMEOUT 60
#endif

// 服务端的一条隧道：访客的连接先进来，客户端为它建的连接 join 后，两边交给 relay 转发。
// 等 join 时不读访客，数据留在内核里，照 Envoy 停读时的做法守候访客关闭
// （EV_CLOSED 察觉 FIN；RST 在 libevent 里报成可写），只有 Linux 的 epoll/poll 支持：
// - 访客关了、内核里没有它的数据：没什么要交付，立即关闭
// - 访客关了、还有数据：照 ss，join 后先交付再关
// - join 一直不来，或平台不支持守候：到期关闭
class ipm_server_tunnel : public interface_ipm_relay
{
public:
	enum SIDE : int
	{
		SIDE_TO = 0,		// 访客
		SIDE_CLIENT = 1,	// 客户端
	};

	ipm_server_tunnel(struct event_base* base, interface_ipm_server_tunnel* ptr_interface_p, unsigned long long index_u);

	// 失败时 to_fd 仍归调用方
	bool init(evutil_socket_t to_fd, size_t max_buffer_sz);
	bool is_init();
	bool exit();
	void reset();

	// false：没接管，bev 仍归调用方
	bool client_connected(struct bufferevent* bev);

public:
	virtual void on_interface_ipm_relay_close();

public:
	void on_fail();
	void on_to_watch();
	void on_join_timeout();

private:
	bool is_state_init;
	interface_ipm_server_tunnel* ptr_interface;
	// 编号
	unsigned long long index;
	// 不释放的变量
	struct event_base* root_event_base;		// 来自外部
	// 整个类的生命周期，join 后停掉
	struct event* to_watch_event;			// 守候访客关闭，平台不支持时为 NULL
	struct event* join_timer;
	// 保留的连接，外部传入，本类释放
	struct bufferevent* client_bufferevent;	// 客户端的 join 连接，fd 已交给 relay
	ipm_relay relay;
};

#endif