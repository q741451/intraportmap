#include "stdafx.h"

void ipm_server_tunnel_to_watch_callback(evutil_socket_t fd, short what, void* ctx);
void ipm_server_tunnel_join_timer_callback(evutil_socket_t fd, short what, void* ctx);

ipm_server_tunnel::ipm_server_tunnel(struct event_base* base, interface_ipm_server_tunnel* ptr_interface_p, unsigned long long index_u) : ptr_interface(ptr_interface_p), index(index_u), root_event_base(base), relay(base, this, index_u)
{
	reset();
}

bool ipm_server_tunnel::init(evutil_socket_t to_fd, size_t max_buffer_sz)
{
	bool ret = false;
	struct timeval tv = { IPM_JOIN_TIMEOUT, 0 };
	const struct timeval* timeout;
	int features = event_base_get_features(root_event_base);

	if (relay.init(max_buffer_sz) != true)
	{
		slog_debug("relay.init error");
		goto end;
	}

	// 所有隧道期限相同，用 common timeout：同时长的定时器挂在一条按到期先后排好的队列上，
	// 增删 O(1)，整条队列在堆里只占一个位置。已有同时长的队列就复用
	if ((timeout = event_base_init_common_timeout(root_event_base, &tv)) == NULL)
	{
		slog_error("event_base_init_common_timeout error");
		goto end;
	}

	if ((join_timer = evtimer_new(root_event_base, ipm_server_tunnel_join_timer_callback, this)) == NULL ||
		evtimer_add(join_timer, timeout) != 0)
	{
		slog_error("join_timer error");
		goto end;
	}

	// 边沿触发：登记时报一次可写，之后只有关闭或出错才报；访客再发数据不会唤醒
	if ((features & EV_FEATURE_EARLY_CLOSE) && (features & EV_FEATURE_ET))
	{
		if ((to_watch_event = event_new(root_event_base, to_fd, EV_WRITE | EV_CLOSED | EV_ET | EV_PERSIST, ipm_server_tunnel_to_watch_callback, this)) == NULL ||
			event_add(to_watch_event, NULL) != 0)
		{
			slog_error("to_watch_event error");
			goto end;
		}
	}

	// 最后一步：之前失败的话 to_fd 还归调用方
	if (relay.attach(SIDE_TO, to_fd) != true)
	{
		slog_debug("relay.attach to error");
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

bool ipm_server_tunnel::is_init()
{
	return is_state_init;
}

bool ipm_server_tunnel::exit()
{
	// 先删事件，fd 由 relay.exit 最后关
	if (to_watch_event)
		event_free(to_watch_event);

	if (join_timer)
		event_free(join_timer);

	if (client_bufferevent)
		bufferevent_free(client_bufferevent);

	relay.exit();
	is_state_init = false;
	return true;
}

void ipm_server_tunnel::reset()
{
	is_state_init = false;
	to_watch_event = NULL;
	join_timer = NULL;
	client_bufferevent = NULL;
	relay.reset();
}

bool ipm_server_tunnel::client_connected(struct bufferevent* bev)
{
	// 一条隧道只接一个客户端连接，重复的 join 不能顶掉已接上的那条
	if (relay.is_attached(SIDE_CLIENT))
	{
		slog_error("tunnel %llu already joined", index);
		return false;
	}

	// 握手后面已读到的数据一并接管
	if (relay.attach(SIDE_CLIENT, bev) != true)
	{
		// 挂了还给上层
		slog_debug("relay.attach client error");
		on_fail();
		return false;
	}
	client_bufferevent = bev;

	// 同一轮里已就绪、还没回调的也一并撤销。守候是边沿触发，
	// 必须先删掉，relay 才能在同一个 fd 上登记水平触发的事件
	if (to_watch_event)
		event_del(to_watch_event);
	evtimer_del(join_timer);

	if (relay.start() != true)
	{
		// bev 已归本类，随 exit 释放，不能再还给上层
		slog_debug("relay.start error");
		on_fail();
		return true;
	}

	return true;
}

void ipm_server_tunnel::on_interface_ipm_relay_close()
{
	return on_fail();
}

void ipm_server_tunnel::on_fail()
{
	slog_debug("tunnel on_fail %llu", index);
	if (ptr_interface)
		ptr_interface->on_interface_ipm_server_tunnel_fail(index);
}

void ipm_server_tunnel::on_to_watch()
{
	char c;
	ev_ssize_t n;

	// 只看不取：>0 还有数据要交付，等 join；0 是 FIN，出错是 RST，都没有数据了
	n = recv(event_get_fd(to_watch_event), &c, 1, MSG_PEEK);
	if (n > 0)
		return;
	if (n < 0 && util::is_socket_error_retriable(EVUTIL_SOCKET_ERROR()))
		return;

	slog_debug("tunnel %llu visitor closed before join", index);
	return on_fail();
}

void ipm_server_tunnel::on_join_timeout()
{
	slog_info("tunnel %llu join timeout", index);
	return on_fail();
}

void ipm_server_tunnel_to_watch_callback(evutil_socket_t fd, short what, void* ctx)
{
	if (ctx)	((ipm_server_tunnel*)ctx)->on_to_watch();
}

void ipm_server_tunnel_join_timer_callback(evutil_socket_t fd, short what, void* ctx)
{
	if (ctx)	((ipm_server_tunnel*)ctx)->on_join_timeout();
}