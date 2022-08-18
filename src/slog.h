#ifndef _SLOG_H
#define _SLOG_H

#ifdef _WIN32
#define slog_debug(fmt, ...)	slog::debug("[%s\t%s\t%d]\t" ##fmt"\r\n",		__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#define slog_info(fmt, ...)		slog::info("[%s\t%s\t%d]\t" ##fmt"\r\n",		__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#define slog_warn(fmt, ...)		slog::warn("[%s\t%s\t%d]\t" ##fmt"\r\n",		__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#define slog_error(fmt, ...)	slog::error("[%s\t%s\t%d]\t" ##fmt"\r\n",		__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#else
#define slog_debug(fmt, ...)	slog::debug("[%s\t%s\t%d]\t" fmt"\r\n",		__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#define slog_info(fmt, ...)		slog::info("[%s\t%s\t%d]\t" fmt"\r\n",		__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#define slog_warn(fmt, ...)		slog::warn("[%s\t%s\t%d]\t" fmt"\r\n",		__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#define slog_error(fmt, ...)	slog::error("[%s\t%s\t%d]\t" fmt"\r\n",		__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#endif

namespace slog
{
	void debug(const char* format, ...);

	void info(const char* format, ...);

	void warn(const char* format, ...);

	void error(const char* format, ...);

	bool log(const char* cTitile, const char* format, va_list pargs);
}

#endif

