#include "stdafx.h"

void util::string_split(const char* str, const char* splits, std::vector<std::string>& result, bool rmempty /*= false*/)
{
	const char* cur = str;
	const char* cur_split = NULL;
	size_t szSplit = strlen(splits);
	std::string result_a;

	for (; (cur_split = strstr(cur, splits)) != NULL; cur += szSplit)
	{
		result_a.assign(cur, cur_split - cur);
		if (result_a.size() > 0 || rmempty == false)
			result.push_back(result_a);
		cur = cur_split;
	}
	result_a.assign(cur);
	if (result_a.size() > 0 || rmempty == false)
		result.push_back(result_a);
}

bool util::split_addr_string(const char* addrfull, std::string& addrstr, std::string& portstr)
{
	bool ret = false;
	std::string addrfull_s = addrfull;
	size_t last_mm = 0;

	if ((last_mm = addrfull_s.rfind(':')) == std::string::npos)
		goto end;

	addrstr = addrfull_s.substr(0, last_mm);
	if (addrstr.size() >= 2 && addrstr.c_str()[0] == '[' && addrstr.c_str()[addrstr.size() - 1] == ']')
	{
		addrstr = addrfull_s.substr(1, last_mm - 2);
	}
	portstr = addrfull_s.substr(last_mm + 1);

	ret = true;
end:
	return ret;
}

bool util::getaddrinfo_first(const char* host_name, const char* port_name, struct sockaddr_storage& addr, unsigned int* addr_len)
{
	bool ret = false;
	struct evutil_addrinfo hints;
	struct addrinfo* result = NULL;
	struct addrinfo* rp;

	memset(&hints, 0, sizeof(struct evutil_addrinfo));
	hints.ai_family = AF_UNSPEC;               /* Return IPv4 and IPv6 choices */
	hints.ai_socktype = SOCK_STREAM;             /* We want a TCP socket */
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* For wildcard IP address */
	hints.ai_protocol = IPPROTO_TCP;

	if (evutil_getaddrinfo(host_name, port_name, &hints, &result) != 0)
		goto end;

	rp = result;

	for (/*rp = result*/; rp != NULL; rp = rp->ai_next) {
		memcpy(&addr, rp->ai_addr, rp->ai_addrlen);
		*addr_len = (unsigned int)rp->ai_addrlen;
		// 只取第一个
		break;
	}

	if (host_name == NULL || strlen(host_name) == 0)
	{
		// 地址取空表示同时监听ipv6，覆盖默认设置

		struct sockaddr_in6 addr_in6_all;
		memset(&addr_in6_all, 0, sizeof(addr_in6_all));
		addr_in6_all.sin6_family = AF_INET6;
		addr_in6_all.sin6_port = htons(util::get_port_from_sockaddr((struct sockaddr*)&addr));
		memcpy(&addr, &addr_in6_all, sizeof(addr_in6_all));
		*addr_len = sizeof(addr_in6_all);
	}

	if (addr.ss_family != AF_INET6 && addr.ss_family != AF_INET)
		goto end;

	ret = true;
end:
	if (result)
		evutil_freeaddrinfo(result);
	return ret;
}

bool util::sockaddr_to_address(struct sockaddr* ai_addr, char* net_ip, unsigned int* net_port, unsigned int* net_is_ipv6)
{
	bool ret = false;

	switch (ai_addr->sa_family) {
	case AF_INET: {
		struct sockaddr_in* addr_in = (struct sockaddr_in*)ai_addr;
		memcpy(net_ip, &addr_in->sin_addr, sizeof(addr_in->sin_addr));
		*net_is_ipv6 = htonl(0);
		*net_port = htonl((unsigned int)ntohs(addr_in->sin_port));
		break;
	}
	case AF_INET6: {
		struct sockaddr_in6* addr_in6 = (struct sockaddr_in6*)ai_addr;
		memcpy(net_ip, &addr_in6->sin6_addr, sizeof(addr_in6->sin6_addr));
		*net_is_ipv6 = htonl(1);
		*net_port = htonl((unsigned int)ntohs(addr_in6->sin6_port));
		break;
	}
	default:
		goto end;
	}

	ret = true;
end:
	return ret;
}

void util::set_checksum(const char* key, char* data, size_t sz_len)
{
	std::string key_str = key;
	std::string check_buf;

	if (sz_len < 4)
		return;

	check_buf.reserve(key_str.size() * 2 + sz_len - 4);

	check_buf.append(key_str);
	check_buf.append(data, sz_len - 4);
	check_buf.append(key_str);

	*(unsigned int*)(data + sz_len - 4) = htonl(calc::crc32(0, check_buf.c_str(), check_buf.size()));
}

bool util::check_checksum(const char* key, const char* data, size_t sz_len)
{
	std::string key_str = key;
	std::string check_buf;

	if (sz_len < 4)
		return false;

	check_buf.reserve(key_str.size() * 2 + sz_len - 4);

	check_buf.append(key_str);
	check_buf.append(data, sz_len - 4);
	check_buf.append(key_str);

	return *(unsigned int*)(data + sz_len - 4) == htonl(calc::crc32(0, check_buf.c_str(), check_buf.size()));
}

std::string util::get_ipname_from_sockaddr(struct sockaddr* res)
{
	std::string buf_string;
	std::string ret;

	buf_string.resize(INET6_ADDRSTRLEN > INET_ADDRSTRLEN ? INET6_ADDRSTRLEN : INET_ADDRSTRLEN);

	switch (res->sa_family) {
	case AF_INET: {
		struct sockaddr_in* addr_in = (struct sockaddr_in*)res;

		evutil_inet_ntop(AF_INET, &(addr_in->sin_addr), (char*)buf_string.c_str(), INET_ADDRSTRLEN);
		break;
	}
	case AF_INET6: {
		struct sockaddr_in6* addr_in6 = (struct sockaddr_in6*)res;

		evutil_inet_ntop(AF_INET6, &(addr_in6->sin6_addr), (char*)buf_string.c_str(), INET6_ADDRSTRLEN);
		break;
	}
	default:
		buf_string = "err";
		break;
	}

	ret = buf_string.c_str();

	return ret;
}

std::string util::get_portstr_from_sockaddr(struct sockaddr* res)
{
	return util::string_format("%u", (unsigned int)get_port_from_sockaddr(res));
}

unsigned short util::get_port_from_sockaddr(struct sockaddr* res)
{
	switch (res->sa_family) {
	case AF_INET: {
		struct sockaddr_in* addr_in = (struct sockaddr_in*)res;
		return ntohs(addr_in->sin_port);
		break;
	}
	case AF_INET6: {
		struct sockaddr_in6* addr_in6 = (struct sockaddr_in6*)res;
		return ntohs(addr_in6->sin6_port);
	}
	default:
		return 0;
	}
}

bool util::set_evutil_socket_keepalive(evutil_socket_t fd)
{
	bool ret = false;
	int keepAlive = 1;
	int keepIdle = 40;
	int keepInterval = 20;

	if (fd == -1)
		goto end;

	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const char*)&keepAlive, sizeof(keepAlive)) != 0)
		goto end;

#ifdef WIN32
	{
		struct tcp_keepalive in_keep_alive;
		memset(&in_keep_alive, 0, sizeof(in_keep_alive));
		unsigned long ul_in_len = sizeof(struct tcp_keepalive);
		struct tcp_keepalive out_keep_alive;
		memset(&out_keep_alive, 0, sizeof(out_keep_alive));
		unsigned long ul_out_len = sizeof(struct tcp_keepalive);
		unsigned long ul_bytes_return = 0;

		in_keep_alive.onoff = 1;                                // 打开keepalive
		in_keep_alive.keepaliveinterval = keepInterval * 1000;  // 发送keepalive心跳时间间隔-单位为毫秒
		in_keep_alive.keepalivetime = keepIdle * 1000;          // 多长时间没有报文开始发送keepalive心跳包-单位为毫秒

		if (WSAIoctl(fd, SIO_KEEPALIVE_VALS, (LPVOID)&in_keep_alive, ul_in_len,
			(LPVOID)&out_keep_alive, ul_out_len, &ul_bytes_return, NULL, NULL) != 0)
			goto end;
	}
#else
#ifdef __APPLE__
	if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &keepIdle, sizeof(keepIdle)) != 0)
		goto end;
#else
	if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(keepIdle)) != 0)
		goto end;
#endif

	if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, (const char*)&keepInterval, sizeof(keepInterval)) != 0)
		goto end;
	{
		int keepCount = 5;
		if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, (const char*)&keepCount, sizeof(keepCount)) != 0)
			goto end;
	}
#endif

	ret = true;
end:
	return ret;
}

// 全进程共用的 UDP 收发缓冲。单线程事件循环下，一个包彻底处理完才会读下一个，
// 收包路径也不会递归回自己，所以不存在重入。
// 约定：调用方在 buf + IPM_UDP_SESSION_LEN 处收包，转发时把会话号写进它前面的
// 8 字节，全程不用再拷贝一次载荷
char* util::get_udp_buffer()
{
	static char udp_buffer[IPM_UDP_BUF_LEN];
	return udp_buffer;
}

// 绑定的是 IPv6 通配地址（::）时，双栈还需要额外补一个 IPv4 通配绑定。
// 返回 true 表示需要补，out_v4 已填成同端口的 0.0.0.0
bool util::dual_stack_v4_fallback(const struct sockaddr* addr, struct sockaddr_in& out_v4)
{
	struct sockaddr_in6 zero_in6;

	if (addr->sa_family != AF_INET6)
		return false;

	memset(&zero_in6, 0, sizeof(zero_in6));

	if (memcmp(&((struct sockaddr_in6*)addr)->sin6_addr, &zero_in6.sin6_addr, sizeof(zero_in6.sin6_addr)) != 0)
		return false;

	memset(&out_v4, 0, sizeof(struct sockaddr_in));
	out_v4.sin_family = AF_INET;
	out_v4.sin_addr.s_addr = htonl(INADDR_ANY);
	out_v4.sin_port = ((struct sockaddr_in6*)addr)->sin6_port;

	return true;
}

// 建一个非阻塞 UDP socket 并绑定。v6only 只对 AF_INET6 有意义：
// 与 TCP 侧一致，v6 只收 v6，v4 交给 dual_stack_v4_fallback 补的那个 socket
evutil_socket_t util::bind_udp_socket(const struct sockaddr* addr, int addr_len, bool v6only)
{
	evutil_socket_t fd = -1;
	bool ret = false;
	int v6 = v6only ? 1 : 0;

	if ((fd = socket(addr->sa_family, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		slog_error("udp socket error");
		goto end;
	}

	if (evutil_make_socket_nonblocking(fd) != 0)
	{
		slog_error("evutil_make_socket_nonblocking error");
		goto end;
	}

	if (evutil_make_listen_socket_reuseable(fd) != 0)
	{
		slog_error("evutil_make_listen_socket_reuseable error");
		goto end;
	}

#ifdef IPV6_V6ONLY
	if (addr->sa_family == AF_INET6)
	{
		// 拿不到 v6only 不致命，退回内核默认行为即可
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&v6, sizeof(v6)) != 0)
			slog_warn("IPV6_V6ONLY error");
	}
#endif

	if (bind(fd, addr, addr_len) != 0)
	{
		slog_error("udp bind error");
		goto end;
	}

	ret = true;
end:
	if (ret != true && fd != -1)
	{
		evutil_closesocket(fd);
		fd = -1;
	}
	return fd;
}

ipm_udp_socket_pair::ipm_udp_socket_pair()
	: fd4(-1), fd6(-1), event4(NULL), event6(NULL)
{
}

bool ipm_udp_socket_pair::open(struct event_base* base, const struct sockaddr* addr, int addr_len, event_callback_fn cb, void* arg)
{
	bool ret = false;
	struct sockaddr_in addr_in4_all;

	if (addr->sa_family == AF_INET6)
	{
		if ((fd6 = util::bind_udp_socket(addr, addr_len, true)) == -1)
		{
			slog_error("udp ipv6 bind error");
			goto end;
		}

		if (add_event(base, fd6, cb, arg, event6) != true)
			goto end;

		if (util::dual_stack_v4_fallback(addr, addr_in4_all))
		{
			// 监听所有，需要同时监听ipv4。与 TCP 侧一致，这一路失败只告警
			if ((fd4 = util::bind_udp_socket((struct sockaddr*)&addr_in4_all, sizeof(addr_in4_all), false)) == -1)
			{
				slog_warn("dual-stack udp ipv4 bind error");
			}
			else if (add_event(base, fd4, cb, arg, event4) != true)
			{
				goto end;
			}
		}
	}
	else if (addr->sa_family == AF_INET)
	{
		if ((fd4 = util::bind_udp_socket(addr, addr_len, false)) == -1)
		{
			slog_error("udp ipv4 bind error");
			goto end;
		}

		if (add_event(base, fd4, cb, arg, event4) != true)
			goto end;
	}
	else
	{
		slog_error("unknown sa_family udp bind error");
		goto end;
	}

	ret = true;
end:
	if (ret != true)
		close();
	return ret;
}

void ipm_udp_socket_pair::close()
{
	if (event4)
	{
		event_free(event4);
		event4 = NULL;
	}

	if (event6)
	{
		event_free(event6);
		event6 = NULL;
	}

	if (fd4 != -1)
	{
		evutil_closesocket(fd4);
		fd4 = -1;
	}

	if (fd6 != -1)
	{
		evutil_closesocket(fd6);
		fd6 = -1;
	}
}

bool ipm_udp_socket_pair::add_event(struct event_base* base, evutil_socket_t fd, event_callback_fn cb, void* arg, struct event*& out_event)
{
	if ((out_event = event_new(base, fd, EV_READ | EV_PERSIST, cb, arg)) == NULL)
	{
		slog_error("event_new error");
		return false;
	}

	if (event_add(out_event, NULL) != 0)
	{
		slog_error("event_add error");
		event_free(out_event);
		out_event = NULL;
		return false;
	}

	return true;
}

unsigned long long util::ntohllx(unsigned long long x)
{
	int ret_val[2] = { 0 };
	unsigned long long ret_val_64 = 0;

	ret_val[0] = ntohl(x >> 32);
	ret_val[1] = ntohl(((x & 0xFFFFFFFF) << 32) >> 32);

	memcpy(&ret_val_64, ret_val, sizeof(unsigned long long));

	return ret_val_64;
}

unsigned long long util::htonllx(unsigned long long x)
{
	int ret_val[2] = { 0 };
	unsigned long long ret_val_64 = 0;

	ret_val[0] = htonl(x >> 32);
	ret_val[1] = htonl(((x & 0xFFFFFFFF) << 32) >> 32);

	memcpy(&ret_val_64, ret_val, sizeof(unsigned long long));

	return ret_val_64;
}

std::string util::string_format(const char* fmt_str, ...) {
	int final_n, n = ((int)strlen(fmt_str)) * 2; /* Reserve two times as much as the length of the fmt_str */
	std::unique_ptr<char[]> formatted;
	va_list ap;
	while (1) {
		formatted.reset(new char[n]); /* Wrap the plain char array into the unique_ptr */
		strcpy(&formatted[0], fmt_str);
		va_start(ap, fmt_str);
		final_n = vsnprintf(&formatted[0], n, fmt_str, ap);
		va_end(ap);
		if (final_n < 0 || final_n >= n)
			n += abs(final_n - n + 1);
		else
			break;
	}
	return std::string(formatted.get());
}

