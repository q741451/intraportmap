#include "stdafx.h"

void ipm_client_tunnel_client_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx);
void ipm_client_tunnel_client_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx);
void ipm_client_tunnel_client_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx);
void ipm_client_tunnel_to_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx);
void ipm_client_tunnel_to_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx);
void ipm_client_tunnel_to_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx);

ipm_server_tunnel::ipm_server_tunnel(struct event_base* base, interface_ipm_server_tunnel* ptr_interface_p, evutil_socket_t to_fd_u) : root_event_base(base), ptr_interface(ptr_interface_p), to_fd(to_fd_u)
{
	reset();
}

bool ipm_server_tunnel::init(struct bufferevent* to_bev)
{
	bool ret = false;

	to_bufferevent = to_bev;

	bufferevent_setcb(to_bufferevent, ipm_client_tunnel_to_bufferevent_data_read_callback, ipm_client_tunnel_to_bufferevent_data_write_callback, ipm_client_tunnel_to_bufferevent_event_callback, this);
	if (bufferevent_enable(to_bufferevent, EV_READ | EV_WRITE) != 0)
	{
		slog_debug("bufferevent_enable error");
		goto end;
	}

	ret = true;
end:
	if (ret == true)
		is_state_init = true;
	else
	{
		to_bufferevent = NULL; // 挂了还给上层
		exit();
		reset();
	}
	return ret;
}

bool ipm_server_tunnel::is_init()
{
	return is_state_init;
}

bool ipm_server_tunnel::exit()
{
	bool ret = false;


	if (client_bufferevent)
		bufferevent_free(client_bufferevent);

	if (to_bufferevent)
		bufferevent_free(to_bufferevent);

	is_state_init = false;
	ret = true;
//end:
	return ret;
}

void ipm_server_tunnel::reset()
{
	is_state_init = false;
	client_bufferevent = NULL;
	to_bufferevent = NULL;
}

bool ipm_server_tunnel::client_connected(struct bufferevent* bev)
{
	bool ret = false;

	client_bufferevent = bev;

	bufferevent_setcb(client_bufferevent, ipm_client_tunnel_client_bufferevent_data_read_callback, ipm_client_tunnel_client_bufferevent_data_write_callback, ipm_client_tunnel_client_bufferevent_event_callback, this);
	if (bufferevent_enable(client_bufferevent, EV_READ | EV_WRITE) != 0)
	{
		slog_debug("bufferevent_enable error");
		goto end;
	}

	if (flush_client_data() != true)
	{
		slog_debug("flush_client_data error");
		goto end;
	}

	if (flush_to_data() != true)
	{
		slog_debug("flush_to_data error");
		goto end;
	}

	ret = true;
end:
	if (ret != true)
	{
		client_bufferevent = NULL; // 挂了还给上层
	}
	return ret;
}

void ipm_server_tunnel::on_fail()
{
	slog_debug("tunnel on_fail %llu", (unsigned long long)to_fd);
	if (ptr_interface)
		ptr_interface->on_interface_ipm_server_tunnel_fail(to_fd);
}

void ipm_server_tunnel::on_client_bufferevent_data_read_callback(struct bufferevent* bev)
{
	bool ret = false;

#if VERBOSE_DEBUG
	slog_debug("tunnel %llu client recv data", (unsigned long long)to_fd);
#endif

	if (to_bufferevent)
	{
		struct evbuffer* input = bufferevent_get_input(bev);

		if (bufferevent_write_buffer(to_bufferevent, input) != 0)
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

void ipm_server_tunnel::on_client_bufferevent_data_write_callback(struct bufferevent* bev)
{

}

void ipm_server_tunnel::on_client_bufferevent_event_callback(struct bufferevent* bev, short flag)
{
	slog_debug("tunnel client event %llu, flag = %u", (unsigned long long)to_fd, (unsigned int)flag);
	if (flag & BEV_EVENT_READING) {
		slog_debug("client BEV_EVENT_READING error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_WRITING) {
		slog_debug("client BEV_EVENT_WRITING error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_ERROR) {
		slog_debug("client BEV_EVENT_ERROR error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_TIMEOUT) {
		slog_debug("client BEV_EVENT_TIMEOUT error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_EOF) {
		slog_debug("client BEV_EVENT_EOF error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_CONNECTED) {
		// 不应该来这里
		slog_debug("client BEV_EVENT_CONNECTED ?");
		on_fail();
		return;
	}
}

void ipm_server_tunnel::on_to_bufferevent_data_read_callback(struct bufferevent* bev)
{
#if VERBOSE_DEBUG
	slog_debug("tunnel %llu to recv data", (unsigned long long)to_fd);
#endif
	if (client_bufferevent)
	{
		struct evbuffer* input = bufferevent_get_input(bev);

		if (bufferevent_write_buffer(client_bufferevent, input) != 0)
		{
			slog_debug("bufferevent_write_buffer error");
			on_fail();
			return;
		}
	}
}

void ipm_server_tunnel::on_to_bufferevent_data_write_callback(struct bufferevent* bev)
{

}

void ipm_server_tunnel::on_to_bufferevent_event_callback(struct bufferevent* bev, short flag)
{
	slog_debug("tunnel to event %llu, flag = %u", (unsigned long long)to_fd, (unsigned int)flag);
	if (flag & BEV_EVENT_READING) {
		slog_debug("to BEV_EVENT_READING error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_WRITING) {
		slog_debug("to BEV_EVENT_WRITING error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_ERROR) {
		slog_debug("to BEV_EVENT_ERROR error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_TIMEOUT) {
		slog_debug("to BEV_EVENT_TIMEOUT error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_EOF) {
		slog_debug("to BEV_EVENT_EOF error");
		on_fail();
		return;
	}

	if (flag & BEV_EVENT_CONNECTED) {
		// 不应该来这里
		slog_debug("to BEV_EVENT_CONNECTED ?");
		on_fail();
		return;
	}
}

bool ipm_server_tunnel::flush_client_data()
{
	bool ret = false;
	struct evbuffer* input = bufferevent_get_input(client_bufferevent);
	size_t length = evbuffer_get_length(input);

	if (length == 0)
	{
		ret = true;
		goto end;
	}

	if (bufferevent_write_buffer(to_bufferevent, input) != 0)
		goto end;

	ret = true;
end:
	return ret;
}

bool ipm_server_tunnel::flush_to_data()
{
	bool ret = false;
	struct evbuffer* input = bufferevent_get_input(to_bufferevent);
	size_t length = evbuffer_get_length(input);

	if (length == 0)
	{
		ret = true;
		goto end;
	}

	if (bufferevent_write_buffer(client_bufferevent, input) != 0)
		goto end;

	ret = true;
end:
	return ret;
}

void ipm_client_tunnel_client_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx)
{
	if (ctx)	((ipm_server_tunnel*)ctx)->on_client_bufferevent_data_read_callback(bev);
}
void ipm_client_tunnel_client_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx)
{
	if (ctx)	((ipm_server_tunnel*)ctx)->on_client_bufferevent_data_write_callback(bev);
}
void ipm_client_tunnel_client_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx)
{
	if (ctx)	((ipm_server_tunnel*)ctx)->on_client_bufferevent_event_callback(bev, what);
}
void ipm_client_tunnel_to_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx)
{
	if (ctx)	((ipm_server_tunnel*)ctx)->on_to_bufferevent_data_read_callback(bev);
}
void ipm_client_tunnel_to_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx)
{
	if (ctx)	((ipm_server_tunnel*)ctx)->on_to_bufferevent_data_write_callback(bev);
}
void ipm_client_tunnel_to_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx)
{
	if (ctx)	((ipm_server_tunnel*)ctx)->on_to_bufferevent_event_callback(bev, what);
}
