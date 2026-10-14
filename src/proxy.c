/* RicochetFix - forwards every winmm.dll / ddraw.dll export to the real
 * system DLL. Which one depends on the name RicochetFix was installed as. */
#include "rf.h"
#include <stdio.h>
#include "proxy_names.h"

void *g_proxyProcs[PROXY_COUNT];

static int __cdecl MissingExport(void)
{
    return 0;
}

int RF_LoadRealDll(HMODULE self)
{
    char path[MAX_PATH], sysdir[MAX_PATH], name[MAX_PATH], *base, *dot;
    HMODULE real = NULL;
    UINT len;
    int i, missing = 0;

    if (!GetModuleFileNameA(self, name, MAX_PATH))
        return 0;
    name[MAX_PATH - 1] = 0;
    base = strrchr(name, '\\');
    base = base ? base + 1 : name;
    dot = strrchr(base, '.');
    if (dot)
        *dot = 0;

    /* Always load the real DLL by full path from the system folder, never by
     * name (that would find ourselves again). Bounded: no overflow even if
     * someone gives the DLL a very long name. */
    len = GetSystemDirectoryA(sysdir, MAX_PATH);
    if (len && len < MAX_PATH &&
        snprintf(path, sizeof(path), "%s\\%s.dll", sysdir, base) < (int)sizeof(path))
        real = LoadLibraryA(path);
    else
        lstrcpyA(path, "(path too long)");
    RF_Log("Installed as %s.dll, real DLL %s -> %p", base, path, (void *)real);

    for (i = 0; i < PROXY_COUNT; i++) {
        void *p = NULL;
        if (real && !lstrcmpiA(g_proxyExports[i].dll, base))
            p = (void *)GetProcAddress(real, g_proxyExports[i].name);
        if (!p) {
            p = (void *)MissingExport;
            if (!lstrcmpiA(g_proxyExports[i].dll, base))
                missing++;
        }
        g_proxyProcs[i] = p;
    }
    if (missing)
        RF_Log("(%d exports of the real DLL are not present on this Windows version)", missing);
    return real != NULL;
}
