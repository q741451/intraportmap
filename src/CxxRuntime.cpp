#include <cstdlib>

// libstdc++ 默认的 terminate 处理器会打印当前异常的类型名，为此要调用
// __cxa_demangle 还原 C++ 符号。静态链接时这一条依赖会把整个 demangler
// 连同它的数据表一起拉进来，musl x86_64 上实测占 48KB。
//
// 静态库按目标文件为粒度按需拉取，而这个处理器在 libstdc++ 里独占
// vterminate.o，所以只要这里先给出定义，那个目标文件就不会被链接，
// demangler 也就整块消失。
//
// 本项目全程不使用异常（构建时也带着 -fno-exceptions），走到 terminate
// 只可能是内存耗尽一类无法继续的情况，直接 abort 即可，没有需要打印的
// 异常类型。
//
// 只接管这一个符号。libstdc++ 的 std::__throw_* 系列同样会牵出异常展开
// 代码，但它们共处一个 functexcept.o：只定义其中一部分的话，一旦某个
// 目标平台的 libstdc++ 需要未被定义的那个，该目标文件仍会被拉入，与这里
// 的定义撞成 multiple definition。
//
// __GLIBCXX__ 由上面的头文件在使用 libstdc++ 时定义。macOS 的 libc++ 和
// MSVC 的标准库都没有这个处理器，那里整个翻译单元为空，不影响构建。
#if defined(__GLIBCXX__)

namespace __gnu_cxx
{
	void __verbose_terminate_handler()
	{
		std::abort();
	}
}

#endif
