/* RicochetFix - entry point.
 *
 * Loaded by Windows as winmm.dll (or ddraw.dll) from the game folder. Finds
 * the module that holds the Ricochet engine and hooks it:
 *
 *   Ricochet Infinity ............ Data3.bin (loaded by RicochetInfinity.exe)
 *   Ricochet Lost Worlds ......... Ricochet.exe itself
 *   Ricochet Lost Worlds Recharged Data3.bin (loaded by RicochetRecharged.exe)
 *
 * The engine is recognised by what it does (imports DDRAW!DirectDrawCreate),
 * not by file name or hardcoded addresses, so all builds share one code path.
 */
#include "rf.h"

int RF_LoadRealDll(HMODULE self);

static HMODULE g_self;
static HMODULE g_engine;
static HMODULE (WINAPI *o_LoadLibraryA)(LPCSTR);
static HMODULE (WINAPI *o_LoadLibraryExA)(LPCSTR, HANDLE, DWORD);

static int IsEngine(HMODULE m)
{
    return m && m != g_self && RF_ModuleImports(m, "ddraw.dll", "DirectDrawCreate");
}

static void HookEngine(HMODULE m)
{
    char path[MAX_PATH];
    if (g_engine)
        return;
    g_engine = m;
    GetModuleFileNameA(m, path, MAX_PATH);
    RF_Log("Engine found in %s", path);
    RF_HookDirectDraw(m);
    RF_HookWindowFunctions(m);
}

static void CheckModule(HMODULE m, const char *name)
{
    const char *base;
    if (!m || m == g_self)
        return;
    if (!g_engine && IsEngine(m)) {
        HookEngine(m);
        return;
    }
    base = strrchr(name, '\\');
    base = base ? base + 1 : name;
    /* Store / trial overlays draw into the game picture and use the mouse. */
    if (!lstrcmpiA(base, "ReflexiveArcade.dll")) {
        RF_Log("Hooking %s", base);
        RF_HookAuxModule(m);
    }
}

static void HookLoaders(HMODULE m);

static HMODULE WINAPI hk_LoadLibraryA(LPCSTR name)
{
    HMODULE m = o_LoadLibraryA(name);
    if (m && name) {
        CheckModule(m, name);
        HookLoaders(m);
    }
    return m;
}

static HMODULE WINAPI hk_LoadLibraryExA(LPCSTR name, HANDLE file, DWORD flags)
{
    HMODULE m = o_LoadLibraryExA(name, file, flags);
    if (m && name && !(flags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE |
                                0x40 /* LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE */))) {
        CheckModule(m, name);
        HookLoaders(m);
    }
    return m;
}

/* Watch the game's own LoadLibrary calls so we see Data3.bin and
 * ReflexiveArcade.dll even if they load after us. */
static void HookLoaders(HMODULE m)
{
    if (!m || m == g_self)
        return;
    RF_PatchIAT(m, "kernel32.dll", "LoadLibraryA", hk_LoadLibraryA, (void **)&o_LoadLibraryA);
    RF_PatchIAT(m, "kernel32.dll", "LoadLibraryExA", hk_LoadLibraryExA, (void **)&o_LoadLibraryExA);
}

static void SetDpiAware(void)
{
    HMODULE user32 = GetModuleHandleA("user32.dll");
    BOOL (WINAPI *ctx)(HANDLE) = (BOOL (WINAPI *)(HANDLE))GetProcAddress(user32, "SetProcessDpiAwarenessContext");
    BOOL (WINAPI *old)(void) = (BOOL (WINAPI *)(void))GetProcAddress(user32, "SetProcessDPIAware");
    /* Per-monitor aware: otherwise Windows bitmap-stretches the whole window
     * on high-DPI screens and the borderless window would be the wrong size. */
    if (ctx && ctx((HANDLE)-4 /* PER_MONITOR_AWARE_V2 */))
        RF_Log("DPI awareness: per-monitor v2");
    else if (old && old())
        RF_Log("DPI awareness: system");
}

static void LogGameVersion(void)
{
    char exe[MAX_PATH];
    DWORD dummy, size;
    GetModuleFileNameA(NULL, exe, MAX_PATH);
    RF_Log("Game: %s", exe);
    size = GetFileVersionInfoSizeA(exe, &dummy);
    if (size) {
        void *buf = HeapAlloc(GetProcessHeap(), 0, size);
        char *desc, *ver;
        UINT len;
        if (buf && GetFileVersionInfoA(exe, 0, size, buf) &&
            VerQueryValueA(buf, "\\StringFileInfo\\040904b0\\FileDescription", (void **)&desc, &len) &&
            VerQueryValueA(buf, "\\StringFileInfo\\040904b0\\FileVersion", (void **)&ver, &len))
            RF_Log("Version info: %s %s", desc, ver);
        if (buf)
            HeapFree(GetProcessHeap(), 0, buf);
    }
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    char *slash;
    HMODULE data3;
    (void)reserved;

    if (reason != DLL_PROCESS_ATTACH)
        return TRUE;

    g_self = inst;
    DisableThreadLibraryCalls(inst);
    GetModuleFileNameA(inst, g_rfDir, MAX_PATH);
    slash = strrchr(g_rfDir, '\\');
    if (slash)
        slash[1] = 0;

    RF_LoadConfig();
    RF_LogOpen();
    RF_Log("RicochetFix " RF_VERSION " - https://github.com/Tubzbuster/RicochetFix");
    if (!RF_LoadRealDll(inst))
        RF_Log("ERROR: could not load the real system DLL");

    if (g_cfg.mode == MODE_OFF) {
        RF_Log("Mode=off: passing everything straight through");
        return TRUE;
    }
    LogGameVersion();
    RF_ReadGameResolution();
    SetDpiAware();

    /* Lost Worlds: the engine is the exe. Infinity / Recharged: Data3.bin is
     * usually already loaded (we are one of its imports). */
    CheckModule(GetModuleHandleA(NULL), "game.exe");
    data3 = GetModuleHandleA("Data3.bin");
    if (data3)
        CheckModule(data3, "Data3.bin");
    HookLoaders(GetModuleHandleA(NULL));
    if (data3)
        HookLoaders(data3);
    if (g_engine)
        HookLoaders(g_engine);
    if (!g_engine)
        RF_Log("Engine not loaded yet - waiting for the game to load it");
    return TRUE;
}
