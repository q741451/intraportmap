#ifndef _IPM_SERVER_UDP_H
#define _IPM_SERVER_UDP_H

class interface_ipm_server_udp
{
public:
	virtual ~interface_ipm_server_udp() {}
	virtual void on_interface_ipm_server_udp_fail() = 0;
};

// 服务端 UDP（一个）。所有 agent、所有会话都共用这一组控制 socket 和客户端通信 ——
// 见 ipm_types.h 里的 socket 约束 1，换源端口发过去会被严格 NAT 丢掉
class ipm_server_udp : public interface_ipm_server_udp_agent
{
public:
	ipm_server_udp(struct event_base* base, interface_ipm_server_udp* ptr_interface_p);

	bool init(const char* server_name_c, const char* server_port_name_c, const char* key_c, unsigned int session_timeout_u);
	bool is_init();
	bool exit();
	void reset();

public:
	virtual bool on_interface_ipm_server_udp_agent_new_session(std::shared_ptr<ipm_udp_session>& session);
	virtual void on_interface_ipm_server_udp_agent_del_session(unsigned long long id);
	virtual bool on_interface_ipm_server_udp_agent_to_client(ipm_server_udp_agent* agent, unsigned long long id, const char* payload, size_t payload_len);

public:
	void on_fail();
	void on_readable(evutil_socket_t fd);
	void on_sweep_timer();

private:
	bool handle_register(evutil_socket_t fd, struct sockaddr* from_addr, unsigned int from_len, char* pkt, size_t pkt_len);
	bool handle_data(char* pkt, size_t pkt_len);
	unsigned long long next_session_id();
	void warn_oversize(size_t payload_len);

private:
	interface_ipm_server_udp* ptr_interface;
	bool is_state_init;
	std::string server_name;
	std::string server_port_name;
	std::string key;
	unsigned int session_timeout;
	struct sockaddr_storage server_addr;
	unsigned int server_addr_len;
	struct event_base* root_event_base;		// 来自外部，不释放
	ipm_udp_socket_pair sock;				// 与客户端通信的控制口
	struct event* sweep_event;
	// 随机播种的单调计数器。绝不能用 fd：fd 会被立刻复用，迟到的数据报会串进
	// 复用了同号的另一个会话；随机播种则是防服务端重启后与客户端残留会话撞号
	unsigned long long session_seq;
	time_t last_oversize_log;
	unsigned long long oversize_count;
	std::map<addr_pkg_idx, std::shared_ptr<ipm_server_udp_agent>> asa_agent;	// 映射端口 -> agent
	std::map<unsigned long long, std::shared_ptr<ipm_udp_session>> mis_session;	// 全局会话号索引
};

#endif
