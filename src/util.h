#ifndef _UTIL_H
#define _UTIL_H

class util
{
public:
	static void string_split(const char* str, const char* splits, std::vector<std::string>& result, bool rmempty = false);
	static bool split_addr_string(const char *addrfull, std::string &addrstr, std::string& portstr);
	static bool getaddrinfo_first(const char* host_name, const char* port_name, struct sockaddr_storage& addr, unsigned int* addr_len);
	static bool sockaddr_to_address(struct sockaddr* ai_addr, char* net_ip, unsigned int* net_port, unsigned int* net_is_ipv6);
	static void set_checksum(const char *key, char* data, size_t sz_len);
	static bool check_checksum(const char* key, const char* data, size_t sz_len);
	static std::string get_ipname_from_sockaddr(struct sockaddr* addr_in);
	static std::string get_portstr_from_sockaddr(struct sockaddr* addr_in);
	static unsigned short get_port_from_sockaddr(struct sockaddr* addr_in);
	static bool set_evutil_socket_keepalive(evutil_socket_t fd);
	static unsigned long long ntohll(unsigned long long x);
	static unsigned long long htonll(unsigned long long x);
	static std::string string_format(const char* fmt_str, ...);
};

#endif
