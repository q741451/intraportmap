#include "stdafx.h"

static void evutil_addrinfo_callback(int err, struct evutil_addrinfo* ai, void* arg);

intraportmap::intraportmap()
{
	reset();
}

bool intraportmap::init(int argc, char* argv[])
{
	bool ret = false;

	if (init_config(argc, argv) != true)
		goto end;

	if ((event_base = event_base_new()) == NULL)
	{
		printf("event_base_new error\r\n");
		goto end;
	}

	if ((evdns_base = evdns_base_new(event_base, EVDNS_BASE_DISABLE_WHEN_INACTIVE)) == NULL)
	{
		printf("evdns_base_new error\r\n");
		goto end;
	}

	if (is_server)
	{
		printf("run as server\r\n");
	}
	else
	{
		printf("run as client\r\n");
		if (dns_query_server() != true)
		{
			printf("dns_query_server error\r\n");
			goto end;
		}
	}


	ret = true;
end:
	if (ret == true)
	{
		is_state_init = true;
	}
	else
	{
		exit();
		reset();
	}
	return ret;
}

bool intraportmap::exit()
{
	bool ret = false;

	if (evdns_base)
		evdns_base_free(evdns_base, 1);

	if (event_base)
		event_base_free(event_base);

	is_state_init = false;

	ret = true;
//end:
	return ret;
}

bool intraportmap::is_init()
{
	return is_state_init;
}

void intraportmap::reset()
{
	is_server = true;
	is_state_init = false;
	evdns_base = NULL;
	event_base = NULL;
}

void intraportmap::exec()
{
	event_base_dispatch(event_base);
}

void intraportmap::on_evdns_getaddrinfo(int err, struct evutil_addrinfo* result)
{
	struct evutil_addrinfo* rp, * ipv4v6bindall;

	rp = result;

	if (server_name.size() == NULL) {
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
		//listen_sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		//if (listen_sock == -1) {
		//	continue;
		//}

		// bufferevent_socket_connect

		// 只取第一个
		break;
	}

	// 改变状态 SERVER_CONNECTING
}

bool intraportmap::init_config(int argc, char* argv[])
{
	bool ret = false;
	std::string server_full;
	std::string to_full;
	std::string from_full;
	int opt = 0;

	while ((opt = getopt(argc, argv, "cs:t:f:")) != -1) {
		switch (opt) {
		case 'c':
			is_server = false;
			break;
		case 's':
			server_full = optarg;
			break;
		case 't':
			to_full = optarg;
			break;
		case 'f':
			from_full = optarg;
			break;
		default:
			printf("Unknown option %c\r\n", opt);
			goto end;
		}
	}

	if (util::split_addr_string(server_full.c_str(), server_name, server_port_name) != true)
	{
		printf("server_full error\r\n");
		goto end;
	}

	if (!is_server)
	{
		if (util::split_addr_string(to_full.c_str(), to_server_name, to_server_port_name) != true)
		{
			printf("to_full error\r\n");
			goto end;
		}
		if (util::split_addr_string(from_full.c_str(), from_server_name, from_server_port_name) != true)
		{
			printf("from_full error\r\n");
			goto end;
		}
	}

	ret = true;
end:
	return ret;
}

bool intraportmap::dns_query_server()
{
	bool ret = false;
	struct evutil_addrinfo hints;

	memset(&hints, 0, sizeof(struct evutil_addrinfo));
	hints.ai_family = AF_UNSPEC;               /* Return IPv4 and IPv6 choices */
	hints.ai_socktype = SOCK_STREAM;             /* We want a TCP socket */
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* For wildcard IP address */
	hints.ai_protocol = IPPROTO_TCP;

	printf("resolving %s:%s...\r\n", server_name.c_str(), server_port_name.c_str());

	// 挂了也会调cb，无需检测返回值
	evdns_getaddrinfo(evdns_base, server_name.c_str(), server_port_name.c_str(), &hints, evutil_addrinfo_callback, this);

	ret = true;

	return ret;
}

bool intraportmap::client_connect_to_server(const struct sockaddr* addr, int addr_length)
{
	bool ret = false;

	/*

	if ((bufferevent = bufferevent_socket_new(event_base, -1, BEV_OPT_CLOSE_ON_FREE)) == NULL)
	{
		printf("evdns_base_new error\r\n");
		goto end;
	}

	if (bufferevent_socket_connect(bufferevent, addr, addr_length) != 0)
		goto end;

		*/

	ret = true;
end:
	return ret;
}

static void evutil_addrinfo_callback(int err, struct evutil_addrinfo* ai, void* arg)
{
	if (arg)
		((intraportmap*)arg)->on_evdns_getaddrinfo(err, ai);
}