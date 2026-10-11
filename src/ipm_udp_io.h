#ifndef _IPM_UDP_IO_H
#define _IPM_UDP_IO_H

// UDP 数据面的收发，-G 打开批量收发（仅 Linux 5.0+）：
//  - 收：socket 开 UDP_GRO，内核把同一条流里、同一次中断到达的、一样长的几个包
//    首尾相接一次交上来，并告诉我们原来每个包多长
//  - 发：同一 socket、同一目的地、一样长的几个包拼成一次 sendmsg + UDP_SEGMENT，
//    整组只走一遍协议栈，到网卡驱动之前才切开
// 省的是每个包固定要付的那部分（系统调用、路由、netfilter、qdisc、驱动），
// 没有网卡 UDP 分段卸载的虚拟机上尤其明显。
//
// 线上格式完全不变：切开后的每一段都是独立的 UDP 包，与逐包发送逐字节相同，
// 所以两端各开各的，开不开 -G 都能互通。
//
// 配置不对就大声报错、拒绝运行，绝不悄悄退回逐包：
//  - 非 Linux、内核 < 5.0：启动时报错退出（见 set_batch()）
//  - 运行中网卡不支持校验和卸载（GSO 发送回 EIO）：报错退出，提示去掉 -G
// 段长超过出口 MTU（EINVAL）不算配置错误：那是个别包太大，这组逐包重发交给
// IP 分片，与不开 -G 时的行为完全一样
//
// 只合并「已经同时到手」的包，绝不为了凑批而等待：每次可读回调只收一次，
// 回调返回前全部发出。批的大小完全由包到达时本来有多密集决定，不改变时序
class ipm_udp_io
{
public:
	// 打开 -G。内核不支持时报错并返回 false，调用方拒绝启动
	static bool set_batch(bool enable);
	static bool is_batch();
	// 给收数据的 socket 打开 GRO。set_batch() 已确认内核支持，这里失败属于意料之外，
	// 同样报错退出
	static void enable_gro(evutil_socket_t fd);
	// 发一个包，可选地在前面加 8 字节会话头。dst 为 NULL 表示 fd 已 connect。
	// 超长不判断，交给 IP 分片，与逐包时代的行为一致
	static bool send_one(evutil_socket_t fd, const struct sockaddr* dst, unsigned int dst_len, unsigned long long id, const char* data, size_t len);
	// -G 跑不下去了：打 ERROR 后退出进程。只用于配置与环境不符这类继续运行就等于
	// 悄悄降级的情况
	static void fatal(const char* what);

private:
	static bool batch;
};

// 一次收包。开了 GRO 时可能是同一条流里好几个原始数据报首尾相接，next() 逐个切出来：
// 每段 seg 字节，最后一段可以短。没合并时就是一段。空数据报也算一段，照常交出去
class ipm_udp_recv
{
public:
	ipm_udp_recv();

	// false 表示这次没收到可用的东西（EAGAIN、出错、被截断），调用方直接返回即可。
	// from 为 NULL 表示 fd 已 connect，不关心来源
	bool read(evutil_socket_t fd, char* buf, size_t buf_len, struct sockaddr* from, ev_socklen_t* from_len);
	bool next(char*& data, size_t& len);

private:
	char* buf;
	size_t total;
	size_t seg;
	size_t off;
	bool done;
};

// 一次回调里要发的包。add() 把能合进当前这组的包攒起来，合不进时先把这组发掉；
// 析构时发掉最后一组，所以只能作为回调里的局部变量用。
// 攒着的是指向收包缓冲的指针，不拷贝载荷：缓冲和 fd 都必须活到 flush。
// 收包缓冲在下次回调才会被覆盖；数据路径上不会关 socket（会话和 agent 只在
// 定时器回调里回收），两者都满足。
// 没开 -G 时 add() 就是 send_one()，与原来逐包发送一样
class ipm_udp_batch
{
public:
	ipm_udp_batch();
	~ipm_udp_batch();

	// id 非 0 时在载荷前面加 8 字节会话头（0 是控制包的判别值，本来就不是合法会话号）
	void add(evutil_socket_t fd, const struct sockaddr* dst, unsigned int dst_len, unsigned long long id, const char* data, size_t len);
	void flush();

private:
	// iov 指向自己的 ids[]，拷贝出来的副本会指回原对象，所以不许拷贝
	ipm_udp_batch(const ipm_udp_batch&);
	ipm_udp_batch& operator=(const ipm_udp_batch&);

	bool can_append(evutil_socket_t fd_p, const struct sockaddr* dst_p, unsigned int dst_len_p, bool with_id_p, size_t pkt_len);
#ifdef __linux__
	void send_each();
#endif

private:
	evutil_socket_t fd;
	struct sockaddr_storage dst;
	unsigned int dst_len;
	bool with_id;		// 这组是否带会话头，一组里要么都带要么都不带
	size_t seg;			// 这组的段长，取第一个包的长度
	size_t bytes;
	unsigned int nseg;
#ifdef __linux__
	unsigned long long ids[IPM_UDP_GSO_MAX_SEGS];			// 会话头，网络字节序
	struct iovec iov[IPM_UDP_GSO_MAX_SEGS * 2];			// 每段至多「头 + 载荷」两块
	unsigned int niov;
#endif
};

#endif
