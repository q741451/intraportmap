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

class addr_pkg_idx
{
public:
	addr_pkg_idx()
	{
		memset(&addr_pkg, 0, sizeof(address_package_t));
	}
	address_package_t addr_pkg;
};

#endif

