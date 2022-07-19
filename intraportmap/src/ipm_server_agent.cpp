#include "stdafx.h"

void ipm_server_agent_client_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx);
void ipm_server_agent_client_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx);
void ipm_server_agent_client_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx);

void ipm_server_agent_listener_callback(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr* sa, int socklen, void* user_data);

ipm_server_agent::ipm_server_agent(struct event_base* base, interface_ipm_server_agent* ptr_interface_p) : root_event_base(base), ptr_interface(ptr_interface_p)
{
	reset();
}

bool ipm_server_agent::init(struct sockaddr_storage& agent_addr_ss, unsigned int agent_addr_len_u, struct bufferevent* client_bev)
{
	bool ret = false;

	client_bufferevent = client_bev;

	bufferevent_setcb(client_bufferevent, NULL, ipm_server_agent_client_bufferevent_data_write_callback, ipm_server_agent_client_bufferevent_event_callback, this);

	// 只写不读
	if (bufferevent_enable(client_bufferevent, EV_WRITE) != 0)
	{
		slog_error("bufferevent_enable error");
		goto end;
	}

	agent_addr = agent_addr_ss;
	agent_addr_len = agent_addr_len_u;

	if (start_listener((struct sockaddr*)&agent_addr, agent_addr_len) != true)
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
		client_bufferevent = NULL; // 挂了还给上层
		exit();
		reset();
	}
	return ret;
}

bool ipm_server_agent::is_init()
{
	return is_state_init;
}

bool ipm_server_agent::exit()
{
	bool ret = false;

	if (client_bufferevent)
		bufferevent_free(client_bufferevent);

	if (listener)
		evconnlistener_free(listener);

	is_state_init = false;
	ret = true;
//end:
	return ret;
}

void ipm_server_agent::reset()
{
	is_state_init = false;
	agent_addr_len = 0;
	listener = NULL;
	client_bufferevent = NULL;
}

void ipm_server_agent::on_fail()
{
	slog_info("on_fail");
	if (ptr_interface)
		ptr_interface->on_interface_ipm_server_agent_fail(client_bufferevent);
}

void ipm_server_agent::on_client_bufferevent_data_read_callback(struct bufferevent* bev)
{

}

void ipm_server_agent::on_client_bufferevent_data_write_callback(struct bufferevent* bev)
{

}

void ipm_server_agent::on_client_bufferevent_event_callback(struct bufferevent* bev, short flag)
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
		// 应该不会来这个
		slog_error("BEV_EVENT_CONNECTED ?");
		on_fail();
	}
}

void ipm_server_agent::on_listener(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr* sa, int socklen)
{
	bool ret = false;
	struct bufferevent* buff_bev = NULL;

	if (fd == -1)
	{
		slog_error("fd == -1");
		goto end;
	}

	if ((buff_bev = bufferevent_socket_new(root_event_base, fd, BEV_OPT_CLOSE_ON_FREE)) == NULL)
	{
		slog_error("bufferevent_socket_new error");
		goto end;
	}

	// 通知客户端
	if (send_penetrate(client_bufferevent, (unsigned long long)fd) != true)
	{
		slog_error("send_penetrate error");
		goto end;
	}

	// OK 通知上层
	if (!ptr_interface)
	{
		slog_error("ptr_interface empty");
		goto end;
	}

	if (ptr_interface->on_interface_ipm_server_agent_new_fd_tunnel(client_bufferevent, buff_bev, fd) != true)
	{
		// 此时未托管
		slog_error("on_interface_ipm_server_agent_new_fd_tunnel error");
		goto end;
	}

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

bool ipm_server_agent::start_listener(const struct sockaddr* addr, int addr_length)
{
	bool ret = false;

	if ((listener = evconnlistener_new_bind(root_event_base, ipm_server_agent_listener_callback, this, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1, (struct sockaddr*)addr, addr_length)) == NULL)
		goto end;

	ret = true;
end:
	return ret;
}

bool ipm_server_agent::send_penetrate(struct bufferevent* bev, unsigned long long index)
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

void ipm_server_agent_client_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx)
{
	if (ctx)	((ipm_server_agent*)ctx)->on_client_bufferevent_data_read_callback(bev);
}
void ipm_server_agent_client_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx)
{
	if (ctx)	((ipm_server_agent*)ctx)->on_client_bufferevent_data_write_callback(bev);
}
void ipm_server_agent_client_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx)
{
	if (ctx)	((ipm_server_agent*)ctx)->on_client_bufferevent_event_callback(bev, what);
}

void ipm_server_agent_listener_callback(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr* sa, int socklen, void* user_data)
{
	if (user_data)	((ipm_server_agent*)user_data)->on_listener(listener, fd, sa, socklen);
}
