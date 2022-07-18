#ifndef _IPM_AGENT_SERVER_H
#define _IPM_AGENT_SERVER_H


class interface_ipm_server_agent
{
public:
	virtual ~interface_ipm_server_agent() {}
};

// 服务器端的用户fd服务模块，多个，listen来访客立即建立
class ipm_server_agent
{
public:
	// 客户端主连接
	// 服务器代理listen

	// listen结果，通知上层有新客户
	// 客户端结果(读写挂)

	// 不存tunnel tunnel查找需要上层
};

#endif