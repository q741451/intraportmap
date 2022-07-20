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
	std::vector<std::string> strs;

	string_split(addrfull, ":", strs, false);

	if (strs.size() != 2)
		goto end;

	addrstr = strs[0];
	portstr = strs[1];

	ret = true;
end:
	return ret;
}

bool util::getaddrinfo_first(const char* host_name, const char* port_name, struct sockaddr_storage& addr, unsigned int* addr_len)
{
	bool ret = false;
	struct evutil_addrinfo hints;
	struct addrinfo* result = NULL;
	struct addrinfo* rp, * ipv4v6bindall;

	memset(&hints, 0, sizeof(struct evutil_addrinfo));
	hints.ai_family = AF_UNSPEC;               /* Return IPv4 and IPv6 choices */
	hints.ai_socktype = SOCK_STREAM;             /* We want a TCP socket */
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* For wildcard IP address */
	hints.ai_protocol = IPPROTO_TCP;

	if (getaddrinfo(host_name, port_name, &hints, &result) != 0)
		goto end;

	rp = result;

	if (host_name == NULL || strlen(host_name) == 0) {
		ipv4v6bindall = result;

		/* Loop over all address infos found until a IPV6 address is found. */
		while (ipv4v6bindall) {
			if (ipv4v6bindall->ai_family == AF_INET6) {
				rp = ipv4v6bindall; /* Take first IPV6 address available */
				break;
			}
			ipv4v6bindall = ipv4v6bindall->ai_next; /* Get next address info, if any */
		}
	}

	for (/*rp = result*/; rp != NULL; rp = rp->ai_next) {
		memcpy(&addr, rp->ai_addr, rp->ai_addrlen);
		*addr_len = (unsigned int)rp->ai_addrlen;
		// 只取第一个
		break;
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

unsigned long long util::ntohll(unsigned long long x)
{
	int ret_val[2] = { 0 };

	ret_val[0] = ntohl(x >> 32);
	ret_val[1] = ntohl(((x & 0xFFFFFFFF) << 32) >> 32);

	return *(unsigned long long*)ret_val;
}

unsigned long long util::htonll(unsigned long long x)
{
	int ret_val[2] = { 0 };

	ret_val[0] = htonl(x >> 32);
	ret_val[1] = htonl(((x & 0xFFFFFFFF) << 32) >> 32);

	return *(unsigned long long*)ret_val;
}

