#include "stdafx.h"
#include <stdlib.h>
#include <string.h>

#ifdef __linux__
// 老的 libc 头文件里可能没有，值来自内核 include/uapi/linux/udp.h
#ifndef UDP_SEGMENT
#define UDP_SEGMENT		103
#endif
#ifndef UDP_GRO
#define UDP_GRO			104
#endif
#endif

bool ipm_udp_io::batch = false;

// 配置不对就拒绝启动，不悄悄降级。这里的检查只决定「能不能启动」，不存在
// 「检查没过就换条路走」：quictun 在 SO_TXTIME 上吃过的亏是设置成功不代表 qdisc
// 真的照办，那种探测有假阳性；UDP_SEGMENT/UDP_GRO 设得上就是内核支持，与 qdisc、
// 网卡无关，没有这个问题。
// 内核 < 4.18 不认识 UDP_SEGMENT，若不在这里拦下，会把这个 cmsg 静默忽略，
// 整组拼起来的载荷成了一个大数据报发出去，对端收到的是粘在一起的几个包。
// 4.18～5.0 能发不能收合并，-G 只生效一半，也算配置不对
bool ipm_udp_io::set_batch(bool enable)
{
#ifdef __linux__
	evutil_socket_t fd = -1;
	int zero = 0;
	int on = 1;
	int err = 0;

	batch = false;

	if (!enable)
		return true;

	// UDP_SEGMENT 设 0 就是不改变默认行为，只看内核认不认这个选项。
	// errno 要在 close 之前取，close 可能改写它
	if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		err = errno;
	else
	{
		if (setsockopt(fd, IPPROTO_UDP, UDP_SEGMENT, (const char*)&zero, sizeof(zero)) != 0
			|| setsockopt(fd, IPPROTO_UDP, UDP_GRO, (const char*)&on, sizeof(on)) != 0)
			err = errno;
		evutil_closesocket(fd);
	}

	if (err != 0)
	{
		slog_error("-G needs Linux 5.0+ (UDP_SEGMENT and UDP_GRO), this kernel does not support it: %s", strerror(err));
		return false;
	}

	batch = true;
	return true;
#else
	if (enable)
	{
		slog_error("-G is only supported on Linux");
		return false;
	}
	return true;
#endif
}

bool ipm_udp_io::is_batch()
{
	return batch;
}

void ipm_udp_io::enable_gro(evutil_socket_t fd)
{
#ifdef __linux__
	int on = 1;

	if (!batch || fd == -1)
		return;

	if (setsockopt(fd, IPPROTO_UDP, UDP_GRO, (const char*)&on, sizeof(on)) != 0)
		fatal("setsockopt UDP_GRO failed");
#endif
}

// 在 libevent 回调深处发现的，没有现成的路径把失败一层层传回 main，直接退出。
// exit() 会把 stdio 缓冲刷出去，ERROR 这一行不会丢
void ipm_udp_io::fatal(const char* what)
{
	slog_error("%s: %s. -G cannot run here, remove -G and restart", what, strerror(errno));
	exit(1);
}

bool ipm_udp_io::send_one(evutil_socket_t fd, const struct sockaddr* dst, unsigned int dst_len, unsigned long long id, const char* data, size_t len)
{
#ifdef _WIN32
	// Windows 不追求效率，用的还是原来那几个调用：会话头和载荷拼进一块缓冲再发
	static char pkt[IPM_UDP_SESSION_LEN + IPM_UDP_BUF_LEN];
	const char* p = data;
	size_t p_len = len;

	if (id != 0)
	{
		*(unsigned long long*)pkt = util::htonllx(id);
		memcpy(pkt + IPM_UDP_SESSION_LEN, data, len);
		p = pkt;
		p_len = IPM_UDP_SESSION_LEN + len;
	}

	if (dst == NULL)
		return send(fd, p, (int)p_len, 0) >= 0;
	return sendto(fd, p, (int)p_len, 0, dst, (int)dst_len) >= 0;
#else
	unsigned long long hdr = util::htonllx(id);
	struct iovec iov[2];
	struct msghdr msg;
	int n = 0;

	if (id != 0)
	{
		iov[n].iov_base = &hdr;
		iov[n].iov_len = IPM_UDP_SESSION_LEN;
		n++;
	}
	iov[n].iov_base = (void*)data;
	iov[n].iov_len = len;
	n++;

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (void*)dst;
	msg.msg_namelen = dst ? dst_len : 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = n;

	return sendmsg(fd, &msg, 0) >= 0;
#endif
}

ipm_udp_recv::ipm_udp_recv()
	: buf(NULL), total(0), seg(0), off(0), done(true)
{
}

bool ipm_udp_recv::read(evutil_socket_t fd, char* buf_p, size_t buf_len, struct sockaddr* from, ev_socklen_t* from_len)
{
	int n = -1;

	buf = buf_p;
	off = 0;
	done = true;

#ifdef __linux__
	if (ipm_udp_io::is_batch())
	{
		char ctl[CMSG_SPACE(sizeof(int))];
		struct iovec iov;
		struct msghdr msg;
		struct cmsghdr* cmsg = NULL;
		int gso_size = 0;

		iov.iov_base = buf;
		iov.iov_len = buf_len;
		memset(&msg, 0, sizeof(msg));
		msg.msg_name = from;
		msg.msg_namelen = from ? *from_len : 0;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = ctl;
		msg.msg_controllen = sizeof(ctl);

		if ((n = recvmsg(fd, &msg, 0)) < 0)
			return false;

		// 理论上都不会发生，真发生了就是 bug，报一次 ERROR 并丢掉这一批：
		//  - MSG_TRUNC：缓冲按单个数据报和 GRO 合并的上限开足了。截断了宁可整批
		//    丢掉，也不能把半个包转出去
		//  - MSG_CTRUNC：控制信息被截断，拿不到段长，会把合并的一批当成一个包
		//    转出去。只开了 GRO 这一种 cmsg，ctl 刚好装得下
		if (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC))
		{
			static bool reported = false;
			if (!reported)
			{
				slog_error("udp recv %s (%d bytes into a %u-byte buffer), batch dropped; this is a bug",
					(msg.msg_flags & MSG_TRUNC) ? "data truncated" : "control data truncated", n, (unsigned int)buf_len);
				reported = true;
			}
			return false;
		}

		if (from)
			*from_len = msg.msg_namelen;

		for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg))
		{
			if (cmsg->cmsg_level == IPPROTO_UDP && cmsg->cmsg_type == UDP_GRO)
				memcpy(&gso_size, CMSG_DATA(cmsg), sizeof(gso_size));
		}

		total = (size_t)n;
		seg = (gso_size > 0 && (size_t)gso_size < total) ? (size_t)gso_size : total;
		done = false;
		return true;
	}
#endif

	if (from)
		n = recvfrom(fd, buf, (int)buf_len, 0, from, from_len);
	else
		n = recv(fd, buf, (int)buf_len, 0);

	if (n < 0)
		return false;

	total = (size_t)n;
	seg = total;
	done = false;
	return true;
}

bool ipm_udp_recv::next(char*& data, size_t& len)
{
	if (done)
		return false;

	data = buf + off;
	len = (total - off < seg) ? total - off : seg;
	off += len;

	// 空数据报 total == 0，这里第一次就置 done，正好交出一段空包
	if (off >= total)
		done = true;

	return true;
}

ipm_udp_batch::ipm_udp_batch()
	: fd(-1), dst_len(0), with_id(false), seg(0), bytes(0), nseg(0)
#ifdef __linux__
	, niov(0)
#endif
{
	memset(&dst, 0, sizeof(dst));
}

ipm_udp_batch::~ipm_udp_batch()
{
	flush();
}

void ipm_udp_batch::add(evutil_socket_t fd_p, const struct sockaddr* dst_p, unsigned int dst_len_p, unsigned long long id, const char* data, size_t len)
{
#ifdef __linux__
	size_t pkt_len = len + (id != 0 ? IPM_UDP_SESSION_LEN : 0);

	// 空包当不了 GSO 的段，单独发；顺序上先把攒着的发掉
	if (!ipm_udp_io::is_batch() || pkt_len == 0)
	{
		flush();
		ipm_udp_io::send_one(fd_p, dst_p, dst_len_p, id, data, len);
		return;
	}

	if (nseg > 0 && !can_append(fd_p, dst_p, dst_len_p, id != 0, pkt_len))
		flush();

	if (nseg == 0)
	{
		fd = fd_p;
		dst_len = dst_p ? dst_len_p : 0;
		if (dst_len)
			memcpy(&dst, dst_p, dst_len);
		with_id = id != 0;
		seg = pkt_len;
	}

	if (id != 0)
	{
		ids[nseg] = util::htonllx(id);
		iov[niov].iov_base = &ids[nseg];
		iov[niov].iov_len = IPM_UDP_SESSION_LEN;
		niov++;
	}
	iov[niov].iov_base = (void*)data;
	iov[niov].iov_len = len;
	niov++;

	bytes += pkt_len;
	nseg++;
#else
	ipm_udp_io::send_one(fd_p, dst_p, dst_len_p, id, data, len);
#endif
}

// GSO 的规矩：同一 socket、同一目的地；各段等长，只有最后一段可以短；
// 段数和总长都有上限。另外一组里要么都带会话头、要么都不带，send_each() 靠它
// 拆段（实际上同一个 fd 上的包本来就是同一个方向，这条不影响合并）
bool ipm_udp_batch::can_append(evutil_socket_t fd_p, const struct sockaddr* dst_p, unsigned int dst_len_p, bool with_id_p, size_t pkt_len)
{
	unsigned int len_p = dst_p ? dst_len_p : 0;

	if (fd_p != fd || len_p != dst_len || with_id_p != with_id)
		return false;
	if (dst_len && memcmp(dst_p, &dst, dst_len) != 0)
		return false;
	if (nseg >= IPM_UDP_GSO_MAX_SEGS)
		return false;
	// 比段长长的放不进；上一个已经是短尾，这组就封口了
	if (pkt_len > seg || bytes % seg != 0)
		return false;
	if (bytes + pkt_len > IPM_UDP_GSO_MAX_BYTES)
		return false;
	return true;
}

void ipm_udp_batch::flush()
{
#ifdef __linux__
	char ctl[CMSG_SPACE(sizeof(uint16_t))];
	struct msghdr msg;
	struct cmsghdr* cmsg = NULL;
	uint16_t gso_size = (uint16_t)seg;

	if (nseg == 0)
		return;

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = dst_len ? (void*)&dst : NULL;
	msg.msg_namelen = dst_len;
	msg.msg_iov = iov;
	msg.msg_iovlen = niov;

	// 只有一段就是普通的一次发送，不带 cmsg，行为与逐包时完全相同
	if (nseg > 1)
	{
		memset(ctl, 0, sizeof(ctl));
		msg.msg_control = ctl;
		msg.msg_controllen = sizeof(ctl);
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = IPPROTO_UDP;
		cmsg->cmsg_type = UDP_SEGMENT;
		cmsg->cmsg_len = CMSG_LEN(sizeof(uint16_t));
		memcpy(CMSG_DATA(cmsg), &gso_size, sizeof(gso_size));
	}

	if (sendmsg(fd, &msg, 0) < 0 && nseg > 1)
	{
		// EIO：出口网卡不支持校验和卸载，GSO 在这台机器上用不了，是配置不对，退出。
		// EINVAL 等：比如段长超过了 MTU，这组逐包重发，交给 IP 分片，与逐包时一致。
		// GSO 发送是原子的，报错就是一个都没发，逐包重发不会重复。
		// 发送缓冲满（EAGAIN）就丢：内核看的是已占用量有没有到上限，不是这一包
		// 放不放得下，紧接着逐包重发也一样会失败，逐包时代也是一样丢
		if (errno == EIO)
			ipm_udp_io::fatal("UDP GSO send rejected by the kernel with EIO (egress NIC has no checksum offload?)");
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			send_each();
	}

	nseg = 0;
	niov = 0;
	bytes = 0;
#endif
}

#ifdef __linux__
// 把攒着的这组拆开逐个发。带会话头时每段是「头 + 载荷」两块，否则一块
void ipm_udp_batch::send_each()
{
	struct msghdr msg;
	unsigned int i = 0;
	unsigned int n = with_id ? 2 : 1;

	for (i = 0; i < niov; i += n)
	{
		memset(&msg, 0, sizeof(msg));
		msg.msg_name = dst_len ? (void*)&dst : NULL;
		msg.msg_namelen = dst_len;
		msg.msg_iov = &iov[i];
		msg.msg_iovlen = n;
		sendmsg(fd, &msg, 0);
	}
}
#endif
