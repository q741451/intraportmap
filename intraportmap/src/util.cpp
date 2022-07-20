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
		// ֻȡ��һ��
		break;
	}

	if (host_name == NULL || strlen(host_name) == 0)
	{
		// ��ַȡ�ձ�ʾͬʱ����ipv6������Ĭ������

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
		freeaddrinfo(result);
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

void util::set_checksum(char* data, size_t sz_len)
{
	if (sz_len >= 4)
		*(unsigned int*)(data + sz_len - 4) = htonl(0xffffffff);
}

bool util::check_checksum(const char* data, size_t sz_len)
{
	if (sz_len < 4)
		return false;
	return *(unsigned int*)(data + sz_len - 4) == htonl(0xffffffff);
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

unsigned long long util::ntohll(unsigned long long x)
{
	int ret_val[2] = { 0 };
	unsigned long long ret_val_64 = 0;

	ret_val[0] = ntohl(x >> 32);
	ret_val[1] = ntohl(((x & 0xFFFFFFFF) << 32) >> 32);

	memcpy(&ret_val_64, ret_val, sizeof(unsigned long long));

	return ret_val_64;
}

unsigned long long util::htonll(unsigned long long x)
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

