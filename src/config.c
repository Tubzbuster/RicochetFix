/* RicochetFix - settings (RicochetFix.ini) and the game's own .CFG file. */
#include "rf.h"

RFConfig g_cfg;
char g_rfDir[MAX_PATH];

static int ReadChoice(const char *ini, const char *sec, const char *key,
                      const char *def, const char *const *names, int count)
{
    char val[64];
    int i;
    GetPrivateProfileStringA(sec, key, def, val, sizeof(val), ini);
    for (i = 0; i < count; i++)
        if (!lstrcmpiA(val, names[i]))
            return i;
    for (i = 0; i < count; i++)
        if (!lstrcmpiA(def, names[i]))
            return i;
    return 0;
}

void RF_LoadConfig(void)
{
    static const char *const modes[] = { "off", "borderless", "windowed" };
    static const char *const scales[] = { "smooth", "sharp", "integer", "stretch" };
    char ini[MAX_PATH];

    lstrcpynA(ini, g_rfDir, MAX_PATH - 20);
    lstrcatA(ini, "RicochetFix.ini");

    g_cfg.mode        = ReadChoice(ini, "Display", "Mode", "borderless", modes, 3);
    g_cfg.scaling     = ReadChoice(ini, "Display", "Scaling", "smooth", scales, 4);
    g_cfg.windowScale = GetPrivateProfileIntA("Display", "WindowScale", 0, ini);
    g_cfg.vsync       = GetPrivateProfileIntA("Display", "VSync", 1, ini);
    g_cfg.clipCursor  = GetPrivateProfileIntA("Display", "ClipCursor", 1, ini);
    g_cfg.altEnter    = GetPrivateProfileIntA("Display", "AltEnterToggle", 1, ini);
    g_cfg.log         = GetPrivateProfileIntA("Debug", "Log", 1, ini);
}

/* The engine reads "<Game Name>.CFG" from the current directory, e.g.
 * "Ricochet Infinity.CFG" with [Options] Screen Width / Screen Height.
 * Testing showed the engine always renders 800x600, but read it anyway so
 * the scaler knows the size if another build does honour it. */
void RF_ReadGameResolution(void)
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA("*.CFG", &fd);
    char path[MAX_PATH];

    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        int w, hgt;
        if (!GetFullPathNameA(fd.cFileName, MAX_PATH, path, NULL))
            continue;
        w = GetPrivateProfileIntA("Options", "Screen Width", 0, path);
        hgt = GetPrivateProfileIntA("Options", "Screen Height", 0, path);
        if (w > 0 && hgt > 0) {
            RF_Log("Game config %s: Screen Width=%d Screen Height=%d (engine renders 800x600 regardless)",
                   fd.cFileName, w, hgt);
            break;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}
