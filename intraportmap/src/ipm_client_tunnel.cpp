#include "stdafx.h"

void ipm_client_tunnel_server_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx);
void ipm_client_tunnel_server_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx);
void ipm_client_tunnel_server_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx);
void ipm_client_tunnel_from_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx);
void ipm_client_tunnel_from_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx);
void ipm_client_tunnel_from_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx);

ipm_client_tunnel::ipm_client_tunnel(struct event_base* base, interface_ipm_client_tunnel* ptr_interface_p, unsigned long long index_u) : ptr_interface(ptr_interface_p), root_event_base(base), index(index_u)
{
	reset();
}

bool ipm_client_tunnel::init(struct sockaddr_storage& server_addr_ss, unsigned int server_addr_len_u, struct sockaddr_storage& from_server_addr_ss, unsigned int from_server_addr_len_u, size_t max_buffer_sz)
{
	bool ret = false;

	server_addr = server_addr_ss;
	server_addr_len = server_addr_len_u;
	from_server_addr = from_server_addr_ss;
	from_server_addr_len = from_server_addr_len_u;
	max_buffer = max_buffer_sz;

	if (connect_to_server() != true)
	{
		slog_debug("connect_to_server error");
		goto end;
	}

	server_state = CONN_STATE::CONNECTING;

	if (connect_to_from() != true)
	{
		slog_debug("connect_to_from error");
		goto end;
	}

	from_state = CONN_STATE::CONNECTING;

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
	max_buffer = DEF_MAX_BUFFER;
	from_state = CONN_STATE::IDLE;
	server_state = CONN_STATE::IDLE;
	from_flush_state = FLUSH_STATE::NORMAL;
	server_flush_state = FLUSH_STATE::NORMAL;
	server_bufferevent = NULL;
	from_bufferevent = NULL;
}

void ipm_client_tunnel::on_fail()
{
	slog_debug("client_tunnel on_fail %llu", index);
	if (ptr_interface)
		ptr_interface->on_interface_ipm_tunnel_client_fail(index);
}

void ipm_client_tunnel::on_server_fail()
{
	slog_debug("client_tunnel on_server_fail %llu", index);

	// 第二次调用不处理
	if (server_state == CONN_STATE::BROKEN)
		return;

	server_state = CONN_STATE::BROKEN;

	// 对方状态挂了直接失败
	if (from_state != CONN_STATE::RUNNING)
		return on_fail();

	// 清除后退出
	{
		struct evbuffer* input = bufferevent_get_input(server_bufferevent);
		struct evbuffer* output = bufferevent_get_output(from_bufferevent);

		if (evbuffer_add_buffer(output, input) != 0)
		{
			slog_debug("evbuffer_add_buffer error");
			return on_fail();
		}

		if (evbuffer_get_length(bufferevent_get_output(from_bufferevent)) == 0)
		{
			// 没有任务
			slog_debug("close no flush data");
			return on_fail();
		}

		// 关闭读，写到0为止
		bufferevent_setwatermark(from_bufferevent, EV_WRITE, 0, 0);
		if (bufferevent_disable(from_bufferevent, EV_READ) != 0)
		{
			slog_debug("bufferevent_disable fail");
			return on_from_fail();
		}

		// 关闭server_bufferevent，防止回调莫名其妙的事件
		if (server_bufferevent)
		{
			bufferevent_free(server_bufferevent);
			server_bufferevent = NULL;
		}

		from_flush_state = FLUSH_STATE::FLUSH_AND_EXIT;
	}
}

void ipm_client_tunnel::on_from_fail()
{
	slog_debug("client_tunnel on_from_fail %llu", index);

	// 第二次调用不处理
	if (from_state == CONN_STATE::BROKEN)
		return;

	from_state = CONN_STATE::BROKEN;

	if (server_state != CONN_STATE::RUNNING)
		return on_fail();

	// 清除后退出
	{
		struct evbuffer* input = bufferevent_get_input(from_bufferevent);
		struct evbuffer* output = bufferevent_get_output(server_bufferevent);

		if (evbuffer_add_buffer(output, input) != 0)
		{
			slog_debug("evbuffer_add_buffer error");
			return on_fail();
		}

		if (evbuffer_get_length(bufferevent_get_output(server_bufferevent)) == 0)
		{
			// 没有任务
			slog_debug("close no flush data");
			return on_fail();
		}

		// 关闭读，写到0为止
		bufferevent_setwatermark(server_bufferevent, EV_WRITE, 0, 0);
		if (bufferevent_disable(server_bufferevent, EV_READ) != 0)
		{
			slog_debug("bufferevent_disable fail");
			return on_server_fail();
		}

		// 关闭from_bufferevent，防止回调莫名其妙的事件
		if (from_bufferevent)
		{
			bufferevent_free(from_bufferevent);
			from_bufferevent = NULL;
		}

		server_flush_state = FLUSH_STATE::FLUSH_AND_EXIT;
	}
}

void ipm_client_tunnel::on_server_bufferevent_data_read_callback(struct bufferevent* bev)
{
#if VERBOSE_DEBUG
	slog_debug("tunnel %llu server recv data", (unsigned long long)index);
#endif

	{
		struct evbuffer* input = bufferevent_get_input(bev);
		struct evbuffer* output = bufferevent_get_output(from_bufferevent);

		if (evbuffer_add_buffer(output, input) != 0)
		{
			slog_debug("evbuffer_add_buffer error");
			return on_fail();
		}

		// 超标了，就不读了，等写一半后再说
		if (evbuffer_get_length(output) >= max_buffer)
		{
			bufferevent_setwatermark(from_bufferevent, EV_WRITE, max_buffer / 2, max_buffer);
			if (bufferevent_disable(bev, EV_READ) != 0)
			{
				slog_debug("bufferevent_disable error");
				return on_server_fail();
			}
			from_flush_state = FLUSH_STATE::FLUSH_AND_REREAD;
		}
	}
}

void ipm_client_tunnel::on_server_bufferevent_data_write_callback(struct bufferevent* bev)
{
	if (server_flush_state == FLUSH_STATE::FLUSH_AND_REREAD)
	{
		bufferevent_setwatermark(bev, EV_WRITE, 0, 0);
		if (bufferevent_enable(from_bufferevent, EV_READ) != 0)
		{
			slog_debug("bufferevent_enable fail");
			return on_from_fail();
		}
		server_flush_state = FLUSH_STATE::NORMAL;
	}
	else if (server_flush_state == FLUSH_STATE::FLUSH_AND_EXIT)
	{
		return on_fail();
	}
}

void ipm_client_tunnel::on_server_bufferevent_event_callback(struct bufferevent* bev, short flag)
{
	slog_debug("tunnel server event %llu, flag = %u", index, (unsigned int)flag);

	if (flag & BEV_EVENT_READING) {
		slog_debug("server BEV_EVENT_READING error");
	}

	if (flag & BEV_EVENT_WRITING) {
		slog_debug("server BEV_EVENT_WRITING error");
	}

	if (flag & BEV_EVENT_ERROR) {
		slog_debug("server BEV_EVENT_ERROR error");
		return on_server_fail();
	}

	if (flag & BEV_EVENT_TIMEOUT) {
		slog_debug("server BEV_EVENT_TIMEOUT error");
		return on_server_fail();
	}

	if (flag & BEV_EVENT_EOF) {
		slog_debug("server BEV_EVENT_EOF error");
		return on_server_fail();
	}

	if (flag & BEV_EVENT_CONNECTED) {
		// 发送
		slog_debug("server BEV_EVENT_CONNECTED");
		server_state = CONN_STATE::RUNNING;
		if (send_penetrate(bev) != true)
		{
			slog_debug("send_penetrate error");
			return on_server_fail();
		}
	}
}

void ipm_client_tunnel::on_from_bufferevent_data_read_callback(struct bufferevent* bev)
{

#if VERBOSE_DEBUG
	slog_debug("tunnel %llu from recv data", (unsigned long long)index);
#endif

	{
		struct evbuffer* input = bufferevent_get_input(bev);
		struct evbuffer* output = bufferevent_get_output(server_bufferevent);

		if (evbuffer_add_buffer(output, input) != 0)
		{
			slog_debug("evbuffer_add_buffer error");
			return on_fail();
		}

		// 超标了，就不读了，等写一半后再说
		if (evbuffer_get_length(output) >= max_buffer)
		{
			bufferevent_setwatermark(server_bufferevent, EV_WRITE, max_buffer / 2, max_buffer);
			if (bufferevent_disable(bev, EV_READ) != 0)
			{
				slog_debug("bufferevent_disable error");
				return on_from_fail();
			}
			server_flush_state = FLUSH_STATE::FLUSH_AND_REREAD;
		}
	}
}

void ipm_client_tunnel::on_from_bufferevent_data_write_callback(struct bufferevent* bev)
{
	if (from_flush_state == FLUSH_STATE::FLUSH_AND_REREAD)
	{
		bufferevent_setwatermark(bev, EV_WRITE, 0, 0);
		if (bufferevent_enable(server_bufferevent, EV_READ) != 0)
		{
			slog_debug("bufferevent_enable fail");
			return on_server_fail();
		}
		server_flush_state = FLUSH_STATE::NORMAL;
	}
	else if (from_flush_state == FLUSH_STATE::FLUSH_AND_EXIT)
	{
		return on_fail();
	}
}

void ipm_client_tunnel::on_from_bufferevent_event_callback(struct bufferevent* bev, short flag)
{
	slog_debug("tunnel from event %llu, flag = %u", index, (unsigned int)flag);

	if (flag & BEV_EVENT_READING) {
		slog_error("from BEV_EVENT_READING error");
	}

	if (flag & BEV_EVENT_WRITING) {
		slog_error("from BEV_EVENT_WRITING error");
	}

	if (flag & BEV_EVENT_ERROR) {
		slog_error("from BEV_EVENT_ERROR error");
		return on_from_fail();
	}

	if (flag & BEV_EVENT_TIMEOUT) {
		slog_error("from BEV_EVENT_TIMEOUT error");
		return on_from_fail();
	}

	if (flag & BEV_EVENT_EOF) {
		slog_error("from BEV_EVENT_EOF error");
		return on_from_fail();
	}

	if (flag & BEV_EVENT_CONNECTED) {
		slog_info("from BEV_EVENT_CONNECTED, done");
		from_state = CONN_STATE::RUNNING;
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
