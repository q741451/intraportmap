#include "stdafx.h"

void ipm_client_evdns_getaddrinfo_callback(int err, struct evutil_addrinfo* ai, void* arg);
void ipm_client_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx);
void ipm_client_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx);
void ipm_client_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx);
void ipm_client_timer_event_callback(evutil_socket_t sig, short events, void* user_data);

ipm_client::ipm_client(struct event_base* base, interface_ipm_client* ptr_interface_p) :  ptr_interface(ptr_interface_p), root_event_base(base)
{
	reset();
}

bool ipm_client::init(const char* server_name_c, const char* server_port_name_c, const char* to_server_name_c, const char* to_server_port_name_c, const char* from_server_name_c, const char* from_server_port_name_c, unsigned int client_reconn_time_u, size_t max_buffer_sz)
{
	bool ret = false;

	server_name = server_name_c;
	server_port_name = server_port_name_c;
	to_server_name = to_server_name_c;
	to_server_port_name = to_server_port_name_c;
	from_server_name = from_server_name_c;
	from_server_port_name = from_server_port_name_c;
	client_reconn_time = client_reconn_time_u;
	max_buffer = max_buffer_sz;

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

	if ((timer_event = evtimer_new(root_event_base, ipm_client_timer_event_callback, this)) == NULL)
	{
		slog_error("evtimer_new error");
		goto end;
	}

	ret = true;

	// 开始第一步，允许失败
	if (dns_query_server() != true)
	{
		slog_error("dns_query_server error");
		on_fail();
		goto end;
	}

	client_state = CLIENT_STATE::DNS_QUERYING;
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

bool ipm_client::is_init()
{
	return is_state_init;
}

bool ipm_client::exit()
{
	bool ret = false;
	std::map<unsigned long long, std::shared_ptr<ipm_client_tunnel>>::iterator iter_tunnel;

	client_exit();

	if (timer_event)
		event_free(timer_event);

	for (iter_tunnel = mst_tunnel.begin(); iter_tunnel != mst_tunnel.end(); ++iter_tunnel)
	{
		if (iter_tunnel->second->is_init())
			iter_tunnel->second->exit();
	}

	is_state_init = false;

	ret = true;
	//end:
	return ret;
}

void ipm_client::reset()
{
	client_state = CLIENT_STATE::IDLE;
	client_reconn_time = 15;
	is_state_init = false;
	server_addr_len = 0;
	to_server_addr_len = 0;
	from_server_addr_len = 0;
	max_buffer = DEF_MAX_BUFFER;
	timer_event = NULL;
	mst_tunnel.clear();
	client_reset();
}

void ipm_client::on_interface_ipm_tunnel_client_fail(unsigned long long index)
{
	std::map<unsigned long long, std::shared_ptr<ipm_client_tunnel>>::iterator iter_tunnel;

	slog_info("tunnel client fail %llu", index);

	if ((iter_tunnel = mst_tunnel.find(index)) == mst_tunnel.end())
	{
		slog_error("unknown tunnel index");
		on_fatal_fail();
		return;
	}

	if (iter_tunnel->second->is_init())
		iter_tunnel->second->exit();

	mst_tunnel.erase(iter_tunnel);
}

void ipm_client::on_fail()
{
	slog_info("client on_fail");
	// 清理全部临时变量
	if (client_exit() != true)
	{
		slog_error("client_exit error");
		on_fatal_fail();
		return;
	}

	client_reset();

	// 设置定时器
	if (set_reconnect_timmer() != true)
	{
		on_fatal_fail();
		return;
	}
	client_state = CLIENT_STATE::WAITING;
}

void ipm_client::on_fatal_fail()
{
	slog_info("client on_fatal_fail");
	if (ptr_interface)
		ptr_interface->on_interface_ipm_client_fail();
}

void ipm_client::on_evdns_getaddrinfo(int err, struct evutil_addrinfo* result)
{
	struct evutil_addrinfo* rp;
	bool ret = false;

	slog_info("on_evdns_getaddrinfo");

	rp = result;

	server_addr_len = 0;

	for (/*rp = result*/; rp != NULL; rp = rp->ai_next) {
		slog_info("client_connect_to_server [%s]:%s", util::get_ipname_from_sockaddr(rp->ai_addr).c_str(), util::get_portstr_from_sockaddr(rp->ai_addr).c_str());
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

	if (client_connect_to_server((struct sockaddr*)&server_addr, server_addr_len) != true)
	{
		slog_error("client_connect_to_server error");
		goto end;
	}

	ret = true;
	client_state = CLIENT_STATE::SERVER_CONNECTING;
end:
	if (result)
		evutil_freeaddrinfo(result);
	if (ret != true)
		on_fail();
}

void ipm_client::on_bufferevent_data_read(struct bufferevent* bev)
{
	struct evbuffer* input = bufferevent_get_input(bev);
	size_t length = 0;
	size_t use_len = 0;
	std::string s_data;
	int bytes_copied = 0;
	unsigned long long ret_val = 0;
	bool ret = false;

	// 判断是否成功
	switch (client_state)
	{
	case ipm_client::CLIENT_STATE::RUNNING:
		use_len = 12;
		s_data.resize(use_len);
		while ((length = evbuffer_get_length(input)) >= use_len)
		{
			if ((bytes_copied = evbuffer_remove(input, (char*)s_data.c_str(), use_len)) != 12)
			{
				slog_error("evbuffer_remove error");
				goto end;
			}
			if (util::check_checksum(s_data.c_str(), use_len) != true)
			{
				slog_error("check_checksum error");
				goto end;
			}
			ret_val = util::ntohll(*(unsigned long long*)s_data.c_str());
			if (ret_val == 0)
			{
				slog_error("RUNNING return error == 0");
				goto end;
			}
			slog_info("RUNNING alloc %llu", ret_val);
			{
				std::shared_ptr<ipm_client_tunnel> st_tunnel = std::make_shared<ipm_client_tunnel>(root_event_base, this, ret_val);
				std::map<unsigned long long, std::shared_ptr<ipm_client_tunnel>>::iterator iter_tunnel;

				// 重复则覆盖掉
				if ((iter_tunnel = mst_tunnel.find(ret_val)) != mst_tunnel.end())
				{
					if (iter_tunnel->second->is_init())
						iter_tunnel->second->exit();
					mst_tunnel.erase(iter_tunnel);
				}

				// 允许启动失败
				if (st_tunnel->init(server_addr, server_addr_len, from_server_addr, from_server_addr_len, max_buffer) != true)
				{
					slog_error("st_tunnel->init error");
					ret = true;
					goto end;
				}

				// 启动成功
				mst_tunnel[ret_val] = st_tunnel;
			}
		}
		break;
	default:
		slog_error("unknown state error");
		goto end;
	}

	ret = true;
end:
	if (ret != true)
		on_fail();
	return;
}

void ipm_client::on_bufferevent_data_write(struct bufferevent* bev)
{
	// 写完了
}

void ipm_client::on_bufferevent_event(struct bufferevent* bev, short flag)
{
	slog_info("bufferevent event flag = %u", (unsigned int)flag);
	if (flag & BEV_EVENT_READING) {
		slog_error("BEV_EVENT_READING error");
	}

	if (flag & BEV_EVENT_WRITING) {
		slog_error("BEV_EVENT_WRITING error");
	}

	if (flag & BEV_EVENT_ERROR) {
		slog_error("BEV_EVENT_ERROR error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_TIMEOUT) {
		slog_error("BEV_EVENT_TIMEOUT error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_EOF) {
		slog_error("BEV_EVENT_EOF error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_CONNECTED) {
		// 发送
		slog_info("BEV_EVENT_CONNECTED, now sending alloc");
		if (send_alloc(bev) != true)
		{
			slog_error("send_alloc error");
			on_fail();
			return;
		}
		client_state = CLIENT_STATE::RUNNING;
	}
}

void ipm_client::on_timer_event(evutil_socket_t sig, short events)
{
	slog_info("wait over, dns_query_server...");
	if (dns_query_server() != true)
	{
		slog_error("dns_query_server error");
		on_fail();
		return;
	}

	client_state = CLIENT_STATE::DNS_QUERYING;
}

bool ipm_client::dns_query_server()
{
	bool ret = false;
	evdns_getaddrinfo_request* request = NULL;	// 无需释放
	struct evutil_addrinfo hints;

	memset(&hints, 0, sizeof(struct evutil_addrinfo));
	hints.ai_family = AF_UNSPEC;               /* Return IPv4 and IPv6 choices */
	hints.ai_protocol = IPPROTO_TCP;

	slog_info("resolving %s:%s...", server_name.c_str(), server_port_name.c_str());

	if ((server_evdns_base = evdns_base_new(root_event_base, 1)) == NULL)
	{
		slog_error("evdns_base_new error");
		goto end;
	}

	// request = NULL时， cb已调用， request不为NULL，异步，如果win下没dns则取消，否则不回调
	if ((request = evdns_getaddrinfo(server_evdns_base, server_name.c_str(), server_port_name.c_str(), &hints, ipm_client_evdns_getaddrinfo_callback, this)) != NULL)
	{
		if (evdns_base_count_nameservers(server_evdns_base) == 0)
		{
			slog_error("evdns_base_count_nameservers error, cancel request");
			if (request)
				evdns_getaddrinfo_cancel(request);
		}
	}

	ret = true;
end:
	return ret;
}

bool ipm_client::client_connect_to_server(const struct sockaddr* addr, int addr_length)
{
	bool ret = false;

	if ((server_bufferevent = bufferevent_socket_new(root_event_base, -1, BEV_OPT_CLOSE_ON_FREE)) == NULL)
	{
		slog_error("bufferevent_socket_new error");
		goto end;
	}
	bufferevent_setcb(server_bufferevent, ipm_client_bufferevent_data_read_callback, ipm_client_bufferevent_data_write_callback, ipm_client_bufferevent_event_callback, this);
	if (bufferevent_enable(server_bufferevent, EV_READ | EV_WRITE) != 0)
	{
		slog_error("bufferevent_enable error");
		goto end;
	}

	if (bufferevent_socket_connect(server_bufferevent, (struct sockaddr*)addr, addr_length) != 0)
	{
		slog_error("bufferevent_socket_connect error");
		goto end;
	}

	ret = true;
end:
	if (ret != true)
	{
		if (server_bufferevent)
		{
			bufferevent_free(server_bufferevent);
			server_bufferevent = NULL;
		}
	}
	return ret;
}

bool ipm_client::send_alloc(struct bufferevent* bev)
{
	bool ret = false;
	unsigned int port = 0;
	unsigned int is_ipv6 = 0;
	size_t buf_len = sizeof(alloc_agent_package_t);
	alloc_agent_package_t* ptr_alloc_agent_package = (alloc_agent_package_t*)malloc(buf_len);

	if (ptr_alloc_agent_package == NULL)
		goto end;

	memset(ptr_alloc_agent_package, 0, buf_len);
	ptr_alloc_agent_package->alloc_zero = util::htonll(0);
	if (util::sockaddr_to_address((sockaddr*)&to_server_addr, ptr_alloc_agent_package->addr_pkg.ip, &port, &is_ipv6) != true)
	{
		slog_error("sockaddr_to_address error");
		goto end;
	}
	ptr_alloc_agent_package->addr_pkg.port = port;
	ptr_alloc_agent_package->addr_pkg.is_ipv6 = is_ipv6;
	util::set_checksum((char*)ptr_alloc_agent_package, buf_len);
	if (bufferevent_write(bev, ptr_alloc_agent_package, buf_len) != 0)
	{
		slog_error("send_alloc error");
		goto end;
	}

	ret = true;
end:
	if (ptr_alloc_agent_package) free(ptr_alloc_agent_package);
	return ret;
}

bool ipm_client::client_exit()
{
	if (server_evdns_base)
		evdns_base_free(server_evdns_base, 0);

	if (timer_event)
		evtimer_del(timer_event);

	if (server_bufferevent)
		bufferevent_free(server_bufferevent);

	return true;
}

void ipm_client::client_reset()
{
	server_evdns_base = NULL;
	server_bufferevent = NULL;
}

bool ipm_client::set_reconnect_timmer()
{
	bool ret = false;
	static timeval tl;

	tl.tv_sec = client_reconn_time;

	if (evtimer_add(timer_event, &tl) != 0)
	{
		slog_error("evtimer_add error");
		goto end;
	}

	ret = true;
end:
	return ret;
}


void ipm_client_evdns_getaddrinfo_callback(int err, struct evutil_addrinfo* ai, void* arg)
{
	if (arg)	((ipm_client*)arg)->on_evdns_getaddrinfo(err, ai);
}

void ipm_client_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx)
{
	if (ctx)	((ipm_client*)ctx)->on_bufferevent_data_read(bev);
}

void ipm_client_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx)
{
	if (ctx)	((ipm_client*)ctx)->on_bufferevent_data_write(bev);
}

void ipm_client_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx)
{
	if (ctx)	((ipm_client*)ctx)->on_bufferevent_event(bev, what);
}

void ipm_client_timer_event_callback(evutil_socket_t sig, short events, void* user_data)
{
	if (user_data)	((ipm_client*)user_data)->on_timer_event(sig, events);
}