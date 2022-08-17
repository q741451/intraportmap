#ifndef _INTRAPORTMAP_H
#define _INTRAPORTMAP_H

class intraportmap : public interface_ipm_client, public interface_ipm_server
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
	unsigned int client_reconn_time;
	std::string server_name;
	std::string server_port_name;
	std::string to_server_name;
	std::string to_server_port_name;
	std::string from_server_name;
	std::string from_server_port_name;
	std::string key;
	unsigned long long max_buffer;
};

#endif
