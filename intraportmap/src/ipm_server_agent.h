#ifndef _IPM_AGENT_SERVER_H
#define _IPM_AGENT_SERVER_H


class interface_ipm_server_agent
{
public:
	virtual ~interface_ipm_server_agent() {}
	virtual void on_interface_ipm_server_agent_fail(bufferevent* bev) = 0;
};

// 服务器端的用户fd服务模块，多个，listen来访客立即建立
class ipm_server_agent
{
public:
	bool init();
	bool is_init();
	bool exit();
	void reset();

private:
	bool is_state_init;
};

#endif