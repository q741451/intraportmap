#include "stdafx.h"

int main(int argc, char* argv[])
{
	int ret = -1;
	intraportmap ipm;

	if (argc < 2)
	{
		printf("syntax: %s [-c] [-s server:port] [-t to_server:to_port] [-f from_server:from_port] [-k key] [-w client_reconn_time] [-b buffer_per_tunnel] [-u] [-U] [-T udp_session_timeout] [-G]\r\n", argv[0]);
		printf("  -u  map UDP as well as TCP\r\n");
		printf("  -U  map UDP only\r\n");
		printf("  -T  UDP session idle timeout in seconds (default %d)\r\n", IPM_UDP_SESSION_TIMEOUT);
		printf("  -G  batch UDP send/receive with GSO/GRO (Linux 5.0+ only, less CPU)\r\n");
		goto end;
	}

#ifdef _WIN32
	{
		WSADATA WSAData;
		if (WSAStartup(0x101, &WSAData) != 0)
		{
			slog_error("WSAStartup error");
			goto end;
		}
	}
#else
	// 往对端已关闭的连接上写，内核发 SIGPIPE，默认处置是不留日志地杀掉进程。
	// 忽略后写失败变成 EPIPE，由 libevent 当普通连接错误报上来
	signal(SIGPIPE, SIG_IGN);
#endif

	if (ipm.init(argc, argv) != true)
		goto end;

	ipm.exec();

	if (ipm.exit() != true)
		goto end;

	ipm.reset();

	ret = 0;
end:
	if (ipm.is_init())
	{
		ipm.exit();
		ipm.reset();
	}
#ifdef _WIN32
	WSACleanup();
#endif
	return ret;
}

