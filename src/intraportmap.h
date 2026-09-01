#ifndef _INTRAPORTMAP_H
#define _INTRAPORTMAP_H

class intraportmap : public interface_ipm_client, public interface_ipm_server,
	public interface_ipm_client_udp, public interface_ipm_server_udp
{
public:
	intraportmap();

	bool init(int argc, char* argv[]);
	bool is_init();
	bool exit();
	void reset();

	void exec();

public:
	virtual void on_interface_ipm_client_fail();
	virtual void on_interface_ipm_server_fail();
	virtual void on_interface_ipm_client_udp_fail();
	virtual void on_interface_ipm_server_udp_fail();

public: // libevent过来的事件
	// 中断
	void on_signal_event(evutil_socket_t sig, short events);

private:
	bool init_config(int argc, char* argv[]);
	bool register_signal_event();

private:
	bool is_state_init;
	bool is_server;
	struct event* signal_event;
	struct event_base* root_event_base;
	std::shared_ptr<ipm_client> sp_ipm_client;
	std::shared_ptr<ipm_server> sp_ipm_server;
	std::shared_ptr<ipm_client_udp> sp_ipm_client_udp;
	std::shared_ptr<ipm_server_udp> sp_ipm_server_udp;
	unsigned int client_reconn_time;
	std::string server_name;
	std::string server_port_name;
	std::string to_server_name;
	std::string to_server_port_name;
	std::string from_server_name;
	std::string from_server_port_name;
	std::string key;
	unsigned long long max_buffer;
	IPM_MODE mode;
	unsigned int session_timeout;	// UDP 会话空闲超时，秒
};

#endif
