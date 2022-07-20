#include "stdafx.h"

void ipm_client_tunnel_server_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx);
void ipm_client_tunnel_server_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx);
void ipm_client_tunnel_server_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx);
void ipm_client_tunnel_from_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx);
void ipm_client_tunnel_from_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx);
void ipm_client_tunnel_from_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx);

ipm_client_tunnel::ipm_client_tunnel(struct event_base* base, interface_ipm_client_tunnel* ptr_interface_p, unsigned long long index_u) : root_event_base(base), ptr_interface(ptr_interface_p), index(index_u)
{
	reset();
}

bool ipm_client_tunnel::init(struct sockaddr_storage& server_addr_ss, unsigned int server_addr_len_u, struct sockaddr_storage& from_server_addr_ss, unsigned int from_server_addr_len_u)
{
	bool ret = false;

	server_addr = server_addr_ss;
	server_addr_len = server_addr_len_u;
	from_server_addr = from_server_addr_ss;
	from_server_addr_len = from_server_addr_len_u;

	if (connect_to_server() != true)
	{
		slog_debug("connect_to_server error");
		goto end;
	}

	server_state = SERVER_STATE::CONNECTING;

	if (connect_to_from() != true)
	{
		slog_debug("connect_to_from error");
		goto end;
	}

	from_state = FROM_STATE::CONNECTING;

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
	bool ret = false;

	if (server_bufferevent)
		bufferevent_free(server_bufferevent);

	if (from_bufferevent)
		bufferevent_free(from_bufferevent);

	is_state_init = false;

	ret = true;
//end:
	return ret;
}

void ipm_client_tunnel::reset()
{
	server_addr_len = 0;
	from_server_addr_len = 0;
	from_state = FROM_STATE::IDLE;
	server_state = SERVER_STATE::IDLE;
	server_bufferevent = NULL;
	from_bufferevent = NULL;
}

void ipm_client_tunnel::on_fail()
{
	slog_debug("client_tunnel on_fail %llu", index);
	if (ptr_interface)
		ptr_interface->on_interface_ipm_tunnel_client_fail(index);
}

void ipm_client_tunnel::on_server_bufferevent_data_read_callback(struct bufferevent* bev)
{
	bool ret = false;

#if VERBOSE_DEBUG
	slog_debug("tunnel %llu server recv data", (unsigned long long)index);
#endif

	if (from_state == FROM_STATE::RUNNING)
	{
		struct evbuffer* input = bufferevent_get_input(bev);

		if (bufferevent_write_buffer(from_bufferevent, input) != 0)
		{
			slog_debug("bufferevent_write_buffer error");
			goto end;
		}
	}

	ret = true;
end:
	if (ret != true)
		on_fail();
	return;
}

void ipm_client_tunnel::on_server_bufferevent_data_write_callback(struct bufferevent* bev)
{

}

void ipm_client_tunnel::on_server_bufferevent_event_callback(struct bufferevent* bev, short flag)
{
	slog_debug("tunnel server event %llu, flag = %u", index, (unsigned int)flag);

	if (flag & BEV_EVENT_READING) {
		slog_debug("server BEV_EVENT_READING error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_WRITING) {
		slog_debug("server BEV_EVENT_WRITING error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_ERROR) {
		slog_debug("server BEV_EVENT_ERROR error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_TIMEOUT) {
		slog_debug("server BEV_EVENT_TIMEOUT error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_EOF) {
		slog_debug("server BEV_EVENT_EOF error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_CONNECTED) {
		// ·¢ËÍ
		slog_debug("server BEV_EVENT_CONNECTED");
		server_state = SERVER_STATE::RUNNING;
		if (send_penetrate(bev) != true)
		{
			slog_debug("send_penetrate error");
			on_fail();
			return;
		}
		if (from_state == FROM_STATE::RUNNING)
		{
			slog_debug("flush_from_data");
			if(flush_from_data() != true)
			{
				slog_debug("flush_from_data error");
				on_fail();
				return;
			}
		}
	}
}

void ipm_client_tunnel::on_from_bufferevent_data_read_callback(struct bufferevent* bev)
{
#if VERBOSE_DEBUG
	slog_debug("tunnel %llu from recv data", (unsigned long long)index);
#endif
	if (server_state == SERVER_STATE::RUNNING)
	{
		struct evbuffer* input = bufferevent_get_input(bev);

		if (bufferevent_write_buffer(server_bufferevent, input) != 0)
		{
			slog_error("bufferevent_write_buffer error");
			on_fail();
			return;
		}
	}
}

void ipm_client_tunnel::on_from_bufferevent_data_write_callback(struct bufferevent* bev)
{

}

void ipm_client_tunnel::on_from_bufferevent_event_callback(struct bufferevent* bev, short flag)
{
	slog_debug("tunnel from event %llu, flag = %u", index, (unsigned int)flag);
	if (flag & BEV_EVENT_READING) {
		slog_error("from BEV_EVENT_READING error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_WRITING) {
		slog_error("from BEV_EVENT_WRITING error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_ERROR) {
		slog_error("from BEV_EVENT_ERROR error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_TIMEOUT) {
		slog_error("from BEV_EVENT_TIMEOUT error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_EOF) {
		slog_error("from BEV_EVENT_EOF error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_CONNECTED) {
		slog_info("from BEV_EVENT_CONNECTED, done");
		from_state = FROM_STATE::RUNNING;
		if (server_state == SERVER_STATE::RUNNING)
		{
			slog_info("flush_server_data");
			if (flush_server_data() != true)
			{
				slog_error("flush_server_data error");
				on_fail();
				return;
			}
		}
	}
}

bool ipm_client_tunnel::connect_to_server()
{
	bool ret = false;

	if ((server_bufferevent = bufferevent_socket_new(root_event_base, -1, BEV_OPT_CLOSE_ON_FREE)) == NULL)
	{
		slog_error("bufferevent_socket_new error");
		goto end;
	}
	bufferevent_setcb(server_bufferevent, ipm_client_tunnel_server_bufferevent_data_read_callback, ipm_client_tunnel_server_bufferevent_data_write_callback, ipm_client_tunnel_server_bufferevent_event_callback, this);
	if (bufferevent_enable(server_bufferevent, EV_READ | EV_WRITE) != 0)
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
	bufferevent_setcb(from_bufferevent, ipm_client_tunnel_from_bufferevent_data_read_callback, ipm_client_tunnel_from_bufferevent_data_write_callback, ipm_client_tunnel_from_bufferevent_event_callback, this);
	if (bufferevent_enable(from_bufferevent, EV_READ | EV_WRITE) != 0)
	{
		slog_error("bufferevent_enable error");
		goto end;
	}

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
	*(unsigned long long*)ptr_penetrate = util::htonll(index);

	util::set_checksum((char*)ptr_penetrate, sz_len);
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

bool ipm_client_tunnel::flush_server_data()
{
	bool ret = false;
	struct evbuffer* input = bufferevent_get_input(server_bufferevent);
	size_t length = evbuffer_get_length(input);

	if (length == 0)
	{
		ret = true;
		goto end;
	}

	if (bufferevent_write_buffer(from_bufferevent, input) != 0)
		goto end;

	ret = true;
end:
	return ret;
}

bool ipm_client_tunnel::flush_from_data()
{
	bool ret = false;
	struct evbuffer* input = bufferevent_get_input(from_bufferevent);
	size_t length = evbuffer_get_length(input);

	if (length == 0)
	{
		ret = true;
		goto end;
	}

	if (bufferevent_write_buffer(server_bufferevent, input) != 0)
		goto end;

	ret = true;
end:
	return ret;
}

void ipm_client_tunnel_server_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx)
{
	if (ctx)	((ipm_client_tunnel*)ctx)->on_server_bufferevent_data_read_callback(bev);
}
void ipm_client_tunnel_server_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx)
{
	if (ctx)	((ipm_client_tunnel*)ctx)->on_server_bufferevent_data_write_callback(bev);
}
void ipm_client_tunnel_server_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx)
{
	if (ctx)	((ipm_client_tunnel*)ctx)->on_server_bufferevent_event_callback(bev, what);
}
void ipm_client_tunnel_from_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx)
{
	if (ctx)	((ipm_client_tunnel*)ctx)->on_from_bufferevent_data_read_callback(bev);
}
void ipm_client_tunnel_from_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx)
{
	if (ctx)	((ipm_client_tunnel*)ctx)->on_from_bufferevent_data_write_callback(bev);
}
void ipm_client_tunnel_from_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx)
{
	if (ctx)	((ipm_client_tunnel*)ctx)->on_from_bufferevent_event_callback(bev, what);
}
