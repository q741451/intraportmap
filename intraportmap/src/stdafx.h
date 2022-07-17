#ifndef _STDAFX_H
#define _STDAFX_H

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include "getopt/getopt.h"
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <vector>
#include <string>

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/dns.h>

#include "util.h"
#include "intraportmap.h"

#endif
