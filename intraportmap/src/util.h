#ifndef _UTIL_H
#define _UTIL_H

class util
{
public:
	static void string_split(const char* str, const char* splits, std::vector<std::string>& result, bool rmempty = false);
	static bool split_addr_string(const char *addrfull, std::string &addrstr, std::string& portstr);
};

#endif
