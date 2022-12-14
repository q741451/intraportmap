#include "stdafx.h"

int main(int argc, char* argv[])
{
	int ret = -1;
	intraportmap ipm;

	if (argc < 2)
	{
		printf("syntax: %s [-c] [-s server:port] [-t to_server:to_port] [-f from_server:from_port] [-k key] [-w client_reconn_time] [-b buffer_per_tunnel]\r\n", argv[0]);
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

