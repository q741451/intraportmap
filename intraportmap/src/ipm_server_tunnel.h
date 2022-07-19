#ifndef _IPM_TUNNEL_SERVER_H
#define _IPM_TUNNEL_SERVER_H

class interface_ipm_server_tunnel
{
public:
	virtual ~interface_ipm_server_tunnel() {}
	virtual void on_interface_ipm_server_tunnel_fail(evutil_socket_t to_client_fd) = 0;
};

class ipm_server_tunnel
{
public:
	// ipm_server_tunnel(struct event_base* base, interface_ipm_server_tunnel* ptr_interface_p, evutil_socket_t to_client_fd);

	bool init();
	bool is_init();
	bool exit();
	void reset();


private:
	bool is_state_init;
};

#endif