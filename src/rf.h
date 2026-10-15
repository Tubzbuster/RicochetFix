/* RicochetFix - shared declarations. */
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>

#define RF_VERSION "0.2.0"

/* ---- config.c ---------------------------------------------------------- */
enum { MODE_OFF, MODE_BORDERLESS, MODE_WINDOWED };
enum { SCALE_SMOOTH, SCALE_SHARP, SCALE_INTEGER, SCALE_STRETCH };
enum { BORDER_BLACK, BORDER_AMBIENT };

typedef struct {
    int mode;           /* MODE_* */
    int scaling;        /* SCALE_* */
    int border;         /* BORDER_*: what fills the bars beside the picture */
    int windowScale;    /* windowed size multiplier, 0 = largest that fits */
    int vsync;          /* wait for DWM after page flips */
    int clipCursor;     /* keep the mouse inside the picture (borderless) */
    int altEnter;       /* Alt+Enter toggles borderless <-> windowed */
    int log;
} RFConfig;

extern RFConfig g_cfg;
extern char g_rfDir[MAX_PATH];  /* folder RicochetFix was loaded from, with trailing '\' */
void RF_LoadConfig(void);
void RF_ReadGameResolution(void);

/* ---- log.c ------------------------------------------------------------- */
void RF_LogOpen(void);
void RF_Log(const char *fmt, ...);
void RF_LogClose(void);

/* ---- iat.c ------------------------------------------------------------- */
int RF_PatchIAT(HMODULE mod, const char *dll, const char *func, void *hook, void **orig);
int RF_ModuleImports(HMODULE mod, const char *dll, const char *func);
void RF_PatchPtr(void **slot, void *value);

/* ---- window.c ---------------------------------------------------------- */
extern HWND g_hwnd;           /* the game's main window */
extern int  g_gameW, g_gameH; /* the game's internal resolution (800x600) */
void RF_HookWindowFunctions(HMODULE engine);
void RF_HookAuxModule(HMODULE mod);
void RF_AttachWindow(HWND hwnd);
void RF_SetGameSize(int w, int h);
void RF_Present(IDirectDrawSurface *src);
void RF_MarkPending(IDirectDrawSurface *src);
void RF_FlushPending(void);
void RF_ForgetSurface(IDirectDrawSurface *s);
void RF_WaitVSync(void);

/* ---- border.c ---------------------------------------------------------- */
void RF_BorderDraw(HDC wdc, HDC sdc, int gameW, int gameH, const RECT *view, int cw, int ch);
void RF_BorderInvalidate(void);
void RF_BorderFree(void);
void RF_BorderStats(double *avgMs, double *maxMs, unsigned long *updates);

/* ---- ddraw_hooks.c ----------------------------------------------------- */
void RF_HookDirectDraw(HMODULE engine);
HRESULT RF_SurfGetDC(IDirectDrawSurface *s, HDC *dc);       /* bypass our hooks */
HRESULT RF_SurfReleaseDC(IDirectDrawSurface *s, HDC dc);
