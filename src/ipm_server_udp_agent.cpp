#include "stdafx.h"

void ipm_server_udp_agent_readable_callback(evutil_socket_t fd, short events, void* user_data);
void ipm_server_udp_agent_session_timeout_callback(evutil_socket_t fd, short events, void* user_data);
void ipm_server_udp_agent_heartbeat_timeout_callback(evutil_socket_t fd, short events, void* user_data);

ipm_server_udp_agent::ipm_server_udp_agent(struct event_base* base, interface_ipm_server_udp_agent* ptr_interface_p)
	: ptr_interface(ptr_interface_p), root_event_base(base)
{
	reset();
}

bool ipm_server_udp_agent::init(addr_pkg_idx& addr_idx_api, unsigned int session_timeout_u)
{
	bool ret = false;
	struct sockaddr_storage agent_addr;
	unsigned int agent_addr_len = 0;
	struct timeval tv;

	addr_idx = addr_idx_api;
	session_timeout = session_timeout_u;

	// 所有会话共用同一个超时时长，注册成 common timeout 后每次重置是 O(1)
	// 队尾插入，而不是每包一次 O(log N) 的最小堆调整
	memset(&tv, 0, sizeof(tv));
	tv.tv_sec = session_timeout;
	session_tv = event_base_init_common_timeout(root_event_base, &tv);

	if (addr_idx.addr_pkg.is_ipv6)
	{
		struct sockaddr_in6* addr_in6 = (struct sockaddr_in6*)&agent_addr;
		memset(addr_in6, 0, sizeof(struct sockaddr_in6));
		memcpy(&addr_in6->sin6_addr, addr_idx.addr_pkg.ip, sizeof(addr_in6->sin6_addr));
		addr_in6->sin6_family = AF_INET6;
		addr_in6->sin6_port = htons((unsigned short)ntohl(addr_idx.addr_pkg.port));
		agent_addr_len = sizeof(struct sockaddr_in6);
	}
	else
	{
		struct sockaddr_in* addr_in = (struct sockaddr_in*)&agent_addr;
		memset(addr_in, 0, sizeof(struct sockaddr_in));
		memcpy(&addr_in->sin_addr, addr_idx.addr_pkg.ip, sizeof(addr_in->sin_addr));
		addr_in->sin_family = AF_INET;
		addr_in->sin_port = htons((unsigned short)ntohl(addr_idx.addr_pkg.port));
		agent_addr_len = sizeof(struct sockaddr_in);
	}

	slog_info("ipm_server_udp_agent start at [%s]:%s", util::get_ipname_from_sockaddr((struct sockaddr*)&agent_addr).c_str(), util::get_portstr_from_sockaddr((struct sockaddr*)&agent_addr).c_str());

	if (sock.open(root_event_base, (struct sockaddr*)&agent_addr, (int)agent_addr_len, ipm_server_udp_agent_readable_callback, this) != true)
	{
		slog_error("udp socket open error");
		goto end;
	}

	if ((hb_timer = evtimer_new(root_event_base, ipm_server_udp_agent_heartbeat_timeout_callback, this)) == NULL)
	{
		slog_error("evtimer_new error");
		goto end;
	}

	ret = true;
end:
	if (ret == true)
		is_state_init = true;
	else
	{
		exit();
		reset();
	}
	return ret;
}

bool ipm_server_udp_agent::is_init()
{
	return is_state_init;
}

bool ipm_server_udp_agent::exit()
{
	std::map<addr_pkg_idx, std::shared_ptr<ipm_udp_session>>::iterator iter;

	sock.close();

	if (hb_timer)
	{
		event_free(hb_timer);
		hb_timer = NULL;
	}

	// 会话自己不持有 fd，摘掉上层的全局索引后随 map 一起释放，
	// 各自的定时器在 ipm_udp_session 的析构里 event_free
	if (ptr_interface)
	{
		for (iter = mss_session.begin(); iter != mss_session.end(); ++iter)
			ptr_interface->on_interface_ipm_server_udp_agent_del_session(iter->second->id);
	}
	mss_session.clear();

	is_state_init = false;
	return true;
}

void ipm_server_udp_agent::reset()
{
	is_state_init = false;
	session_timeout = IPM_UDP_SESSION_TIMEOUT;
	session_tv = NULL;
	sock.close();
	// reset() 只赋初值，释放一律归 exit()：它会被构造函数调用，那时成员尚未
	// 初始化，在这里 event_free 等于解引用野指针。各类的分工都是如此
	hb_timer = NULL;
	client_fd = -1;
	client_addr_len = 0;
	memset(&client_addr, 0, sizeof(client_addr));
	mss_session.clear();
}

addr_pkg_idx& ipm_server_udp_agent::get_addr_pkg_idx()
{
	return addr_idx;
}

void ipm_server_udp_agent::set_client(evutil_socket_t fd, const struct sockaddr* addr, unsigned int addr_len)
{
	struct timeval tv;

	// 每次心跳都刷新：客户端 NAT 重绑定后源端口会变，最新的赢。
	// 这个「最新的赢」也正是注册包重放能劫持流量的原因，两者是同一件事，
	// 将来加重放保护时要一起改
	client_fd = fd;
	memset(&client_addr, 0, sizeof(client_addr));
	memcpy(&client_addr, addr, addr_len);
	client_addr_len = addr_len;

	// 重新压上判死定时器。event_add 对已挂起的 event 就是重置
	if (hb_timer)
	{
		memset(&tv, 0, sizeof(tv));
		tv.tv_sec = IPM_UDP_AGENT_TIMEOUT;
		event_add(hb_timer, &tv);
	}
}

evutil_socket_t ipm_server_udp_agent::get_client_fd()
{
	return client_fd;
}

const struct sockaddr* ipm_server_udp_agent::get_client_addr()
{
	return (const struct sockaddr*)&client_addr;
}

unsigned int ipm_server_udp_agent::get_client_addr_len()
{
	return client_addr_len;
}

// 访客来包：查/建会话，然后原样交给客户端
void ipm_server_udp_agent::on_readable(evutil_socket_t fd)
{
	char* buf = util::get_udp_buffer();
	char* payload = buf + IPM_UDP_SESSION_LEN;
	struct sockaddr_storage from_addr;
	ev_socklen_t from_len = sizeof(from_addr);
	std::map<addr_pkg_idx, std::shared_ptr<ipm_udp_session>>::iterator iter;
	std::shared_ptr<ipm_udp_session> session;
	addr_pkg_idx peer_idx;
	unsigned int port = 0;
	unsigned int is_ipv6 = 0;
	int recv_len = 0;

	memset(&from_addr, 0, sizeof(from_addr));

	// 一次只收一个包，收完就回事件循环，避免一个高速会话饿死其他 socket
	recv_len = recvfrom(fd, payload, IPM_UDP_MAX_PAYLOAD, 0, (struct sockaddr*)&from_addr, &from_len);
	if (recv_len < 0)
		return;

	// 客户端还没注册上来（或映射刚断），没地方转，直接丢
	if (client_addr_len == 0)
		return;

	if (util::sockaddr_to_address((struct sockaddr*)&from_addr, peer_idx.addr_pkg.ip, &port, &is_ipv6) != true)
		return;
	peer_idx.addr_pkg.port = port;
	peer_idx.addr_pkg.is_ipv6 = is_ipv6;

	if ((iter = mss_session.find(peer_idx)) != mss_session.end())
	{
		session = iter->second;
	}
	else
	{
		if (mss_session.size() >= IPM_UDP_MAX_SESSION)
		{
			// 上限保护：不抢占，直接丢，让老会话自己老化掉
			return;
		}

		session = std::make_shared<ipm_udp_session>();
		session->peer_idx = peer_idx;
		memcpy(&session->peer_addr, &from_addr, from_len);
		session->peer_addr_len = from_len;
		// 双栈下 agent 可能有 v4/v6 两个 socket，回包必须原路出去
		session->from_fd = fd;
		session->agent = this;

		if ((session->timer = evtimer_new(root_event_base, ipm_server_udp_agent_session_timeout_callback, session.get())) == NULL)
			return;

		if (!ptr_interface || ptr_interface->on_interface_ipm_server_udp_agent_new_session(session) != true)
			return;

		mss_session[peer_idx] = session;
	}

	event_add(session->timer, session_tv);

	if (ptr_interface)
		ptr_interface->on_interface_ipm_server_udp_agent_to_client(this, session->id, payload, (size_t)recv_len);
}

bool ipm_server_udp_agent::send_to_peer(ipm_udp_session* session, const char* payload, size_t payload_len)
{
	if (session == NULL || session->from_fd == -1)
		return false;

	event_add(session->timer, session_tv);

	if (sendto(session->from_fd, payload, (int)payload_len, 0, (const struct sockaddr*)&session->peer_addr, session->peer_addr_len) < 0)
		return false;

	return true;
}

void ipm_server_udp_agent::on_session_timeout(ipm_udp_session* session)
{
	std::map<addr_pkg_idx, std::shared_ptr<ipm_udp_session>>::iterator iter;
	std::shared_ptr<ipm_udp_session> keep;

	if ((iter = mss_session.find(session->peer_idx)) == mss_session.end())
		return;

	// 先攥住一份，保证 erase 之后对象活到回调返回（它的定时器正在执行）
	keep = iter->second;

	if (ptr_interface)
		ptr_interface->on_interface_ipm_server_udp_agent_del_session(keep->id);

	mss_session.erase(iter);
}

void ipm_server_udp_agent::on_heartbeat_timeout()
{
	slog_info("udp agent port %u heartbeat lost, closing", ntohl(addr_idx.addr_pkg.port));

	if (ptr_interface)
		ptr_interface->on_interface_ipm_server_udp_agent_dead(this);
}

void ipm_server_udp_agent_readable_callback(evutil_socket_t fd, short events, void* user_data)
{
	if (user_data)	((ipm_server_udp_agent*)user_data)->on_readable(fd);
}

void ipm_server_udp_agent_session_timeout_callback(evutil_socket_t fd, short events, void* user_data)
{
	ipm_udp_session* session = (ipm_udp_session*)user_data;

	if (session && session->agent)
		session->agent->on_session_timeout(session);
}

void ipm_server_udp_agent_heartbeat_timeout_callback(evutil_socket_t fd, short events, void* user_data)
{
	if (user_data)	((ipm_server_udp_agent*)user_data)->on_heartbeat_timeout();
}
