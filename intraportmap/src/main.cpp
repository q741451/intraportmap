#include "stdafx.h"

int main(int argc, char* argv[])
{
	int ret = -1;
	intraportmap ipm;

	if (argc < 2)
	{
		printf("syntax: %s [-c] [-w client_reconn_time] [-s server:port] [-t to_server:to_port] [-f from_server:from_port]\r\n", argv[0]);
		goto end;
	}

#ifdef _WIN32
	{
		WSADATA WSAData;
		WSAStartup(0x101, &WSAData);
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

