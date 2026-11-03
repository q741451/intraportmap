#ifndef _IPM_RELAY_H
#define _IPM_RELAY_H

// 隧道两端连接之间的TCP转发，照 shadowsocks-libev：
// 每边一块固定缓冲区，从这一边读到的数据马上发给另一边；发不完就停读这一边，
// 等另一边可写时发完再读。转发时不分配内存，一条隧道只占两块缓冲区。
// 任一边读到 FIN 或出错，关闭整条隧道；EAGAIN、EINTR 等下次事件再试。
//
// 两边都接上才 start：停读的一边靠另一边的写事件推进，另一边必须在。
class interface_ipm_relay
{
public:
	virtual ~interface_ipm_relay() {}
	virtual void on_interface_ipm_relay_close() = 0;
};

class ipm_relay
{
public:
	struct side_t
	{
		ipm_relay* relay;
		int id;
		evutil_socket_t fd;			// -1：还没接上
		struct event* read_event;
		struct event* write_event;
		bool is_reading;
		bool is_writing;
		// 从这一边读到、待发往另一边的数据：buf[pos, len)
		char* buf;
		size_t size;
		size_t len;
		size_t pos;
	};

	ipm_relay(struct event_base* base, interface_ipm_relay* ptr_interface_p, unsigned long long index_u);

	bool init(size_t buffer_size_sz);
	bool is_init();
	bool exit();
	void reset();

	// 两边各接一次，只接管资源，不登记事件。
	// 成功后 fd 归本类，exit 时关闭；失败时仍归调用方
	bool attach(int id, evutil_socket_t fd);
	// 接管 bev 的 fd 和输入缓冲里已读到的数据；bev 本身仍归调用方，fd 已摘掉
	bool attach(int id, struct bufferevent* bev);
	bool is_attached(int id);
	// 两边都接上后开始转发。失败时由调用方关闭隧道
	bool start();

public:
	void on_read(int id);
	void on_write(int id);

private:
	bool attach_side(int id, evutil_socket_t fd, struct evbuffer* input);
	bool set_events(int id);
	void update();
	void send_to(int id);
	void on_close();

private:
	bool is_state_init;
	interface_ipm_relay* ptr_interface;
	unsigned long long index;			// 隧道编号，用于日志
	size_t buffer_size;
	// 不释放的变量
	struct event_base* root_event_base;		// 来自外部
	side_t sides[2];
};

#endif
