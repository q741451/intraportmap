#include "stdafx.h"

void slog::debug(const char* format, ...)
{
	va_list args;

	va_start(args, format);
	log("DEBUG", format, args);
	va_end(args);
}

void slog::info(const char* format, ...)
{
	va_list args;

	va_start(args, format);
	log("INFO ", format, args);
	va_end(args);
}

void slog::warn(const char* format, ...)
{
	va_list args;

	va_start(args, format);
	log("WARN ", format, args);
	va_end(args);
}

void slog::error(const char* format, ...)
{
	va_list args;

	va_start(args, format);
	log("ERROR", format, args);
	va_end(args);
}

bool slog::log(const char* cTitile, const char* format, va_list pargs)
{
	unsigned int nLen;
	unsigned int nUseLen;
	char *cLog = NULL;
	bool ret = false;
	va_list args;
	time_t time_seconds = time(NULL);
	struct tm now_time;

	va_copy(args, pargs);
	nLen = vsnprintf(NULL, 0, format, args);
	va_end(args);

	nLen += (unsigned int)(strlen("%s%08X ") + strlen(cTitile) + 0x10 + 0x10);

	cLog = (char*)malloc(nLen);

#ifdef _WIN32
	localtime_s(&now_time, &time_seconds);
	sprintf_s(cLog, nLen, "%02u%02u%02u%02u%02u %s %08X ", now_time.tm_mon + 1, now_time.tm_mday, now_time.tm_hour, now_time.tm_min, now_time.tm_sec, cTitile, GetCurrentThreadId());
#else
	localtime_r(&time_seconds, &now_time);
	sprintf(cLog, "%02u%02u%02u%02u%02u %s %08X ", now_time.tm_mon + 1, now_time.tm_mday, now_time.tm_hour, now_time.tm_min, now_time.tm_sec, cTitile, (unsigned int)syscall(SYS_gettid));
#endif
	
	nUseLen = (unsigned int)strlen(cLog);

	va_copy(args, pargs);
	vsnprintf(cLog + nUseLen, nLen - nUseLen, format, args);
	va_end(args);

	printf(cLog);

	ret = true;
// end:
	if (cLog) free(cLog);
	return ret;
}
