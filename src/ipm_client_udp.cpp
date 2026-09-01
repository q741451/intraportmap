#include "stdafx.h"

void ipm_client_udp_evdns_getaddrinfo_callback(int err, struct evutil_addrinfo* ai, void* arg);
void ipm_client_udp_server_readable_callback(evutil_socket_t fd, short events, void* user_data);
void ipm_client_udp_session_readable_callback(evutil_socket_t fd, short events, void* user_data);
void ipm_client_udp_session_timeout_callback(evutil_socket_t fd, short events, void* user_data);
void ipm_client_udp_tick_callback(evutil_socket_t fd, short events, void* user_data);

ipm_client_udp::ipm_client_udp(struct event_base* base, interface_ipm_client_udp* ptr_interface_p)
	: ptr_interface(ptr_interface_p), root_event_base(base)
{
	reset();
}

bool ipm_client_udp::init(const char* server_name_c, const char* server_port_name_c, const char* to_server_name_c, const char* to_server_port_name_c, const char* from_server_name_c, const char* from_server_port_name_c, unsigned int client_reconn_time_u, const char* key_c, unsigned int session_timeout_u)
{
	bool ret = false;
	struct timeval tv;

	server_name = server_name_c;
	server_port_name = server_port_name_c;
	to_server_name = to_server_name_c;
	to_server_port_name = to_server_port_name_c;
	from_server_name = from_server_name_c;
	from_server_port_name = from_server_port_name_c;
	client_reconn_time = client_reconn_time_u;
	key = key_c;
	session_timeout = session_timeout_u;

	// 客户端侧刻意比服务端晚老化，理由见 IPM_UDP_CLIENT_LAG
	memset(&tv, 0, sizeof(tv));
	tv.tv_sec = session_timeout + IPM_UDP_CLIENT_LAG;
	session_tv = event_base_init_common_timeout(root_event_base, &tv);

	if (util::getaddrinfo_first(to_server_name.c_str(), to_server_port_name.c_str(), to_server_addr, &to_server_addr_len) != true)
	{
		slog_error("getaddrinfo_first to_server_name error");
		goto end;
	}

	if (util::getaddrinfo_first(from_server_name.c_str(), from_server_port_name.c_str(), from_server_addr, &from_server_addr_len) != true)
	{
		slog_error("getaddrinfo_first from_server_name error");
		goto end;
	}

	// 1 秒一跳，驱动注册重试、惰性心跳、重连等待。会话老化不在这里，
	// 每条会话自带定时器
	if ((tick_event = event_new(root_event_base, -1, EV_PERSIST, ipm_client_udp_tick_callback, this)) == NULL)
	{
		slog_error("event_new error");
		goto end;
	}

	memset(&tv, 0, sizeof(tv));
	tv.tv_sec = 1;
	if (event_add(tick_event, &tv) != 0)
	{
		slog_error("event_add error");
		goto end;
	}

	ret = true;

	// 状态必须在调用之前置好：地址是字面量时 evdns_getaddrinfo 会同步回调，
	// 回调里已经把状态推进到 REGISTERING 了，放在后面赋值会把它冲回 DNS_QUERYING，
	// 于是 on_tick 认不出状态，心跳不会跑
	client_state = CLIENT_STATE::DNS_QUERYING;

	// 开始第一步，允许失败
	if (dns_query_server() != true)
	{
		slog_error("dns_query_server error");
		on_fail();
		goto end;
	}
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

bool ipm_client_udp::is_init()
{
	return is_state_init;
}

bool ipm_client_udp::exit()
{
	client_exit();

	if (tick_event)
	{
		event_free(tick_event);
		tick_event = NULL;
	}

	is_state_init = false;
	return true;
}

void ipm_client_udp::reset()
{
	client_state = CLIENT_STATE::IDLE;
	is_state_init = false;
	client_reconn_time = 15;
	session_timeout = IPM_UDP_SESSION_TIMEOUT;
	server_addr_len = 0;
	to_server_addr_len = 0;
	from_server_addr_len = 0;
	tick_event = NULL;
	session_tv = NULL;
	client_reset();
}

// 重连之前的清理：会话和到服务端的 socket 都随之作废
void ipm_client_udp::client_exit()
{
	// 会话的 fd 与两个 event 都由 ipm_client_udp_session 的析构负责
	mis_session.clear();

	if (server_read_event)
	{
		event_free(server_read_event);
		server_read_event = NULL;
	}

	if (server_fd != -1)
	{
		evutil_closesocket(server_fd);
		server_fd = -1;
	}

	if (server_evdns_base)
	{
		evdns_base_free(server_evdns_base, 0);
		server_evdns_base = NULL;
	}
}

void ipm_client_udp::client_reset()
{
	server_evdns_base = NULL;
	server_fd = -1;
	server_read_event = NULL;
	last_recv = 0;
	last_send = 0;
	wait_since = 0;
	reg_tries = 0;
	hb_miss = 0;
	hb_pending = false;
	mis_session.clear();
}

void ipm_client_udp::on_fail()
{
	slog_info("client_udp on_fail");

	client_exit();
	client_reset();

	wait_since = time(NULL);
	client_state = CLIENT_STATE::WAITING;
}

void ipm_client_udp::on_fatal_fail()
{
	slog_error("client_udp on_fatal_fail");
	if (ptr_interface)
		ptr_interface->on_interface_ipm_client_udp_fail();
}

bool ipm_client_udp::dns_query_server()
{
	bool ret = false;
	evdns_getaddrinfo_request* request = NULL;
	struct evutil_addrinfo hints;

	memset(&hints, 0, sizeof(struct evutil_addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_socktype = SOCK_DGRAM;

	slog_info("udp resolving %s:%s...", server_name.c_str(), server_port_name.c_str());

	if ((server_evdns_base = evdns_base_new(root_event_base, 1)) == NULL)
	{
		slog_error("evdns_base_new error");
		goto end;
	}

	if ((request = evdns_getaddrinfo(server_evdns_base, server_name.c_str(), server_port_name.c_str(), &hints, ipm_client_udp_evdns_getaddrinfo_callback, this)) != NULL)
	{
		if (evdns_base_count_nameservers(server_evdns_base) == 0)
		{
			slog_error("evdns_base_count_nameservers error, cancel request");
			evdns_getaddrinfo_cancel(request);
		}
	}

	ret = true;
end:
	return ret;
}

void ipm_client_udp::on_evdns_getaddrinfo(int err, struct evutil_addrinfo* result)
{
	struct evutil_addrinfo* rp = result;
	bool ret = false;

	server_addr_len = 0;

	for (; rp != NULL; rp = rp->ai_next)
	{
		slog_info("udp server [%s]:%s", util::get_ipname_from_sockaddr(rp->ai_addr).c_str(), util::get_portstr_from_sockaddr(rp->ai_addr).c_str());
		memcpy(&server_addr, rp->ai_addr, rp->ai_addrlen);
		server_addr_len = (unsigned int)rp->ai_addrlen;
		// 只取第一个
		break;
	}

	if (server_addr_len == 0)
	{
		slog_error("no address found");
		goto end;
	}

	if (open_server_socket() != true)
	{
		slog_error("open_server_socket error");
		goto end;
	}

	if (send_register() != true)
	{
		slog_error("send_register error");
		goto end;
	}

	reg_tries = 0;
	client_state = CLIENT_STATE::REGISTERING;
	ret = true;
end:
	if (result)
		evutil_freeaddrinfo(result);
	if (ret != true)
		on_fail();
}

bool ipm_client_udp::open_server_socket()
{
	bool ret = false;

	if ((server_fd = socket(server_addr.ss_family, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		slog_error("udp socket error");
		goto end;
	}

	if (evutil_make_socket_nonblocking(server_fd) != 0)
	{
		slog_error("evutil_make_socket_nonblocking error");
		goto end;
	}

	// connect 上去：内核只把服务端来的包投递进来，顺带能收到 ICMP 端口不可达
	if (connect(server_fd, (struct sockaddr*)&server_addr, server_addr_len) != 0)
	{
		slog_error("udp connect error");
		goto end;
	}

	if ((server_read_event = event_new(root_event_base, server_fd, EV_READ | EV_PERSIST, ipm_client_udp_server_readable_callback, this)) == NULL)
	{
		slog_error("event_new error");
		goto end;
	}

	if (event_add(server_read_event, NULL) != 0)
	{
		slog_error("event_add error");
		goto end;
	}

	ret = true;
end:
	if (ret != true)
	{
		if (server_read_event)
		{
			event_free(server_read_event);
			server_read_event = NULL;
		}
		if (server_fd != -1)
		{
			evutil_closesocket(server_fd);
			server_fd = -1;
		}
	}
	return ret;
}

// 注册和心跳是同一个包：幂等地告诉服务端「请在这个端口上开 UDP」，
// 同时刷新它记下的客户端地址，并把 NAT 映射续上
bool ipm_client_udp::send_register()
{
	alloc_agent_package_t pkg;
	unsigned int port = 0;
	unsigned int is_ipv6 = 0;

	if (server_fd == -1)
		return false;

	memset(&pkg, 0, sizeof(pkg));
	pkg.alloc_zero = util::htonllx(0);

	if (util::sockaddr_to_address((struct sockaddr*)&to_server_addr, pkg.addr_pkg.ip, &port, &is_ipv6) != true)
	{
		slog_error("sockaddr_to_address error");
		return false;
	}
	pkg.addr_pkg.port = port;
	pkg.addr_pkg.is_ipv6 = is_ipv6;

	util::set_checksum(key.c_str(), (char*)&pkg, sizeof(pkg));

	last_send = time(NULL);

	if (send(server_fd, (const char*)&pkg, sizeof(pkg), 0) < 0)
		return false;

	return true;
}

// 服务端来包：前 8 字节为 0 是注册 ACK，非 0 是会话数据
void ipm_client_udp::on_server_readable(evutil_socket_t fd)
{
	char* buf = util::get_udp_buffer();
	std::map<unsigned long long, std::shared_ptr<ipm_client_udp_session>>::iterator iter;
	std::shared_ptr<ipm_client_udp_session> session;
	unsigned long long index = 0;
	int recv_len = 0;

	recv_len = recv(fd, buf, IPM_UDP_BUF_LEN, 0);
	if (recv_len < (int)IPM_UDP_SESSION_LEN)
		return;

	index = util::ntohllx(*(unsigned long long*)buf);

	if (index == 0)
	{
		// 注册 ACK
		if (recv_len != (int)sizeof(alloc_agent_package_t))
			return;
		if (util::check_checksum(key.c_str(), buf, (size_t)recv_len) != true)
			return;

		last_recv = time(NULL);
		hb_pending = false;
		hb_miss = 0;

		if (client_state == CLIENT_STATE::REGISTERING)
		{
			slog_info("udp registered, port = %u", ntohl(((alloc_agent_package_t*)buf)->addr_pkg.port));
			client_state = CLIENT_STATE::RUNNING;
		}
		return;
	}

	last_recv = time(NULL);
	hb_pending = false;
	hb_miss = 0;

	if ((iter = mis_session.find(index)) != mis_session.end())
	{
		session = iter->second;
	}
	else
	{
		// 陌生会话号就是新会话 —— 服务端不用先握手，第一个包直接带数据，
		// 比 TCP 那边还要先回连一次省一个来回
		if ((session = new_session(index)) == NULL)
			return;
	}

	event_add(session->timer, session_tv);
	send(session->fd, buf + IPM_UDP_SESSION_LEN, recv_len - (int)IPM_UDP_SESSION_LEN, 0);
}

// 被代理主机回包：套上会话号和校验发回服务端
void ipm_client_udp::on_session_readable(evutil_socket_t fd, ipm_client_udp_session* session)
{
	char* buf = util::get_udp_buffer();
	char* payload = buf + IPM_UDP_SESSION_LEN;
	size_t pkt_len = 0;
	int recv_len = 0;

	recv_len = recv(fd, payload, IPM_UDP_MAX_PAYLOAD, 0);
	if (recv_len < 0)
		return;

	if (server_fd == -1)
		return;

	event_add(session->timer, session_tv);

	pkt_len = IPM_UDP_SESSION_LEN + (size_t)recv_len;
	*(unsigned long long*)buf = util::htonllx(session->id);

	send(server_fd, buf, (int)pkt_len, 0);
}

std::shared_ptr<ipm_client_udp_session> ipm_client_udp::new_session(unsigned long long id)
{
	std::shared_ptr<ipm_client_udp_session> session;

	if (mis_session.size() >= IPM_UDP_MAX_SESSION)
		return session;

	session = std::make_shared<ipm_client_udp_session>();
	session->id = id;
	session->owner = this;

	if ((session->fd = socket(from_server_addr.ss_family, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		goto fail;

	if (evutil_make_socket_nonblocking(session->fd) != 0)
		goto fail;

	// connect 到被代理主机：回包只会从它来，用 recv 即可
	if (connect(session->fd, (struct sockaddr*)&from_server_addr, from_server_addr_len) != 0)
		goto fail;

	if ((session->read_event = event_new(root_event_base, session->fd, EV_READ | EV_PERSIST, ipm_client_udp_session_readable_callback, session.get())) == NULL)
		goto fail;

	if (event_add(session->read_event, NULL) != 0)
		goto fail;

	if ((session->timer = evtimer_new(root_event_base, ipm_client_udp_session_timeout_callback, session.get())) == NULL)
		goto fail;

	if (event_add(session->timer, session_tv) != 0)
		goto fail;

	mis_session[id] = session;

	return session;
fail:
	// fd 与两个 event 都由 ipm_client_udp_session 的析构负责
	session.reset();
	return session;
}

void ipm_client_udp::on_session_timeout(ipm_client_udp_session* session)
{
	std::map<unsigned long long, std::shared_ptr<ipm_client_udp_session>>::iterator iter;
	std::shared_ptr<ipm_client_udp_session> keep;

	if ((iter = mis_session.find(session->id)) == mis_session.end())
		return;

	// 先攥住一份，保证 erase 之后对象活到回调返回（它的定时器正在执行）
	keep = iter->second;
	mis_session.erase(iter);
}

void ipm_client_udp::on_tick()
{
	time_t now = time(NULL);

	switch (client_state)
	{
	case CLIENT_STATE::REGISTERING:
		// 没有 connect() 失败这种内核信号，注册超时就是 TCP 连接失败的等价物
		if (now - last_send >= IPM_UDP_REG_RETRY)
		{
			if (++reg_tries >= IPM_UDP_REG_TRIES)
			{
				slog_error("udp register timeout, retrying from dns");
				on_fail();
				return;
			}
			send_register();
		}
		break;

	case CLIENT_STATE::RUNNING:
		if (hb_pending)
		{
			if (now - last_send >= IPM_UDP_REG_RETRY)
			{
				if (++hb_miss >= IPM_UDP_HEARTBEAT_TRIES)
				{
					slog_error("udp heartbeat lost, retrying from dns");
					on_fail();
					return;
				}
				send_register();
			}
		}
		// 惰性心跳，且必须以「最后收到」计时。若按「最后发出」算，服务端重启后
		// 客户端还在持续发数据就永远不算空闲，也就永远不会重新注册，会永久卡死
		else if (now - last_recv >= IPM_UDP_HEARTBEAT_IDLE)
		{
			hb_pending = true;
			hb_miss = 0;
			send_register();
		}
		break;

	case CLIENT_STATE::WAITING:
		if (now - wait_since >= (time_t)client_reconn_time)
		{
			slog_info("udp wait over, dns_query_server...");
			// 同上，状态先置好再调用，否则同步回调推进的状态会被冲掉
			client_state = CLIENT_STATE::DNS_QUERYING;
			if (dns_query_server() != true)
			{
				client_state = CLIENT_STATE::WAITING;
				wait_since = now;	// 解析都起不来，下一轮再试
			}
		}
		break;

	default:
		break;
	}
}

void ipm_client_udp_evdns_getaddrinfo_callback(int err, struct evutil_addrinfo* ai, void* arg)
{
	if (arg)	((ipm_client_udp*)arg)->on_evdns_getaddrinfo(err, ai);
}

void ipm_client_udp_server_readable_callback(evutil_socket_t fd, short events, void* user_data)
{
	if (user_data)	((ipm_client_udp*)user_data)->on_server_readable(fd);
}

void ipm_client_udp_session_readable_callback(evutil_socket_t fd, short events, void* user_data)
{
	ipm_client_udp_session* session = (ipm_client_udp_session*)user_data;

	if (session && session->owner)
		session->owner->on_session_readable(fd, session);
}

void ipm_client_udp_session_timeout_callback(evutil_socket_t fd, short events, void* user_data)
{
	ipm_client_udp_session* session = (ipm_client_udp_session*)user_data;

	if (session && session->owner)
		session->owner->on_session_timeout(session);
}

void ipm_client_udp_tick_callback(evutil_socket_t fd, short events, void* user_data)
{
	if (user_data)	((ipm_client_udp*)user_data)->on_tick();
}
