#if defined(TRACE)
#undef TRACE
#endif

#ifdef _DEBUG

#ifndef _WINDOWS_H
#include <windows.h>
#endif

#include <stdlib.h>

inline void _trace_(LPCTSTR lpFormat,...)
{
  char szBuffer[10240];
  va_list args;
  va_start(args, lpFormat);
  wvsprintf(szBuffer, lpFormat, args);
  OutputDebugString(szBuffer);
}

#define TRACE _trace_
#else
#define TRACE (void)
#endif