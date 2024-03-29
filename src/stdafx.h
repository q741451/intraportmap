#ifndef _STDAFX_H
#define _STDAFX_H

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include "getopt/getopt.h"
#else
#include <sys/socket.h>
#include <sys/syscall.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#define VERBOSE_DEBUG 0
#define DEF_MAX_BUFFER (1024 * 1024)

#include <signal.h>
#include <memory.h>

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <set>

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/dns.h>

#include "slog.h"
#include "crc32.h"
#include "util.h"
#include "ipm_types.h"
#include "ipm_server_tunnel.h"
#include "ipm_server_agent.h"
#include "ipm_server.h"
#include "ipm_client_tunnel.h"
#include "ipm_client.h"
#include "intraportmap.h"

#endif
