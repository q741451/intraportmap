#include "stdafx.h"

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

ipm_client::ipm_client(struct event_base* base, interface_ipm_client* ptr_interface_p) : root_event_base(base), ptr_interface(ptr_interface_p)
{
	reset();
}

bool ipm_client::init(const char* server_name_c, const char* server_port_name_c, const char* to_server_name_c, const char* to_server_port_name_c, const char* from_server_name_c, const char* from_server_port_name_c, unsigned int client_reconn_time_u)
{
	bool ret = false;

	server_name = server_name_c;
	server_port_name = server_port_name_c;
	to_server_name = to_server_name_c;
	to_server_port_name = to_server_port_name_c;
	from_server_name = from_server_name_c;
	from_server_port_name = from_server_port_name_c;
	client_reconn_time = client_reconn_time_u;

	if (util::getaddrinfo_first(to_server_name.c_str(), to_server_port_name.c_str(), to_server_addr) != true)
	{
		slog_error("getaddrinfo_first to_server_name error");
		goto end;
	}

	if (util::getaddrinfo_first(from_server_name.c_str(), from_server_port_name.c_str(), from_server_addr) != true)
	{
		slog_error("getaddrinfo_first from_server_name error");
		goto end;
	}

	if ((server_evdns_base = evdns_base_new(root_event_base, EVDNS_BASE_DISABLE_WHEN_INACTIVE)) == NULL)
	{
		slog_error("evdns_base_new error");
		goto end;
	}

	if ((timer_event = evtimer_new(root_event_base, ipm_client_timer_event_callback, this)) == NULL)
	{
		slog_error("evtimer_new error");
		goto end;
	}

	// 开始第一步
	slog_info("dns_query_server...");
	if (dns_query_server() != true)
	{
		slog_error("dns_query_server error");
		goto end;
	}

	client_state = CLIENT_STATE::DNS_QUERYING;

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

bool ipm_client::is_init()
{
	return is_state_init;
}

bool ipm_client::exit()
{
	bool ret = false;

	client_exit();

	if (server_evdns_base)
		evdns_base_free(server_evdns_base, 0);

	if (timer_event)
		event_free(timer_event);

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
	timer_event = NULL;
	server_evdns_base = NULL;
	client_reset();
}

void ipm_client::on_fail()
{
	slog_info("on_fail");
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
	slog_info("on_fatal_fail");
	if (ptr_interface)
		ptr_interface->on_interface_ipm_client_fail();
}

void ipm_client::on_evdns_getaddrinfo(int err, struct evutil_addrinfo* result)
{
	struct evutil_addrinfo* rp, * ipv4v6bindall;

	slog_info("on_evdns_getaddrinfo");

	rp = result;

	if (server_name.size() == NULL) {
		ipv4v6bindall = result;

		/* Loop over all address infos found until a IPV6 address is found. */
		while (ipv4v6bindall) {
			if (ipv4v6bindall->ai_family == AF_INET6) {
				rp = ipv4v6bindall; /* Take first IPV6 address available */
				break;
			}
			ipv4v6bindall = ipv4v6bindall->ai_next; /* Get next address info, if any */
		}
	}

	for (/*rp = result*/; rp != NULL; rp = rp->ai_next) {
		slog_info("client_connect_to_server %s", util::get_ipname_from_sockaddr(rp->ai_addr).c_str());
		if (client_connect_to_server(rp->ai_addr, rp->ai_addrlen) != true)
		{
			on_fail();
			goto end;
		}

		// 只取第一个
		break;
	}

	client_state = CLIENT_STATE::SERVER_CONNECTING;
end:
	if (result)
		evutil_freeaddrinfo(result);
}

void ipm_client::on_bufferevent_data_read(struct bufferevent* bev)
{
	struct evbuffer* input = bufferevent_get_input(bev);
	size_t length = evbuffer_get_length(input);
	std::string s_data;
	int bytes_copied = 0;
	int ret_val = -1;
	bool ret = false;

	if (length < 8)
	{
		ret = true;
		goto end;
	}

	s_data.resize(8);

	if ((bytes_copied = evbuffer_remove(input, (char*)s_data.c_str(), 8)) != 8)
	{
		slog_error("evbuffer_remove error");
		goto end;
	}

	// 判断是否成功
	switch (client_state)
	{
	case ipm_client::CLIENT_STATE::SENDING_ALLOC:
		if (util::check_checksum(s_data.c_str(), 4, s_data.c_str() + 4) != true)
		{
			slog_error("check_checksum error");
			goto end;
		}
		ret_val = ntohl(*(unsigned int*)s_data.c_str());
		if (ret_val != 0)
		{
			slog_error("SENDING_ALLOC return error");
			goto end;
		}
		// 成功了，等来数据
		slog_info("SENDING_ALLOC return ok");
		client_state = CLIENT_STATE::RUNNING;
		break;
	case ipm_client::CLIENT_STATE::RUNNING:
		if (util::check_checksum(s_data.c_str(), 4, s_data.c_str() + 4) != true)
		{
			slog_error("check_checksum error");
			goto end;
		}
		ret_val = ntohl(*(unsigned int*)s_data.c_str());
		if (ret_val == 0)
		{
			slog_error("RUNNING return error == 0");
			goto end;
		}
		slog_info("RUNNING alloc %d", ret_val);
		// alloc tunnel
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
	// 是否要释放struct evbuffer* output = bufferevent_get_output(bev);
}

void ipm_client::on_bufferevent_event(struct bufferevent* bev, short flag)
{
	if (flag & BEV_EVENT_READING) {
		slog_error("BEV_EVENT_READING error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_WRITING) {
		slog_error("BEV_EVENT_WRITING error");
		on_fail();
		return;
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
		slog_info("BEV_EVENT_CONNECTED, now sending alloc...");
		if (send_alloc(bev) != true)
		{
			slog_error("send_alloc error");
			on_fail();
			return;
		}
		client_state = CLIENT_STATE::SENDING_ALLOC;
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
	struct evutil_addrinfo hints;

	memset(&hints, 0, sizeof(struct evutil_addrinfo));
	hints.ai_family = AF_UNSPEC;               /* Return IPv4 and IPv6 choices */
	hints.ai_socktype = SOCK_STREAM;             /* We want a TCP socket */
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* For wildcard IP address */
	hints.ai_protocol = IPPROTO_TCP;

	slog_info("resolving %s:%s...", server_name.c_str(), server_port_name.c_str());

	// 挂了也会调cb，无需检测返回值
	evdns_getaddrinfo(server_evdns_base, server_name.c_str(), server_port_name.c_str(), &hints, ipm_client_evdns_getaddrinfo_callback, this);

	ret = true;
//end:
	return ret;
}

bool ipm_client::client_connect_to_server(const struct sockaddr* addr, int addr_length)
{
	bool ret = false;

	if ((server_bufferevent = bufferevent_socket_new(root_event_base, -1, BEV_OPT_CLOSE_ON_FREE)) == NULL)
	{
		slog_error("evdns_base_new error");
		goto end;
	}
	bufferevent_setcb(server_bufferevent, ipm_client_bufferevent_data_read_callback, ipm_client_bufferevent_data_write_callback, ipm_client_bufferevent_event_callback, this);
	bufferevent_enable(server_bufferevent, EV_READ | EV_WRITE);

	if (bufferevent_socket_connect(server_bufferevent, addr, addr_length) != 0)
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
	alloc_agent_package_t* ptr_alloc_agent_package = (alloc_agent_package_t*)malloc(sizeof(alloc_agent_package_t));

	memset(ptr_alloc_agent_package, 0, sizeof(ptr_alloc_agent_package));
	ptr_alloc_agent_package->alloc_zero = htonl(0);
	if (util::sockaddr_to_address((sockaddr*)&to_server_addr, ptr_alloc_agent_package->ip, &ptr_alloc_agent_package->port, &ptr_alloc_agent_package->is_ipv6) != true)
	{
		slog_error("sockaddr_to_address error");
		goto end;
	}
	util::set_checksum((char*)ptr_alloc_agent_package, sizeof(ptr_alloc_agent_package));
	if (bufferevent_write(bev, ptr_alloc_agent_package, sizeof(ptr_alloc_agent_package)) != 0)
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
	if (timer_event)
		evtimer_del(timer_event);

	if (server_bufferevent)
		bufferevent_free(server_bufferevent);

	return true;
}

void ipm_client::client_reset()
{
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

void intraportmap_signal_event_callback(evutil_socket_t sig, short events, void* user_data)
{
	if (user_data)
		((intraportmap*)user_data)->on_signal_event(sig, events);
}

intraportmap::intraportmap()
{
	reset();
}

bool intraportmap::init(int argc, char* argv[])
{
	bool ret = false;

	if (init_config(argc, argv) != true)
		goto end;

	if ((root_event_base = event_base_new()) == NULL)
	{
		slog_error("event_base_new error");
		goto end;
	}

	if (register_signal_event() != true)
		goto end;

	if (is_server)
	{
		slog_info("run as server");
	}
	else
	{
		slog_info("run as client");
		sp_ipm_client = std::make_shared<ipm_client>(root_event_base, this);
		if (sp_ipm_client->init(server_name.c_str(), server_port_name.c_str(), to_server_name.c_str(), to_server_port_name.c_str(), from_server_name.c_str(), from_server_port_name.c_str(), client_reconn_time) != true)
		{
			slog_error("sp_ipm_client init error");
			goto end;
		}
	}


	ret = true;
end:
	if (ret == true)
	{
		is_state_init = true;
	}
	else
	{
		exit();
		reset();
	}
	return ret;
}

bool intraportmap::is_init()
{
	return is_state_init;
}

bool intraportmap::exit()
{
	bool ret = false;

	if (signal_event)
	{
		event_free(signal_event);
	}

	if (sp_ipm_client)
	{
		if (sp_ipm_client->is_init())
			sp_ipm_client->exit();
	}

	if (root_event_base)
		event_base_free(root_event_base);

	is_state_init = false;

	ret = true;
//end:
	return ret;
}

void intraportmap::reset()
{
	is_server = true;
	is_state_init = false;
	client_reconn_time = 15;
	root_event_base = NULL;
	signal_event = NULL;
	if (sp_ipm_client)
	{
		sp_ipm_client->reset();
	}
	sp_ipm_client.reset();
}

void intraportmap::exec()
{
	event_base_dispatch(root_event_base);
}

void intraportmap::on_interface_ipm_client_fail()
{
	slog_error("on_interface_ipm_client_fail");
	event_base_loopbreak(root_event_base);
}

void intraportmap::on_signal_event(evutil_socket_t sig, short events)
{
	event_base_loopbreak(root_event_base);
}

bool intraportmap::init_config(int argc, char* argv[])
{
	bool ret = false;
	std::string server_full;
	std::string to_full;
	std::string from_full;
	int opt = 0;

	while ((opt = getopt(argc, argv, "cs:t:f:w:")) != -1) {
		switch (opt) {
		case 'c':
			is_server = false;
			break;
		case 's':
			server_full = optarg;
			break;
		case 't':
			to_full = optarg;
			break;
		case 'f':
			from_full = optarg;
			break;
		case 'w':
			if (sscanf(optarg, "%u", &client_reconn_time) <= 0)
			{
				slog_error("error option w %s", optarg);
				goto end;
			}
			break;
		default:
			slog_error("Unknown option %c", opt);
			goto end;
		}
	}

	if (util::split_addr_string(server_full.c_str(), server_name, server_port_name) != true)
	{
		slog_error("server_full error");
		goto end;
	}

	if (!is_server)
	{
		if (util::split_addr_string(to_full.c_str(), to_server_name, to_server_port_name) != true)
		{
			slog_error("to_full error");
			goto end;
		}
		if (util::split_addr_string(from_full.c_str(), from_server_name, from_server_port_name) != true)
		{
			slog_error("from_full error");
			goto end;
		}
	}

	ret = true;
end:
	return ret;
}

bool intraportmap::register_signal_event()
{
	bool ret = false;

	if ((signal_event = evsignal_new(root_event_base, SIGINT, intraportmap_signal_event_callback, this)) == NULL)
	{
		slog_error("evsignal_new error");
		goto end;
	}

	if (event_add(signal_event, NULL) != 0)
	{
		slog_error("event_add error");
		goto end;
	}

	ret = true;
end:
	if (signal_event)
	{
		event_free(signal_event);
		signal_event = NULL;
	}
	return ret;
}

