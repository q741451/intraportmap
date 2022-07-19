#ifndef _IPM_TYPES_H
#define _IPM_TYPES_H

#ifdef WIN32
#pragma pack(1)
#endif
typedef struct _alloc_agent_package
{
	unsigned long long alloc_zero;
	unsigned int is_ipv6;
	char ip[16];
	unsigned int port;
	unsigned int checksum;
}
#ifndef WIN32
__attribute__((packed))
#endif
alloc_agent_package_t;
#ifdef WIN32
#pragma pack()
#endif

#endif

