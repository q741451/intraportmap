#include "stdafx.h"

void ipm_server_listener_callback(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr* sa, int socklen, void* user_data);

void ipm_server_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx);
void ipm_server_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx);
void ipm_server_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx);

bool operator < (const addr_pkg_idx& item1, const addr_pkg_idx& item2)
{
	return memcmp(&item1.addr_pkg, &item2.addr_pkg, sizeof(address_package_t)) < 0;
}

ipm_server::ipm_server(struct event_base* base, interface_ipm_server* ptr_interface_p) : ptr_interface(ptr_interface_p), root_event_base(base)
{
	reset();
}

bool ipm_server::init(const char* server_name_c, const char* server_port_name_c, size_t max_buffer_sz)
{
	bool ret = false;

	server_name = server_name_c;
	server_port_name = server_port_name_c;
	max_buffer = max_buffer_sz;

	if (util::getaddrinfo_first(server_name.c_str(), server_port_name.c_str(), server_addr, &server_addr_len) != true)
	{
		slog_error("getaddrinfo_first server_name error");
		goto end;
	}

	slog_info("ipm_server start at [%s]:%s", util::get_ipname_from_sockaddr((struct sockaddr*)&server_addr).c_str(), util::get_portstr_from_sockaddr((struct sockaddr*)&server_addr).c_str());

	if (start_listener((struct sockaddr*)&server_addr, server_addr_len) != true)
	{
		slog_error("start_listener error");
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

bool ipm_server::is_init()
{
	return is_state_init;
}

bool ipm_server::exit()
{
	bool ret = false;
	std::set<bufferevent*>::iterator iter_sbe;
	std::map<bufferevent*, std::shared_ptr<ipm_server_agent>>::iterator iter_agent;
	std::map<evutil_socket_t, std::shared_ptr<ipm_server_tunnel>>::iterator iter_tunnel;

	for (iter_tunnel = mst_tunnel.begin(); iter_tunnel != mst_tunnel.end(); ++iter_tunnel)
	{
		if (iter_tunnel->second->is_init())
			iter_tunnel->second->exit();
	}

	for (iter_agent = msa_agent.begin(); iter_agent != msa_agent.end(); ++iter_agent)
	{
		if (iter_agent->second->is_init())
			iter_agent->second->exit();
	}

	for (iter_sbe = sbe_bufferevent.begin(); iter_sbe != sbe_bufferevent.end(); ++iter_sbe)
	{
		if (*iter_sbe)
			bufferevent_free(*iter_sbe);
	}

	if(listener)
		evconnlistener_free(listener);

	if (listener6)
		evconnlistener_free(listener6);

	is_state_init = false;

	ret = true;
//end:
	return ret;
}

void ipm_server::reset()
{
	is_state_init = false;
	server_state = SERVER_STATE::IDLE;
	server_addr_len = 0;
	max_buffer = DEF_MAX_BUFFER;
	sbe_bufferevent.clear();
	msa_agent.clear();
	asa_agent.clear();
	mst_tunnel.clear();
	listener = NULL;
	listener6 = NULL;
}

void ipm_server::on_interface_ipm_server_agent_fail(bufferevent* bev)
{
	std::map<bufferevent*, std::shared_ptr<ipm_server_agent>>::iterator iter;

	slog_error("server agent_fail %p", bev);

	if ((iter = msa_agent.find(bev)) == msa_agent.end())
	{
		slog_error("msa_agent error");
		on_fail();
		return;
	}

	asa_agent.erase(iter->second->get_addr_pkg_idx());
	iter->second->exit();
	msa_agent.erase(iter);
}

bool ipm_server::on_interface_ipm_server_agent_new_fd_tunnel(bufferevent* bev, bufferevent* to_bev, evutil_socket_t to_fd)
{
	std::shared_ptr<ipm_server_tunnel> st_tunnel = std::make_shared<ipm_server_tunnel>(root_event_base, this, to_fd);
	bool ret = false;

	if (st_tunnel->init(to_bev, max_buffer) != true)
	{
		slog_error("st_tunnel->init(to_bev) error");
		goto end;
	}

	mst_tunnel[to_fd] = st_tunnel;

	ret = true;
end:
	if (ret != true)
	{
		if (st_tunnel->is_init())
			st_tunnel->exit();
	}
	return ret;
}

void ipm_server::on_interface_ipm_server_tunnel_fail(evutil_socket_t to_fd)
{
	std::map<evutil_socket_t, std::shared_ptr<ipm_server_tunnel>>::iterator iter;

	slog_debug("server tunnel_fail %llu", (unsigned long long)to_fd);

	if ((iter = mst_tunnel.find(to_fd)) == mst_tunnel.end())
	{
		slog_error("mst_tunnel error");
		on_fail();
		return;
	}

	iter->second->exit();
	mst_tunnel.erase(iter);
}

void ipm_server::on_fail()
{
	slog_error("server on_fail");
	if (ptr_interface)
		ptr_interface->on_interface_ipm_server_fail();
}

void ipm_server::on_listener(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr* sa, int socklen)
{
	bool ret = false;
	struct bufferevent* buff_bev = NULL;

	if ((buff_bev = bufferevent_socket_new(root_event_base, fd, BEV_OPT_CLOSE_ON_FREE)) == NULL)
	{
		slog_error("bufferevent_socket_new error");
		goto end;
	}

	bufferevent_setcb(buff_bev, ipm_server_bufferevent_data_read_callback, ipm_server_bufferevent_data_write_callback, ipm_server_bufferevent_event_callback, this);
	if (bufferevent_enable(buff_bev, EV_READ | EV_WRITE) != 0)
	{
		slog_error("bufferevent_enable error");
		goto end;
	}

	sbe_bufferevent.insert(buff_bev);

	ret = true;
end:
	if (ret != true)
	{
		if (buff_bev)
		{
			bufferevent_free(buff_bev);
			buff_bev = NULL;
		}
		else if (fd != -1)
		{
			evutil_closesocket(fd);
		}
	}
}

void ipm_server::on_bufferevent_data_read(struct bufferevent* bev)
{
	struct evbuffer* input = bufferevent_get_input(bev);
	size_t length = evbuffer_get_length(input);
	size_t use_len = 0;
	unsigned long long index;
	int bytes_copied = 0;
	unsigned char* buffer;
	bool is_fatal = false;
	alloc_agent_package_t* ag_agent = NULL;
	std::string s_data;
	bool ret = false;

	use_len = 8;
	if (length < use_len)
	{
		ret = true;
		goto end;
	}
	if ((buffer = evbuffer_pullup(input, use_len)) == NULL)
		goto end;

	index = util::ntohll(*(unsigned long long*)buffer);
	if (index == 0)
	{
		use_len = sizeof(alloc_agent_package_t);
		// 开辟新的服务
		if (length < use_len)
		{
			ret = true;
			goto end;
		}
		// 已准备好
		ag_agent = (alloc_agent_package_t*)malloc(use_len);
		if (ag_agent == NULL)
		{
			slog_error("ag_agent null error");
			goto end;
		}
		if ((bytes_copied = evbuffer_remove(input, (char*)ag_agent, use_len)) != (int)use_len)
		{
			slog_error("evbuffer_remove error");
			goto end;
		}
		if (util::check_checksum((char*)ag_agent, use_len) != true)
		{
			slog_error("check_checksum error");
			goto end;
		}
		slog_info("alloc_agent client_bev = %p port = %u", bev, ntohl(ag_agent->addr_pkg.port));
		if (alloc_agent(bev, ag_agent, is_fatal) != true)
		{
			// 此时未托管
			slog_error("alloc_agent error");
			goto end;
		}
	}
	else
	{
		use_len = 12;
		// 寻找待连接的Tunnel
		if (length < use_len)
		{
			ret = true;
			goto end;
		}
		s_data.resize(use_len);
		if ((bytes_copied = evbuffer_remove(input, (char*)s_data.c_str(), use_len)) != (int)use_len)
		{
			slog_error("evbuffer_remove error");
			goto end;
		}
		if (util::check_checksum(s_data.c_str(), use_len) != true)
		{
			slog_error("check_checksum error");
			goto end;
		}
		slog_info("join_tunnel_client index = %llu", index);
		// 已准备好
		if (join_tunnel_client(bev, index) != true)
		{
			// 此时未托管
			slog_error("join_tunnel_client error");
			goto end;
		}
	}

	// 已托管remove buffer
	if (remove_bufferevent(bev) != true)
	{
		// fatal
		slog_error("remove_bufferevent error");
		is_fatal = true;
		goto end;
	}

	ret = true;
end:
	if (ag_agent) free(ag_agent);
	if (ret != true)
	{
		if (close_and_remove_bufferevent(bev) != true)
		{
			slog_error("close_and_remove_bufferevent error");
			is_fatal = true;
		}
	}
	if (is_fatal)
	{
		on_fail();
	}
}

void ipm_server::on_bufferevent_data_write(struct bufferevent* bev)
{
	// 不用处理
}

void ipm_server::on_bufferevent_event(struct bufferevent* bev, short flag)
{
	slog_error("server event flag = %u", (unsigned int)flag);
	if (flag & BEV_EVENT_READING) {
	}

	if (flag & BEV_EVENT_WRITING) {
	}

	if (flag & BEV_EVENT_ERROR) {
		if (close_and_remove_bufferevent(bev) != true)
		{
			slog_error("close_and_remove_bufferevent error");
			on_fail();
		}
		return;
	}

	if (flag & BEV_EVENT_TIMEOUT) {
		if (close_and_remove_bufferevent(bev) != true)
		{
			slog_error("close_and_remove_bufferevent error");
			on_fail();
		}
		return;
	}

	if (flag & BEV_EVENT_EOF) {
		if (close_and_remove_bufferevent(bev) != true)
		{
			slog_error("close_and_remove_bufferevent error");
			on_fail();
		}
		return;
	}

	if (flag & BEV_EVENT_CONNECTED) {
		// 应该不会来这个
		slog_error("BEV_EVENT_CONNECTED ?");
		on_fail();
	}
}

bool ipm_server::start_listener(const struct sockaddr* addr, int addr_length)
{
	bool ret = false;
	struct sockaddr_in6 zero_in6;

	if (addr->sa_family == AF_INET6)
	{
		if (
#ifdef LEV_OPT_BIND_IPV6ONLY
			(listener6 = evconnlistener_new_bind(root_event_base, ipm_server_listener_callback, this, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE | LEV_OPT_BIND_IPV6ONLY, -1, (struct sockaddr*)addr, addr_length)) == NULL &&
#endif
			(listener6 = evconnlistener_new_bind(root_event_base, ipm_server_listener_callback, this, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1, (struct sockaddr*)addr, addr_length)) == NULL)
		{
			slog_error("ipv6 listen error");
			goto end;
		}
			

		memset(&zero_in6, 0, sizeof(zero_in6));

		if (memcmp(&((struct sockaddr_in6*)addr)->sin6_addr, &zero_in6.sin6_addr, sizeof(zero_in6.sin6_addr)) == 0)
		{
			// 监听所有，需要同时监听ipv4

			struct sockaddr_in addr_in4_all;
			memset(&addr_in4_all, 0, sizeof(struct sockaddr_in));
			addr_in4_all.sin_family = AF_INET;
			addr_in4_all.sin_addr.s_addr = htonl(INADDR_ANY);
			addr_in4_all.sin_port = ((struct sockaddr_in6*)addr)->sin6_port;

			if ((listener = evconnlistener_new_bind(root_event_base, ipm_server_listener_callback, this, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1, (struct sockaddr*)&addr_in4_all, sizeof(addr_in4_all))) == NULL)
			{
				slog_warn("dual-stack ipv4 listen error");
			}

		}
	}
	else if (addr->sa_family == AF_INET)
	{
		if ((listener = evconnlistener_new_bind(root_event_base, ipm_server_listener_callback, this, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1, (struct sockaddr*)addr, addr_length)) == NULL)
		{
			slog_error("ipv4 listen error");
			goto end;
		}
	}
	else
	{
		slog_error("unknown sa_family listen error");
		goto end;
	}

	ret = true;
end:
	return ret;
}

bool ipm_server::remove_bufferevent(bufferevent* buf_event)
{
	bool ret = false;
	std::set<bufferevent*>::iterator iter;

	if ((iter = sbe_bufferevent.find(buf_event)) == sbe_bufferevent.end())
		goto end;

	sbe_bufferevent.erase(iter);

	ret = true;
end:
	return ret;
}

bool ipm_server::close_and_remove_bufferevent(bufferevent* buf_event)
{
	bool ret = false;
	std::set<bufferevent*>::iterator iter;

	if ((iter = sbe_bufferevent.find(buf_event)) == sbe_bufferevent.end())
		goto end;

	bufferevent_free(*iter);

	sbe_bufferevent.erase(iter);

	ret = true;
end:
	return ret;
}

bool ipm_server::alloc_agent(struct bufferevent* bev, alloc_agent_package_t* pkg, bool& is_fatal)
{
	bool ret = false;
	addr_pkg_idx addr_idx;
	std::shared_ptr<ipm_server_agent> sag;
	std::map<addr_pkg_idx, std::shared_ptr<ipm_server_agent>>::iterator iter_idx;
	std::map<bufferevent*, std::shared_ptr<ipm_server_agent>>::iterator iter_bev;

	addr_idx.addr_pkg = pkg->addr_pkg;
	if ((iter_idx = asa_agent.find(addr_idx)) != asa_agent.end())
	{
		slog_warn("asa_agent port %u is in use, agent %p, exit now", ntohl(addr_idx.addr_pkg.port), iter_idx->second->get_client_bufferevent());

		if ((iter_bev = msa_agent.find(iter_idx->second->get_client_bufferevent())) == msa_agent.end())
		{
			slog_error("msa_agent error");
			is_fatal = true;
			goto end;
		}

		asa_agent.erase(iter_idx);
		iter_bev->second->exit();
		msa_agent.erase(iter_bev);
	}

	sag = std::make_shared<ipm_server_agent>(root_event_base, this);
	
	if (sag->init(addr_idx, bev) != true)
		goto end;

	msa_agent[bev] = sag;
	asa_agent[sag->get_addr_pkg_idx()] = sag;

	ret = true;
end:
	if (ret != true)
	{
		if (sag && sag->is_init())
			sag->exit();
	}
	return ret;
}

bool ipm_server::join_tunnel_client(struct bufferevent* bev, unsigned long long index)
{
	bool ret = false;
	std::map<evutil_socket_t, std::shared_ptr<ipm_server_tunnel>>::iterator iter;
	evutil_socket_t fd = (evutil_socket_t)index;

	if ((iter = mst_tunnel.find(fd)) == mst_tunnel.end())
		goto end;

	if (iter->second->client_connected(bev) != true)
		goto end;

	ret = true;
end:
	return ret;
}

void ipm_server_listener_callback(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr* sa, int socklen, void* user_data)
{
	if (user_data)	((ipm_server*)user_data)->on_listener(listener, fd, sa, socklen);
}


void ipm_server_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx)
{
	if (ctx)	((ipm_server*)ctx)->on_bufferevent_data_read(bev);
}

void ipm_server_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx)
{
	if (ctx)	((ipm_server*)ctx)->on_bufferevent_data_write(bev);
}

void ipm_server_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx)
{
	if (ctx)	((ipm_server*)ctx)->on_bufferevent_event(bev, what);
}
