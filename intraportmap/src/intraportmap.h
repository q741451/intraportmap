#ifndef _INTRAPORTMAP_H
#define _INTRAPORTMAP_H

class intraportmap
{
public:
	enum class CLIENT_STATE : UINT32
	{
		IDLE,
		DNS_QUERYING,
		SERVER_CONNECTING,
		RUNNING,
	};
	enum class SERVER_STATE : UINT32
	{
		IDLE,
		RUNNING,
	};

public:
	intraportmap();

	bool init(int argc, char* argv[]);
	bool exit();
	bool is_init();
	void reset();

	void exec();

public: // libevent过来的事件
	// DNS
	void on_evdns_getaddrinfo(int err, struct evutil_addrinfo* result);

private:
	bool init_config(int argc, char* argv[]);
	bool dns_query_server();
	bool client_connect_to_server(const struct sockaddr* addr, int addr_length);

private:
	bool is_state_init;
	bool is_server;
	struct event_base* event_base;
	struct evdns_base* evdns_base;	// 整个程序只查一个dns
	std::string server_name;
	std::string server_port_name;
	std::string to_server_name;
	std::string to_server_port_name;
	std::string from_server_name;
	std::string from_server_port_name;
};

#endif
