/* RicochetFix - log file (RicochetFix.log next to the DLL). */
#include "rf.h"
#include <stdio.h>
#include <stdarg.h>

static HANDLE g_logFile = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_logLock;

void RF_LogOpen(void)
{
    char path[MAX_PATH];
    if (!g_cfg.log)
        return;
    InitializeCriticalSection(&g_logLock);
    lstrcpynA(path, g_rfDir, MAX_PATH - 20);
    lstrcatA(path, "RicochetFix.log");
    g_logFile = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

void RF_Log(const char *fmt, ...)
{
    char buf[1024];
    int n;
    DWORD written;
    SYSTEMTIME t;
    va_list ap;

    if (g_logFile == INVALID_HANDLE_VALUE)
        return;
    GetLocalTime(&t);
    n = snprintf(buf, sizeof(buf), "[%02d:%02d:%02d.%03d] ",
                 t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_start(ap, fmt);
    n += vsnprintf(buf + n, sizeof(buf) - n - 3, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof(buf) - 3)
        n = sizeof(buf) - 3;
    buf[n++] = '\r';
    buf[n++] = '\n';

    EnterCriticalSection(&g_logLock);
    WriteFile(g_logFile, buf, n, &written, NULL);
    FlushFileBuffers(g_logFile);
    LeaveCriticalSection(&g_logLock);
}
