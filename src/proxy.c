/* RicochetFix - forwards every winmm.dll / ddraw.dll export to the real
 * system DLL. Which one depends on the name RicochetFix was installed as. */
#include "rf.h"
#include "proxy_names.h"

void *g_proxyProcs[PROXY_COUNT];

static int __cdecl MissingExport(void)
{
    return 0;
}

int RF_LoadRealDll(HMODULE self)
{
    char path[MAX_PATH], name[MAX_PATH], *base, *dot;
    HMODULE real;
    int i, missing = 0;

    GetModuleFileNameA(self, name, MAX_PATH);
    base = strrchr(name, '\\');
    base = base ? base + 1 : name;
    dot = strrchr(base, '.');
    if (dot)
        *dot = 0;

    GetSystemDirectoryA(path, MAX_PATH - 32);
    lstrcatA(path, "\\");
    lstrcatA(path, base);
    lstrcatA(path, ".dll");
    real = LoadLibraryA(path);
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
