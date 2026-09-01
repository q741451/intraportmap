#ifndef _IPM_SERVER_UDP_AGENT_H
#define _IPM_SERVER_UDP_AGENT_H

class ipm_server_udp_agent;

// 一条 UDP 会话：公网侧某个访客地址 <-> 一个全局唯一的会话号。
// 没有连接可言，靠每会话一个空闲定时器回收 —— 这就是我们自己的那套 DNAT 映射表
class ipm_udp_session
{
public:
	ipm_udp_session()
		: id(0), peer_addr_len(0), from_fd(-1), timer(NULL), agent(NULL)
	{
		memset(&peer_addr, 0, sizeof(peer_addr));
	}

	// 在自己的定时器回调里被析构是允许的：event_free 对正在执行的 event
	// 会做延迟释放
	~ipm_udp_session()
	{
		if (timer)
			event_free(timer);
	}

	unsigned long long id;
	addr_pkg_idx peer_idx;					// 访客地址，做 map key 用
	struct sockaddr_storage peer_addr;		// 原样保留，回包直接用
	unsigned int peer_addr_len;
	// 访客是从 agent 的哪个 socket 进来的。双栈下 agent 可能同时有 v4/v6 两个
	// socket，回包必须原路出去
	evutil_socket_t from_fd;
	struct event* timer;					// 空闲老化，每次收发都重置
	ipm_server_udp_agent* agent;
};

class interface_ipm_server_udp_agent
{
public:
	virtual ~interface_ipm_server_udp_agent() {}
	// 访客来了个新地址，请上层分配全局会话号并登记进全局索引。失败表示会话表满
	virtual bool on_interface_ipm_server_udp_agent_new_session(std::shared_ptr<ipm_udp_session>& session) = 0;
	// 会话被回收，上层同步摘掉全局索引
	virtual void on_interface_ipm_server_udp_agent_del_session(unsigned long long id) = 0;
	// 把一个数据包发给该 agent 注册时记下的客户端地址
	virtual bool on_interface_ipm_server_udp_agent_to_client(ipm_server_udp_agent* agent, unsigned long long id, const char* payload, size_t payload_len) = 0;
	// 心跳超时，该 agent 判死，请上层摘掉并销毁
	virtual void on_interface_ipm_server_udp_agent_dead(ipm_server_udp_agent* agent) = 0;
};

// 服务端的一个 UDP 映射端口。双栈时可能持有 v4/v6 两个 socket
class ipm_server_udp_agent
{
public:
	ipm_server_udp_agent(struct event_base* base, interface_ipm_server_udp_agent* ptr_interface_p);

	bool init(addr_pkg_idx& addr_idx_api, unsigned int session_timeout_u);
	bool is_init();
	bool exit();
	void reset();

	addr_pkg_idx& get_addr_pkg_idx();

	// 客户端每次心跳都会刷新这两样：NAT 重绑定后源地址会变，最新的赢
	void set_client(evutil_socket_t fd, const struct sockaddr* addr, unsigned int addr_len);
	evutil_socket_t get_client_fd();
	const struct sockaddr* get_client_addr();
	unsigned int get_client_addr_len();

	// 客户端回来的数据，原路发给访客。会话自带 from_fd 和 peer_addr，
	// 双栈下必须从访客进来的那个 socket 出去
	bool send_to_peer(ipm_udp_session* session, const char* payload, size_t payload_len);

public:
	void on_readable(evutil_socket_t fd);
	void on_session_timeout(ipm_udp_session* session);
	void on_heartbeat_timeout();

private:
	bool is_state_init;
	interface_ipm_server_udp_agent* ptr_interface;
	struct event_base* root_event_base;		// 来自外部，不释放
	addr_pkg_idx addr_idx;
	unsigned int session_timeout;
	// 公网侧监听，双栈时 v4/v6 两个都在
	ipm_udp_socket_pair sock;
	// 客户端当前的 UDP 地址（NAT 映射的外侧），以及它是从哪个控制 socket 来的
	evutil_socket_t client_fd;
	struct sockaddr_storage client_addr;
	unsigned int client_addr_len;
	struct event* hb_timer;					// 心跳缺席判死，每次注册/心跳重置
	// 会话的空闲超时。所有会话共用同一个时长，用 libevent 的 common timeout
	// 装进按时长分桶的有序队列，重置退化成 O(1) 队尾插入，不碰最小堆
	const struct timeval* session_tv;
	// 访客地址 -> 会话。全局的会话号索引在 ipm_server_udp 那边
	std::map<addr_pkg_idx, std::shared_ptr<ipm_udp_session>> mss_session;
};

#endif
