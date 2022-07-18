#ifndef _UTIL_H
#define _UTIL_H

class util
{
public:
	static void string_split(const char* str, const char* splits, std::vector<std::string>& result, bool rmempty = false);
	static bool split_addr_string(const char *addrfull, std::string &addrstr, std::string& portstr);
	static bool getaddrinfo_first(const char* host_name, const char* port_name, struct sockaddr_storage& addr);
	static bool sockaddr_to_address(struct sockaddr* ai_addr, char* net_ip, unsigned int* net_port, unsigned int* net_is_ipv6);
	static void set_checksum(char* data, size_t sz_len);
	static bool check_checksum(const char* data, size_t sz_len, const char* cksum);
	static std::string get_ipname_from_sockaddr(struct sockaddr* addr_in);
};

#endif
