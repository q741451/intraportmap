#include "stdafx.h"

void util::string_split(const char* str, const char* splits, std::vector<std::string>& result, bool rmempty /*= false*/)
{
	const char* cur = str;
	const char* cur_split = NULL;
	size_t szSplit = strlen(splits);
	std::string result_a;

	for (; (cur_split = strstr(cur, splits)) != NULL; cur += szSplit)
	{
		result_a.assign(cur, cur_split - cur);
		if (result_a.size() > 0 || rmempty == false)
			result.push_back(result_a);
		cur = cur_split;
	}
	result_a.assign(cur);
	if (result_a.size() > 0 || rmempty == false)
		result.push_back(result_a);
}

bool util::split_addr_string(const char* addrfull, std::string& addrstr, std::string& portstr)
{
	bool ret = false;
	std::vector<std::string> strs;

	string_split(addrfull, ":", strs, false);

	if (strs.size() != 2)
		goto end;

	addrstr = strs[0];
	portstr = strs[1];

	ret = true;
end:
	return ret;
}

