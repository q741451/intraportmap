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
	static void set_checksum_fast(const char* key, char* data, size_t sz_len);
	static bool check_checksum_fast(const char* key, const char* data, size_t sz_len);
	static std::string get_ipname_from_sockaddr(struct sockaddr* addr_in);
	static std::string get_portstr_from_sockaddr(struct sockaddr* addr_in);
	static unsigned short get_port_from_sockaddr(struct sockaddr* addr_in);
	static bool set_evutil_socket_keepalive(evutil_socket_t fd);
	static bool dual_stack_v4_fallback(const struct sockaddr* addr, struct sockaddr_in& out_v4);
	static char* get_udp_buffer();
	static evutil_socket_t bind_udp_socket(const struct sockaddr* addr, int addr_len, bool v6only);
	static unsigned long long ntohllx(unsigned long long x);
	static unsigned long long htonllx(unsigned long long x);
	static std::string string_format(const char* fmt_str, ...);
};

// 一组双栈 UDP socket 及其读事件。绑 :: 时会同时开一个 v4 通配，
// 与 TCP 侧 evconnlistener 那套双栈处理保持一致。
// 服务端的控制口、每个 agent 的公网口、客户端的每个 agent 口都用它
class ipm_udp_socket_pair
{
public:
	ipm_udp_socket_pair();

	bool open(struct event_base* base, const struct sockaddr* addr, int addr_len, event_callback_fn cb, void* arg);
	void close();
	bool is_open();

	evutil_socket_t fd4;
	evutil_socket_t fd6;
	struct event* event4;
	struct event* event6;

private:
	bool add_event(struct event_base* base, evutil_socket_t fd, event_callback_fn cb, void* arg, struct event*& out_event);
};

#endif
