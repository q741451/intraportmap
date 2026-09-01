#include "stdafx.h"

void ipm_server_udp_readable_callback(evutil_socket_t fd, short events, void* user_data);
void ipm_server_udp_sweep_callback(evutil_socket_t fd, short events, void* user_data);

ipm_server_udp::ipm_server_udp(struct event_base* base, interface_ipm_server_udp* ptr_interface_p)
	: ptr_interface(ptr_interface_p), root_event_base(base)
{
	reset();
}

bool ipm_server_udp::init(const char* server_name_c, const char* server_port_name_c, const char* key_c, unsigned int session_timeout_u)
{
	bool ret = false;
	struct timeval tv;

	server_name = server_name_c;
	server_port_name = server_port_name_c;
	key = key_c;
	session_timeout = session_timeout_u;

	// 会话号随机播种，理由见头文件
	evutil_secure_rng_get_bytes(&session_seq, sizeof(session_seq));

	if (util::getaddrinfo_first(server_name.c_str(), server_port_name.c_str(), server_addr, &server_addr_len) != true)
	{
		slog_error("getaddrinfo_first server_name error");
		goto end;
	}

	slog_info("ipm_server_udp start at [%s]:%s", util::get_ipname_from_sockaddr((struct sockaddr*)&server_addr).c_str(), util::get_portstr_from_sockaddr((struct sockaddr*)&server_addr).c_str());

	if (sock.open(root_event_base, (struct sockaddr*)&server_addr, (int)server_addr_len, ipm_server_udp_readable_callback, this) != true)
	{
		slog_error("udp control socket open error");
		goto end;
	}

	// 一个全局清扫定时器就够，不必每会话一个 timer
	if ((sweep_event = event_new(root_event_base, -1, EV_PERSIST, ipm_server_udp_sweep_callback, this)) == NULL)
	{
		slog_error("event_new error");
		goto end;
	}

	memset(&tv, 0, sizeof(tv));
	tv.tv_sec = IPM_UDP_SWEEP_INTERVAL;
	if (event_add(sweep_event, &tv) != 0)
	{
		slog_error("event_add error");
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

bool ipm_server_udp::is_init()
{
	return is_state_init;
}

bool ipm_server_udp::exit()
{
	std::map<addr_pkg_idx, std::shared_ptr<ipm_server_udp_agent>>::iterator iter;

	if (sweep_event)
	{
		event_free(sweep_event);
		sweep_event = NULL;
	}

	sock.close();

	for (iter = asa_agent.begin(); iter != asa_agent.end(); ++iter)
	{
		if (iter->second->is_init())
			iter->second->exit();
	}
	asa_agent.clear();
	mis_session.clear();

	is_state_init = false;
	return true;
}

void ipm_server_udp::reset()
{
	is_state_init = false;
	server_addr_len = 0;
	session_timeout = IPM_UDP_SESSION_TIMEOUT;
	session_seq = 0;
	last_oversize_log = 0;
	oversize_count = 0;
	sweep_event = NULL;
	sock.close();
	asa_agent.clear();
	mis_session.clear();
}

void ipm_server_udp::on_fail()
{
	slog_error("server_udp on_fail");
	if (ptr_interface)
		ptr_interface->on_interface_ipm_server_udp_fail();
}

unsigned long long ipm_server_udp::next_session_id()
{
	// 0 是控制包的判别值，不能当会话号发出去
	if (++session_seq == 0)
		++session_seq;

	return session_seq;
}

bool ipm_server_udp::on_interface_ipm_server_udp_agent_new_session(std::shared_ptr<ipm_udp_session>& session)
{
	if (mis_session.size() >= IPM_UDP_MAX_SESSION)
		return false;

	session->id = next_session_id();
	mis_session[session->id] = session;

	return true;
}

void ipm_server_udp::on_interface_ipm_server_udp_agent_del_session(unsigned long long id)
{
	mis_session.erase(id);
}

// 访客的载荷已经躺在 util::get_udp_buffer() + IPM_UDP_SESSION_LEN 处，
// 这里正好在它前面写会话号、后面补校验，一次 sendto 发走，全程不再拷贝
bool ipm_server_udp::on_interface_ipm_server_udp_agent_to_client(ipm_server_udp_agent* agent, unsigned long long id, const char* payload, size_t payload_len)
{
	char* pkt = (char*)payload - IPM_UDP_SESSION_LEN;
	size_t pkt_len = IPM_UDP_SESSION_LEN + payload_len + IPM_UDP_CHECKSUM_LEN;

	if (agent == NULL || agent->get_client_addr_len() == 0 || agent->get_client_fd() == -1)
		return false;

	warn_oversize(payload_len);

	*(unsigned long long*)pkt = util::htonllx(id);
	util::set_checksum_fast(key.c_str(), pkt, pkt_len);

	if (sendto(agent->get_client_fd(), pkt, (int)pkt_len, 0, agent->get_client_addr(), agent->get_client_addr_len()) < 0)
		return false;

	return true;
}

// 控制口来包：前 8 字节为 0 是注册/心跳，非 0 是会话数据
void ipm_server_udp::on_readable(evutil_socket_t fd)
{
	char* buf = util::get_udp_buffer();
	struct sockaddr_storage from_addr;
	ev_socklen_t from_len = sizeof(from_addr);
	unsigned long long index = 0;
	int recv_len = 0;

	memset(&from_addr, 0, sizeof(from_addr));

	recv_len = recvfrom(fd, buf, IPM_UDP_BUF_LEN, 0, (struct sockaddr*)&from_addr, &from_len);
	if (recv_len < (int)IPM_UDP_SESSION_LEN)
		return;

	index = util::ntohllx(*(unsigned long long*)buf);

	if (index == 0)
		handle_register(fd, (struct sockaddr*)&from_addr, from_len, buf, (size_t)recv_len);
	else
		handle_data(buf, (size_t)recv_len);
}

bool ipm_server_udp::handle_register(evutil_socket_t fd, struct sockaddr* from_addr, unsigned int from_len, char* pkt, size_t pkt_len)
{
	alloc_agent_package_t* ag_agent = (alloc_agent_package_t*)pkt;
	std::map<addr_pkg_idx, std::shared_ptr<ipm_server_udp_agent>>::iterator iter;
	std::shared_ptr<ipm_server_udp_agent> sag;
	addr_pkg_idx addr_idx;

	if (pkt_len != sizeof(alloc_agent_package_t))
		return false;

	if (util::check_checksum(key.c_str(), pkt, pkt_len) != true)
		return false;

	addr_idx.addr_pkg = ag_agent->addr_pkg;

	if ((iter = asa_agent.find(addr_idx)) != asa_agent.end())
	{
		sag = iter->second;
	}
	else
	{
		slog_info("udp alloc_agent port = %u", ntohl(addr_idx.addr_pkg.port));

		sag = std::make_shared<ipm_server_udp_agent>(root_event_base, this);
		if (sag->init(addr_idx, session_timeout) != true)
		{
			slog_error("udp agent init error");
			return false;
		}

		asa_agent[sag->get_addr_pkg_idx()] = sag;
	}

	// 客户端 NAT 重绑定后源地址会变，每次心跳都刷新，最新的赢
	sag->set_client(fd, from_addr, from_len);

	// 原样回显作为 ACK。客户端靠它判活、触发 DNS 重解析，
	// 同时这一来一回也把 Linux conntrack 的 UDP 映射从 30s 提升到 180s
	if (sendto(fd, pkt, (int)pkt_len, 0, from_addr, from_len) < 0)
		return false;

	return true;
}

bool ipm_server_udp::handle_data(char* pkt, size_t pkt_len)
{
	std::map<unsigned long long, std::shared_ptr<ipm_udp_session>>::iterator iter;
	unsigned long long index = 0;

	if (pkt_len < IPM_UDP_OVERHEAD)
		return false;

	if (util::check_checksum_fast(key.c_str(), pkt, pkt_len) != true)
		return false;

	index = util::ntohllx(*(unsigned long long*)pkt);

	// 会话可能已经老化掉了，或者是迟到的陈包，丢掉即可
	if ((iter = mis_session.find(index)) == mis_session.end())
		return false;

	if (iter->second->agent == NULL)
		return false;

	return iter->second->agent->send_to_peer(iter->second.get(), pkt + IPM_UDP_SESSION_LEN, pkt_len - IPM_UDP_OVERHEAD);
}

// 超长只告警不丢弃（与 ss 一致），但日志必须限流，否则高 pps 下会被日志打死
void ipm_server_udp::warn_oversize(size_t payload_len)
{
	time_t now;

	if (payload_len <= IPM_UDP_WARN_PAYLOAD)
		return;

	oversize_count++;
	now = time(NULL);

	if (now - last_oversize_log < 60)
		return;

	last_oversize_log = now;
	slog_warn("udp payload %llu > %d, relying on IP fragmentation (%llu so far)", (unsigned long long)payload_len, IPM_UDP_WARN_PAYLOAD, oversize_count);
}

void ipm_server_udp::on_sweep_timer()
{
	std::map<addr_pkg_idx, std::shared_ptr<ipm_server_udp_agent>>::iterator iter = asa_agent.begin();
	time_t now = time(NULL);

	while (iter != asa_agent.end())
	{
		// 心跳缺席就判定 agent 死亡。这一层不需要独立超时机制，心跳本身就够
		if (now - iter->second->get_last_heartbeat() >= (time_t)IPM_UDP_AGENT_TIMEOUT)
		{
			slog_info("udp agent port %u heartbeat lost, closing", ntohl(iter->second->get_addr_pkg_idx().addr_pkg.port));
			iter->second->exit();		// 会顺带摘掉它名下所有会话的全局索引
			asa_agent.erase(iter++);
		}
		else
		{
			// 会话级的空闲老化：心跳保的是 agent 那条 NAT 映射，管不到会话
			iter->second->sweep(now);
			++iter;
		}
	}
}

void ipm_server_udp_readable_callback(evutil_socket_t fd, short events, void* user_data)
{
	if (user_data)	((ipm_server_udp*)user_data)->on_readable(fd);
}

void ipm_server_udp_sweep_callback(evutil_socket_t fd, short events, void* user_data)
{
	if (user_data)	((ipm_server_udp*)user_data)->on_sweep_timer();
}
