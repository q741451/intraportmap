#ifndef _IPM_TUNNEL_CLIENT_H
#define _IPM_TUNNEL_CLIENT_H

// 配对完成后的转发tunnel，客户端收到控制信号编号立即建立
// 客户端（一个）
class interface_ipm_tunnel_client
{
public:
	virtual ~interface_ipm_tunnel_client() {}
	virtual void on_interface_ipm_tunnel_client_fail(unsigned int index) = 0;
};
class ipm_tunnel_client
{
public:
	enum class TO_STATE : UINT32
	{
		IDLE,
		CONNECTING,
		RUNNING,
	};
	enum class FROM_STATE : UINT32
	{
		IDLE,
		CONNECTING,
		PENETRATING,
		RUNNING,
	};

	ipm_tunnel_client(struct event_base* base, interface_ipm_tunnel_client* ptr_interface_p);

private:
	bool is_state_init;
	interface_ipm_tunnel_client* ptr_interface;
	// 不释放的变量
	struct event_base* root_event_base;		// 来自外部
};

#endif