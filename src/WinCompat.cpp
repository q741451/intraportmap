// Windows 下 libevent 的 evutil_inet_pton_scope() 会无条件调用
// if_nametoindex()，用来把 IPv6 地址里 "fe80::1%eth0" 这种网卡名后缀转成
// 序号。这个函数由 IPHLPAPI.DLL 导出，但它是 Vista 才加入的，XP 上没有。
//
// PE 的导入是加载期解析的：只要导入表里留着这个名字，XP 上进程根本起不来，
// 哪怕运行时压根走不到那段代码。
//
// 静态链接按目标文件为粒度按需拉取，这里先给出定义，链接器就不会再去
// libiphlpapi.a 里取那个导入桩，IPHLPAPI.DLL 也就整条从导入表消失。改成
// 运行时查找后，新系统上功能不变，XP 上自动退化成解析不出网卡名——libevent
// 会回退去按数字解析，"fe80::1%3" 这种写法仍然可用。
//
// 只对 MinGW 生效。MSVC 走的是自己的一套导入库，在那边给出同名定义可能与
// iphlpapi.lib 撞成重复符号，而那条构建路径这里验证不到，不去碰它。
//
// 调用点只在解析带 '%' 的地址时命中，频率极低，所以不做缓存，省掉一份
// 需要考虑初始化时序的全局状态。
#if defined(_WIN32) && defined(__MINGW32__)

#include <windows.h>

extern "C" unsigned int WINAPI if_nametoindex(const char* name)
{
	typedef unsigned int (WINAPI *pfn_if_nametoindex)(const char*);
	HMODULE hmod = NULL;
	pfn_if_nametoindex pfn = NULL;
	unsigned int index = 0;

	hmod = LoadLibraryA("iphlpapi.dll");
	if (hmod == NULL)
		return 0;

	pfn = (pfn_if_nametoindex)GetProcAddress(hmod, "if_nametoindex");
	if (pfn != NULL)
		index = pfn(name);

	FreeLibrary(hmod);
	return index;
}

#endif
