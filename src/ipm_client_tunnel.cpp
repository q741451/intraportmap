#include "stdafx.h"

void ipm_client_tunnel_server_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx);
void ipm_client_tunnel_server_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx);
void ipm_client_tunnel_from_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx);

ipm_client_tunnel::ipm_client_tunnel(struct event_base* base, interface_ipm_client_tunnel* ptr_interface_p, unsigned long long index_u) : ptr_interface(ptr_interface_p), root_event_base(base), index(index_u), relay(base, this, index_u)
{
	reset();
}

bool ipm_client_tunnel::init(struct sockaddr_storage& server_addr_ss, unsigned int server_addr_len_u, struct sockaddr_storage& from_server_addr_ss, unsigned int from_server_addr_len_u, const char* key_c, size_t max_buffer_sz)
{
	bool ret = false;

	server_addr = server_addr_ss;
	server_addr_len = server_addr_len_u;
	from_server_addr = from_server_addr_ss;
	from_server_addr_len = from_server_addr_len_u;
	key = key_c;

	if (relay.init(max_buffer_sz) != true)
	{
		slog_debug("relay.init error");
		goto end;
	}

	if (connect_to_server() != true)
	{
		slog_debug("connect_to_server error");
		goto end;
	}

	// 握手排在最前，连上后先发出去
	if (send_penetrate(server_bufferevent) != true)
	{
		slog_debug("send_penetrate error");
		goto end;
	}

	if (connect_to_from() != true)
	{
		slog_debug("connect_to_from error");
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

bool ipm_client_tunnel::is_init()
{
	return is_state_init;
}

bool ipm_client_tunnel::exit()
{
	if (server_bufferevent)
		bufferevent_free(server_bufferevent);

	if (from_bufferevent)
		bufferevent_free(from_bufferevent);

	relay.exit();

	is_state_init = false;
	return true;
}

void ipm_client_tunnel::reset()
{
	is_state_init = false;
	server_addr_len = 0;
	from_server_addr_len = 0;
	is_server_ready = false;
	is_from_ready = false;
	server_bufferevent = NULL;
	from_bufferevent = NULL;
	relay.reset();
}

void ipm_client_tunnel::on_interface_ipm_relay_close()
{
	return on_fail();
}

void ipm_client_tunnel::on_fail()
{
	slog_debug("client_tunnel on_fail %llu", index);
	if (ptr_interface)
		ptr_interface->on_interface_ipm_tunnel_client_fail(index);
}

void ipm_client_tunnel::on_server_bufferevent_data_write_callback(struct bufferevent* bev)
{
	// 握手已发完
	is_server_ready = true;
	return start_relay();
}

void ipm_client_tunnel::on_server_bufferevent_event_callback(struct bufferevent* bev, short flag)
{
	slog_debug("tunnel server event %llu, flag = %u", index, (unsigned int)flag);

	if (flag & (BEV_EVENT_ERROR | BEV_EVENT_TIMEOUT | BEV_EVENT_EOF)) {
		slog_debug("server connect error");
		return on_fail();
	}

	if (flag & BEV_EVENT_CONNECTED) {
		slog_debug("server BEV_EVENT_CONNECTED");
		if (util::set_evutil_socket_keepalive(bufferevent_getfd(bev)) != true)
		{
			slog_debug("set_evutil_socket_keepalive error");
			return on_fail();
		}
	}
}

void ipm_client_tunnel::on_from_bufferevent_event_callback(struct bufferevent* bev, short flag)
{
	slog_debug("tunnel from event %llu, flag = %u", index, (unsigned int)flag);

	if (flag & (BEV_EVENT_ERROR | BEV_EVENT_TIMEOUT | BEV_EVENT_EOF)) {
		slog_error("from connect error");
		return on_fail();
	}

	if (flag & BEV_EVENT_CONNECTED) {
		slog_info("from BEV_EVENT_CONNECTED, done");
		if (util::set_evutil_socket_keepalive(bufferevent_getfd(bev)) != true)
		{
			slog_debug("set_evutil_socket_keepalive error");
			return on_fail();
		}
		is_from_ready = true;
		return start_relay();
	}
}

void ipm_client_tunnel::start_relay()
{
	if (!is_server_ready || !is_from_ready)
		return;

	// bev 留到 exit 释放，fd 已交给 relay
	if (relay.attach(SIDE_SERVER, server_bufferevent) != true)
	{
		slog_debug("relay.attach server error");
		return on_fail();
	}

	if (relay.attach(SIDE_FROM, from_bufferevent) != true)
	{
		slog_debug("relay.attach from error");
		return on_fail();
	}

	if (relay.start() != true)
	{
		slog_debug("relay.start error");
		return on_fail();
	}
}

// 连接阶段不读：数据留在内核里，等两边都就绪再由 relay 读
bool ipm_client_tunnel::connect_to_server()
{
	bool ret = false;

	if ((server_bufferevent = bufferevent_socket_new(root_event_base, -1, BEV_OPT_CLOSE_ON_FREE)) == NULL)
	{
		slog_error("bufferevent_socket_new error");
		goto end;
	}
	bufferevent_setcb(server_bufferevent, NULL, ipm_client_tunnel_server_bufferevent_data_write_callback, ipm_client_tunnel_server_bufferevent_event_callback, this);
	if (bufferevent_enable(server_bufferevent, EV_WRITE) != 0)
	{
		slog_error("bufferevent_enable error");
		goto end;
	}

	if (bufferevent_socket_connect(server_bufferevent, (struct sockaddr*)&server_addr, server_addr_len) != 0)
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

bool ipm_client_tunnel::connect_to_from()
{
	bool ret = false;

	if ((from_bufferevent = bufferevent_socket_new(root_event_base, -1, BEV_OPT_CLOSE_ON_FREE)) == NULL)
	{
		slog_error("bufferevent_socket_new error");
		goto end;
	}
	bufferevent_setcb(from_bufferevent, NULL, NULL, ipm_client_tunnel_from_bufferevent_event_callback, this);

	if (bufferevent_socket_connect(from_bufferevent, (struct sockaddr*)&from_server_addr, from_server_addr_len) != 0)
	{
		slog_error("bufferevent_socket_connect error");
		goto end;
	}

	ret = true;
end:
	if (ret != true)
	{
		if (from_bufferevent)
		{
			bufferevent_free(from_bufferevent);
			from_bufferevent = NULL;
		}
	}
	return ret;
}

bool ipm_client_tunnel::send_penetrate(struct bufferevent* bev)
{
	bool ret = false;
	size_t sz_len = 12 * sizeof(char);
	char* ptr_penetrate = (char*)malloc(sz_len);

	if (ptr_penetrate == NULL)
		goto end;

	memset(ptr_penetrate, 0, sz_len);
	*(unsigned long long*)ptr_penetrate = util::htonllx(index);

	util::set_checksum(key.c_str(), (char*)ptr_penetrate, sz_len);
	if (bufferevent_write(bev, ptr_penetrate, sz_len) != 0)
	{
		slog_error("send_penetrate error");
		goto end;
	}

	ret = true;
end:
	if (ptr_penetrate) free(ptr_penetrate);
	return ret;
}

void ipm_client_tunnel_server_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx)
{
	if (ctx)	((ipm_client_tunnel*)ctx)->on_server_bufferevent_data_write_callback(bev);
}
void ipm_client_tunnel_server_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx)
{
	if (ctx)	((ipm_client_tunnel*)ctx)->on_server_bufferevent_event_callback(bev, what);
}
void ipm_client_tunnel_from_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx)
{
	if (ctx)	((ipm_client_tunnel*)ctx)->on_from_bufferevent_event_callback(bev, what);
}