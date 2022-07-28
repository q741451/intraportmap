#include "stdafx.h"

void ipm_client_tunnel_client_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx);
void ipm_client_tunnel_client_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx);
void ipm_client_tunnel_client_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx);
void ipm_client_tunnel_to_bufferevent_data_read_callback(struct bufferevent* bev, void* ctx);
void ipm_client_tunnel_to_bufferevent_data_write_callback(struct bufferevent* bev, void* ctx);
void ipm_client_tunnel_to_bufferevent_event_callback(struct bufferevent* bev, short what, void* ctx);

ipm_server_tunnel::ipm_server_tunnel(struct event_base* base, interface_ipm_server_tunnel* ptr_interface_p, evutil_socket_t to_fd_u) : ptr_interface(ptr_interface_p), to_fd(to_fd_u), root_event_base(base)
{
	reset();
}

bool ipm_server_tunnel::init(struct bufferevent* to_bev, size_t max_buffer_sz)
{
	bool ret = false;

	max_buffer = max_buffer_sz;
	to_bufferevent = to_bev;

	bufferevent_setcb(to_bufferevent, ipm_client_tunnel_to_bufferevent_data_read_callback, ipm_client_tunnel_to_bufferevent_data_write_callback, ipm_client_tunnel_to_bufferevent_event_callback, this);
	if (bufferevent_enable(to_bufferevent, EV_READ | EV_WRITE) != 0)
	{
		slog_debug("bufferevent_enable error");
		goto end;
	}

	to_state = CONN_STATE::RUNNING;
	to_flush_state = FLUSH_STATE::NORMAL;	// 此时都无数据，所以NORMAL

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
	to_state = CONN_STATE::IDLE;
	to_flush_state = FLUSH_STATE::NORMAL;
	client_state = CONN_STATE::IDLE;
	client_flush_state = FLUSH_STATE::NORMAL;
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

	// to_bufferevent中途可能缓存满了，关闭读取，恢复
	if (bufferevent_enable(to_bufferevent, EV_READ | EV_WRITE) != 0)
	{
		slog_debug("bufferevent_enable error");
		goto end;
	}

	client_state = CONN_STATE::RUNNING;
	client_flush_state = FLUSH_STATE::NORMAL;

	if (flush_client_data(false) != true)
	{
		slog_debug("flush_client_data error");
		goto end;
	}

	if (flush_to_data(false) != true)
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

void ipm_server_tunnel::on_client_fail()
{
	slog_debug("server_tunnel on_client_fail %llu", (unsigned long long)to_fd);

	// 第二次调用不处理
	if (client_state == CONN_STATE::BROKEN)
		return;

	client_state = CONN_STATE::BROKEN;

	// 对方状态挂了直接失败
	if (to_state != CONN_STATE::RUNNING)
		return on_fail();

	// 清除后退出
	{
		struct evbuffer* input = bufferevent_get_input(client_bufferevent);
		struct evbuffer* output = bufferevent_get_output(to_bufferevent);

		if (evbuffer_add_buffer(output, input) != 0)
		{
			slog_debug("evbuffer_add_buffer error");
			return on_fail();
		}

		if (evbuffer_get_length(bufferevent_get_output(to_bufferevent)) == 0)
		{
			// 没有任务
			slog_debug("close no flush data");
			return on_fail();
		}

		// 关闭读，写到0为止
		bufferevent_setwatermark(to_bufferevent, EV_WRITE, 0, 0);
		if (bufferevent_disable(to_bufferevent, EV_READ) != 0)
		{
			slog_debug("bufferevent_disable fail");
			return on_to_fail();
		}

		// 关闭server_bufferevent，防止回调莫名其妙的事件
		if (client_bufferevent)
		{
			bufferevent_free(client_bufferevent);
			client_bufferevent = NULL;
		}

		to_flush_state = FLUSH_STATE::FLUSH_AND_EXIT;
	}
}

void ipm_server_tunnel::on_to_fail()
{
	slog_debug("server_tunnel on_to_fail %llu", (unsigned long long)to_fd);

	// 第二次调用不处理
	if (to_state == CONN_STATE::BROKEN)
		return;

	to_state = CONN_STATE::BROKEN;

	if (client_state != CONN_STATE::RUNNING)
		return on_fail();

	// 清除后退出
	{
		struct evbuffer* input = bufferevent_get_input(to_bufferevent);
		struct evbuffer* output = bufferevent_get_output(client_bufferevent);

		if (evbuffer_add_buffer(output, input) != 0)
		{
			slog_debug("evbuffer_add_buffer error");
			return on_fail();
		}

		if (evbuffer_get_length(bufferevent_get_output(client_bufferevent)) == 0)
		{
			// 没有任务
			slog_debug("close no flush data");
			return on_fail();
		}

		// 关闭读，写到0为止
		bufferevent_setwatermark(client_bufferevent, EV_WRITE, 0, 0);
		if (bufferevent_disable(client_bufferevent, EV_READ) != 0)
		{
			slog_debug("bufferevent_disable fail");
			return on_to_fail();
		}

		// 关闭to_bufferevent，防止回调莫名其妙的事件
		if (to_bufferevent)
		{
			bufferevent_free(to_bufferevent);
			to_bufferevent = NULL;
		}

		client_flush_state = FLUSH_STATE::FLUSH_AND_EXIT;
	}
}

void ipm_server_tunnel::on_client_bufferevent_data_read_callback(struct bufferevent* bev)
{
#if VERBOSE_DEBUG
	slog_debug("tunnel %llu client recv data", (unsigned long long)to_fd);
#endif

	flush_client_data(true);
}

void ipm_server_tunnel::on_client_bufferevent_data_write_callback(struct bufferevent* bev)
{
	if (client_flush_state == FLUSH_STATE::FLUSH_AND_REREAD)
	{
		bufferevent_setwatermark(bev, EV_WRITE, 0, 0);
		if (bufferevent_enable(to_bufferevent, EV_READ) != 0)
		{
			slog_debug("bufferevent_enable fail");
			return on_to_fail();
		}
		client_flush_state = FLUSH_STATE::NORMAL;
	}
	else if (client_flush_state == FLUSH_STATE::FLUSH_AND_EXIT)
	{
		return on_fail();
	}
}

void ipm_server_tunnel::on_client_bufferevent_event_callback(struct bufferevent* bev, short flag)
{
	slog_debug("tunnel client event %llu, flag = %u", (unsigned long long)to_fd, (unsigned int)flag);
	if (flag & BEV_EVENT_READING) {
		slog_debug("client BEV_EVENT_READING error");
	}

	if (flag & BEV_EVENT_WRITING) {
		slog_debug("client BEV_EVENT_WRITING error");
	}

	if (flag & BEV_EVENT_ERROR) {
		slog_debug("client BEV_EVENT_ERROR error");
		return on_client_fail();
	}

	if (flag & BEV_EVENT_TIMEOUT) {
		slog_debug("client BEV_EVENT_TIMEOUT error");
		return on_client_fail();
	}

	if (flag & BEV_EVENT_EOF) {
		slog_debug("client BEV_EVENT_EOF error");
		return on_client_fail();
	}

	if (flag & BEV_EVENT_CONNECTED) {
		// 不应该来这里
		slog_debug("client BEV_EVENT_CONNECTED ?");
		return on_fail();
	}
}

void ipm_server_tunnel::on_to_bufferevent_data_read_callback(struct bufferevent* bev)
{
#if VERBOSE_DEBUG
	slog_debug("tunnel %llu to recv data", (unsigned long long)to_fd);
#endif

	if (client_state == CONN_STATE::RUNNING)
	{
		flush_to_data();
		return;
	}
	else
	{
		struct evbuffer* input = bufferevent_get_input(bev);

		if (evbuffer_get_length(input) >= max_buffer / 2)
		{
			// 积压的数据太大了，先不读了，等flush后读
			if (bufferevent_disable(bev, EV_READ) != 0)
			{
				slog_debug("bufferevent_disable error");
				return on_fail();
			}
		}
	}
}

void ipm_server_tunnel::on_to_bufferevent_data_write_callback(struct bufferevent* bev)
{
	if (to_flush_state == FLUSH_STATE::FLUSH_AND_REREAD)
	{
		bufferevent_setwatermark(bev, EV_WRITE, 0, 0);
		if (bufferevent_enable(client_bufferevent, EV_READ) != 0)
		{
			slog_debug("bufferevent_enable fail");
			return on_client_fail();
		}
		client_flush_state = FLUSH_STATE::NORMAL;
	}
	else if (to_flush_state == FLUSH_STATE::FLUSH_AND_EXIT)
	{
		return on_fail();
	}
}

void ipm_server_tunnel::on_to_bufferevent_event_callback(struct bufferevent* bev, short flag)
{
	slog_debug("tunnel to event %llu, flag = %u", (unsigned long long)to_fd, (unsigned int)flag);
	if (flag & BEV_EVENT_READING) {
		slog_debug("to BEV_EVENT_READING error");
	}

	if (flag & BEV_EVENT_WRITING) {
		slog_debug("to BEV_EVENT_WRITING error");
	}

	if (flag & BEV_EVENT_ERROR) {
		slog_debug("to BEV_EVENT_ERROR error");
		return on_to_fail();
	}

	if (flag & BEV_EVENT_TIMEOUT) {
		slog_debug("to BEV_EVENT_TIMEOUT error");
		return on_to_fail();
	}

	if (flag & BEV_EVENT_EOF) {
		slog_debug("to BEV_EVENT_EOF error");
		return on_to_fail();
	}

	if (flag & BEV_EVENT_CONNECTED) {
		// 不应该来这里
		slog_debug("to BEV_EVENT_CONNECTED ?");
		return on_fail();
	}
}

bool ipm_server_tunnel::flush_client_data(bool is_event)
{
	struct evbuffer* input = bufferevent_get_input(client_bufferevent);
	struct evbuffer* output = bufferevent_get_output(to_bufferevent);
	bool ret = false;

	if (evbuffer_add_buffer(output, input) != 0)
	{
		slog_debug("evbuffer_add_buffer error");
		if (is_event)
			on_fail();
		goto end;
	}

	// 超标了，就不读了，等写一半后再说
	if (evbuffer_get_length(output) >= max_buffer)
	{
		bufferevent_setwatermark(to_bufferevent, EV_WRITE, max_buffer / 2, max_buffer);
		if (bufferevent_disable(client_bufferevent, EV_READ) != 0)
		{
			slog_debug("bufferevent_disable error");
			if (is_event)
				on_client_fail();
			goto end;
		}
		to_flush_state = FLUSH_STATE::FLUSH_AND_REREAD;
	}

	ret = true;
end:
	return ret;
}

bool ipm_server_tunnel::flush_to_data(bool is_event)
{
	struct evbuffer* input = bufferevent_get_input(to_bufferevent);
	struct evbuffer* output = bufferevent_get_output(client_bufferevent);
	bool ret = false;

	if (evbuffer_add_buffer(output, input) != 0)
	{
		slog_debug("evbuffer_add_buffer error");
		if (is_event)
			on_fail();
		goto end;
	}

	// 超标了，就不读了，等写一半后再说
	if (evbuffer_get_length(output) >= max_buffer)
	{
		bufferevent_setwatermark(client_bufferevent, EV_WRITE, max_buffer / 2, max_buffer);
		if (bufferevent_disable(to_bufferevent, EV_READ) != 0)
		{
			slog_debug("bufferevent_disable error");
			if (is_event)
				on_to_fail();
			goto end;
		}
		client_flush_state = FLUSH_STATE::FLUSH_AND_REREAD;
	}

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
