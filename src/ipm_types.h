#ifndef _IPM_TYPES_H
#define _IPM_TYPES_H

#ifdef WIN32
#pragma pack(1)
#endif
typedef struct _address_package
{
	unsigned int is_ipv6;
	char ip[16];
	unsigned int port;
}
#ifndef WIN32
__attribute__((packed))
#endif
address_package_t;
#ifdef WIN32
#pragma pack()
#endif

#ifdef WIN32
#pragma pack(1)
#endif
typedef struct _alloc_agent_package
{
	unsigned long long alloc_zero;
	address_package_t addr_pkg;
	unsigned int checksum;
}
#ifndef WIN32
__attribute__((packed))
#endif
alloc_agent_package_t;
#ifdef WIN32
#pragma pack()
#endif

// 注意：构造时清零是必须的。address_package_t 是 packed 的，配合 operator< 的
// memcmp 才能当 map 的 key；而 sockaddr_to_address 对 v4 只写 ip 的前 4 字节，
// 剩下 12 字节靠这里的清零兜底，否则同一个地址会算出两个不同的 key
class addr_pkg_idx
{
public:
	addr_pkg_idx()
	{
		memset(&addr_pkg, 0, sizeof(address_package_t));
	}
	address_package_t addr_pkg;
};

// 比较器必须随类型一起放在头文件里：多个编译单元都要拿 addr_pkg_idx 当 map 的 key
inline bool operator < (const addr_pkg_idx& item1, const addr_pkg_idx& item2)
{
	return memcmp(&item1.addr_pkg, &item2.addr_pkg, sizeof(address_package_t)) < 0;
}

// 与 shadowsocks 一致的三态：默认只开 TCP，-u 同时开 UDP，-U 只开 UDP
enum class IPM_MODE : unsigned int
{
	TCP_ONLY,
	TCP_AND_UDP,
	UDP_ONLY,
};

// ── UDP 端口映射的线上格式 ────────────────────────────────────────────
//
// 沿用 TCP 控制连接那套判别方式：前 8 字节为 0 是控制包，非 0 是会话号。
//
//   客户端 → 服务端   前8字节 == 0 : 注册/心跳，整包按 alloc_agent_package_t 解析
//   服务端 → 客户端   前8字节 == 0 : 上面那个包的 ACK（原样回显）
//   两个方向          前8字节 != 0 : 数据，[8B session_id][payload]
//
// 只有控制包带校验和，数据包不带 —— 与 TCP 侧一致，那边转发的字节流同样只在
// alloc/penetrate 这类控制包上校验。UDP 本身已有传输层校验和覆盖完整性，再叠
// 一层带 key 的 CRC32 是冗余；且 CRC32 线性，本来也不构成 MAC，留着只是每包一次
// 全量计算的开销。防注入靠的是 64 位随机播种的会话号（off-path 需猜中 2^64），
// 客户端侧的 socket 还是 connect() 过的，内核只收服务端地址来的包。
//
// session_id 由服务端全局分配，随机播种的单调递增计数器 —— 不能用 fd：fd 会被
// 立刻复用，迟到的数据报会被投递进复用了同一号码的另一个会话，造成跨会话串数据。
// 随机播种是为了避免服务端重启后与客户端残留的会话撞号。
//
// 两处 socket 约束（改动前务必看懂，不是可以随手优化掉的）：
//  1. 服务端对所有 agent、所有会话只用一个 UDP socket。客户端 NAT 的映射按
//     (client_ip, client_port, server_ip, server_port) 匹配，换个源端口发过去
//     会被严格 NAT 丢掉。
//  2. 客户端每个 agent 只用一个 socket，心跳和数据必须走同一个 —— 会话正是靠
//     蹭心跳维持的那条映射才免于各自保活的。
#define IPM_UDP_SESSION_LEN			8
// 合法 UDP 数据报的上限。收包缓冲必须按它开：缓冲小了内核会静默截断，
// 那比干脆丢弃更糟（转出去半个包）。
// 超长不判断、不告警、不丢弃，一律照转，交给 IP 分片
#define IPM_UDP_MAX_PAYLOAD			65507
#define IPM_UDP_BUF_LEN				(IPM_UDP_SESSION_LEN + IPM_UDP_MAX_PAYLOAD)

// 心跳沿用 TCP keepalive 的形状（空闲触发，随后密集探测），但间隔必须短得多：
// TCP 的 NAT 映射按 RFC 5382 至少 2 小时 4 分，UDP 的在 Linux conntrack 上未应答
// 时只有 30 秒。15 秒取自 RFC 8445（ICE）第 11 节对 STUN 保活 Tr 的规定
// 「SHOULD use a value of 15 seconds / MUST NOT use a value smaller than 15
// seconds」，相对 30 秒留出一倍余量
#define IPM_UDP_HEARTBEAT_IDLE		15		// 秒，这么久没「收到」服务端任何包才发心跳
#define IPM_UDP_REG_RETRY			2		// 秒，SERVER_REGISTERING 下重发注册的间隔
#define IPM_UDP_REG_TRIES			3		// 注册重试次数，用完转 WAITING 并重新解析 DNS
#define IPM_UDP_HEARTBEAT_TRIES		5		// RUNNING 下连续这么多次没 ACK 判定失联
// 服务端判定 agent 死亡。刻意与心跳间隔解耦、取绝对值：它必须明显长于客户端
// 自己那套「检测 + 重连」的耗时（15s 空闲 + 5×2s 探测 ≈ 25s，再加 -w 默认 15s），
// 否则正在恢复中的客户端会被服务端提前拆掉 agent，连带丢掉它名下所有会话
#define IPM_UDP_AGENT_TIMEOUT		60
#define IPM_UDP_SESSION_TIMEOUT		180		// 秒，会话空闲超时默认值，-T 可配
// 客户端侧会话比服务端多活这么久。不能更早：否则同一条会话中途换了后端源端口，
// 对按 addr:port 认客户端的有状态被代理主机来说就是换了个人
#define IPM_UDP_CLIENT_LAG			10
#define IPM_UDP_MAX_SESSION			16384	// 会话表上限，防止扫描把 fd 和内存打爆

#endif

