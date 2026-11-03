#include "stdafx.h"

void ipm_relay_read_callback(evutil_socket_t fd, short what, void* ctx);
void ipm_relay_write_callback(evutil_socket_t fd, short what, void* ctx);

ipm_relay::ipm_relay(struct event_base* base, interface_ipm_relay* ptr_interface_p, unsigned long long index_u) : ptr_interface(ptr_interface_p), index(index_u), root_event_base(base)
{
	reset();
}

bool ipm_relay::init(size_t buffer_size_sz)
{
	buffer_size = buffer_size_sz;
	is_state_init = true;
	return true;
}

bool ipm_relay::is_init()
{
	return is_state_init;
}

bool ipm_relay::exit()
{
	int id;

	for (id = 0; id < 2; id++)
	{
		if (sides[id].read_event)
			event_free(sides[id].read_event);
		if (sides[id].write_event)
			event_free(sides[id].write_event);
		if (sides[id].fd != -1)
			evutil_closesocket(sides[id].fd);
		if (sides[id].buf)
			free(sides[id].buf);
	}

	is_state_init = false;
	return true;
}

void ipm_relay::reset()
{
	int id;

	is_state_init = false;
	buffer_size = DEF_MAX_BUFFER;
	for (id = 0; id < 2; id++)
	{
		sides[id].relay = this;
		sides[id].id = id;
		sides[id].fd = -1;
		sides[id].read_event = NULL;
		sides[id].write_event = NULL;
		sides[id].is_reading = false;
		sides[id].is_writing = false;
		sides[id].buf = NULL;
		sides[id].size = 0;
		sides[id].len = 0;
		sides[id].pos = 0;
	}
}

bool ipm_relay::attach(int id, evutil_socket_t fd)
{
	return attach_side(id, fd, NULL);
}

bool ipm_relay::attach(int id, struct bufferevent* bev)
{
	// 写出去的只有握手，交接前应已发完
	if (evbuffer_get_length(bufferevent_get_output(bev)) != 0)
	{
		slog_error("relay %llu side %d output not flushed", index, id);
		return false;
	}

	if (attach_side(id, bufferevent_getfd(bev), bufferevent_get_input(bev)) != true)
		return false;

	// fd 归本类，bev 仍归调用方：摘掉 fd，调用方释放 bev 时就不会关它
	bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
	bufferevent_setfd(bev, -1);
	return true;
}

bool ipm_relay::is_attached(int id)
{
	return sides[id].fd != -1;
}

bool ipm_relay::start()
{
	if (!is_attached(0) || !is_attached(1))
	{
		slog_error("relay %llu start before both sides attached", index);
		return false;
	}

	if (set_events(0) != true || set_events(1) != true)
	{
		slog_error("relay %llu set_events error", index);
		return false;
	}
	return true;
}

void ipm_relay::on_read(int id)
{
	side_t& s = sides[id];
	ev_ssize_t n;
	int err;

	n = recv(s.fd, s.buf, (int)s.size, 0);
	if (n > 0)
	{
		s.len = (size_t)n;
		s.pos = 0;
		return send_to(1 - id);
	}

	if (n == 0)
	{
		slog_debug("relay %llu side %d eof", index, id);
		return on_close();
	}

	err = EVUTIL_SOCKET_ERROR();
	if (util::is_socket_error_retriable(err))
		return;
	slog_debug("relay %llu side %d recv error %s", index, id, evutil_socket_error_to_string(err));
	return on_close();
}

void ipm_relay::on_write(int id)
{
	return send_to(id);
}

// 只取资源，不登记事件。缓冲区和事件都留给 exit 统一释放；失败时不记下 fd，fd 仍归调用方
bool ipm_relay::attach_side(int id, evutil_socket_t fd, struct evbuffer* input)
{
	side_t& s = sides[id];
	size_t pending = input ? evbuffer_get_length(input) : 0;
	bool ret = false;

	if (s.buf)
	{
		slog_error("relay %llu side %d already attached", index, id);
		goto end;
	}

	// 已读到的数据多于一块缓冲区时，这一块开大些装下
	s.size = pending > buffer_size ? pending : buffer_size;
	if ((s.buf = (char*)malloc(s.size)) == NULL)
	{
		slog_error("relay %llu malloc error", index);
		goto end;
	}

	if (pending && evbuffer_copyout(input, s.buf, pending) != (ev_ssize_t)pending)
	{
		slog_error("relay %llu evbuffer_copyout error", index);
		goto end;
	}
	s.len = pending;

	if ((s.read_event = event_new(root_event_base, fd, EV_READ | EV_PERSIST, ipm_relay_read_callback, &s)) == NULL ||
		(s.write_event = event_new(root_event_base, fd, EV_WRITE | EV_PERSIST, ipm_relay_write_callback, &s)) == NULL)
	{
		slog_error("relay %llu event_new error", index);
		goto end;
	}

	s.fd = fd;
	ret = true;
end:
	if (ret != true)
		s.len = 0;
	return ret;
}

// 读：自己的缓冲区空着
// 写：另一边的缓冲区里有数据
bool ipm_relay::set_events(int id)
{
	side_t& s = sides[id];
	side_t& o = sides[1 - id];
	bool is_read = s.len == 0;
	bool is_write = o.pos < o.len;

	if (is_read != s.is_reading)
	{
		if ((is_read ? event_add(s.read_event, NULL) : event_del(s.read_event)) != 0)
			return false;
		s.is_reading = is_read;
	}

	if (is_write != s.is_writing)
	{
		if ((is_write ? event_add(s.write_event, NULL) : event_del(s.write_event)) != 0)
			return false;
		s.is_writing = is_write;
	}

	return true;
}

void ipm_relay::update()
{
	if (set_events(0) != true || set_events(1) != true)
	{
		slog_error("relay %llu set_events error", index);
		return on_close();
	}
}

// 把另一边读到的数据发给这一边
void ipm_relay::send_to(int id)
{
	side_t& s = sides[id];
	side_t& o = sides[1 - id];
	ev_ssize_t n;
	int err;

	while (o.pos < o.len)
	{
		n = send(s.fd, o.buf + o.pos, (int)(o.len - o.pos), 0);
		if (n > 0)
		{
			o.pos += (size_t)n;
			continue;
		}

		if (n < 0)
		{
			err = EVUTIL_SOCKET_ERROR();
			if (util::is_socket_error_retriable(err))
				return update();
			slog_debug("relay %llu side %d send error %s", index, id, evutil_socket_error_to_string(err));
		}
		return on_close();
	}

	o.len = 0;
	o.pos = 0;
	return update();
}

void ipm_relay::on_close()
{
	slog_debug("relay %llu close", index);
	if (ptr_interface)
		ptr_interface->on_interface_ipm_relay_close();
}

void ipm_relay_read_callback(evutil_socket_t fd, short what, void* ctx)
{
	ipm_relay::side_t* s = (ipm_relay::side_t*)ctx;

	if (s)	s->relay->on_read(s->id);
}

void ipm_relay_write_callback(evutil_socket_t fd, short what, void* ctx)
{
	ipm_relay::side_t* s = (ipm_relay::side_t*)ctx;

	if (s)	s->relay->on_write(s->id);
}
