/* RicochetFix - window placement, scaling and mouse mapping.
 *
 * The engine keeps believing it owns an 800x600 window: GetClientRect on the
 * main window reports the game size, ClientToScreen is the identity (so its
 * windowed-mode blits land at 0,0 of our virtual primary surface), and mouse
 * messages are converted from real window pixels back into game pixels.
 * Meanwhile the real window is borderless/fullscreen (or a resizable window)
 * and each finished frame is scaled into it with GDI.
 */
#include "rf.h"

HWND g_hwnd;
int  g_gameW = 800, g_gameH = 600;

static WNDPROC g_gameProc;     /* the engine's own window procedure */
static int     g_curMode;      /* MODE_* currently in use (Alt+Enter changes it) */
static RECT    g_view;         /* where the picture goes, in client pixels */
static int     g_clientW, g_clientH;
static int     g_active;
static int     g_placing;      /* inside our own SetWindowPos */
static int     g_clipped;      /* we confined the cursor (so we must release it) */

static IDirectDrawSurface *g_pending;  /* frame waiting to be shown */
static IDirectDrawSurface *g_last;     /* last frame shown (for repaints) */
static DWORD   g_lastPresent;

/* ---- layout ------------------------------------------------------------ */

static void UpdateLayout(void)
{
    RECT rc;
    int cw, ch, vw, vh;

    if (!g_hwnd || !GetClientRect(g_hwnd, &rc))
        return;
    cw = rc.right - rc.left;
    ch = rc.bottom - rc.top;
    if (cw <= 0 || ch <= 0)
        return;  /* minimised */
    g_clientW = cw;
    g_clientH = ch;

    if (g_cfg.scaling == SCALE_STRETCH) {
        vw = cw;
        vh = ch;
    } else if (g_cfg.scaling == SCALE_INTEGER && cw >= g_gameW && ch >= g_gameH) {
        int s = cw / g_gameW < ch / g_gameH ? cw / g_gameW : ch / g_gameH;
        vw = g_gameW * s;
        vh = g_gameH * s;
    } else if ((double)cw / ch > (double)g_gameW / g_gameH) {
        vh = ch;
        vw = MulDiv(ch, g_gameW, g_gameH);
    } else {
        vw = cw;
        vh = MulDiv(cw, g_gameH, g_gameW);
    }
    g_view.left = (cw - vw) / 2;
    g_view.top = (ch - vh) / 2;
    g_view.right = g_view.left + vw;
    g_view.bottom = g_view.top + vh;
}

static void UpdateCursorClip(void)
{
    RECT r;
    POINT a, b;

    if (!g_hwnd)
        return;
    if (!g_cfg.clipCursor || g_curMode != MODE_BORDERLESS || !g_active || IsIconic(g_hwnd)) {
        /* Only undo a clip we made (or any clip once we lose focus), so a
         * clip the game set itself survives when ClipCursor=0. */
        if (g_clipped || !g_active)
            ClipCursor(NULL);
        g_clipped = 0;
        return;
    }
    a.x = g_view.left;  a.y = g_view.top;
    b.x = g_view.right; b.y = g_view.bottom;
    ClientToScreen(g_hwnd, &a);
    ClientToScreen(g_hwnd, &b);
    SetRect(&r, a.x, a.y, b.x, b.y);
    ClipCursor(&r);
    g_clipped = 1;
}

static void ComputePlacement(DWORD *style, DWORD *exStyle, RECT *r)
{
    HMONITOR mon;
    MONITORINFO mi;

    mon = g_hwnd ? MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTOPRIMARY)
                 : MonitorFromPoint((POINT){ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    mi.cbSize = sizeof(mi);
    GetMonitorInfoA(mon, &mi);

    if (g_curMode == MODE_BORDERLESS) {
        *style = WS_POPUP | WS_VISIBLE | WS_SYSMENU | WS_MINIMIZEBOX;
        *exStyle = WS_EX_APPWINDOW;
        *r = mi.rcMonitor;
    } else {
        int workW = mi.rcWork.right - mi.rcWork.left;
        int workH = mi.rcWork.bottom - mi.rcWork.top;
        int cw, ch, w, h;
        RECT a;

        if (g_cfg.windowScale > 0) {
            cw = g_gameW * g_cfg.windowScale;
            ch = g_gameH * g_cfg.windowScale;
        } else {
            /* biggest 4:3 window that fills ~85% of the screen */
            ch = workH * 85 / 100;
            cw = MulDiv(ch, g_gameW, g_gameH);
            if (cw > workW * 85 / 100) {
                cw = workW * 85 / 100;
                ch = MulDiv(cw, g_gameH, g_gameW);
            }
            if (cw < g_gameW || ch < g_gameH) {
                cw = g_gameW;
                ch = g_gameH;
            }
        }
        *style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        *exStyle = WS_EX_APPWINDOW;
        SetRect(&a, 0, 0, cw, ch);
        AdjustWindowRectEx(&a, *style, FALSE, *exStyle);
        w = a.right - a.left;
        h = a.bottom - a.top;
        r->left = mi.rcWork.left + (workW - w) / 2;
        r->top = mi.rcWork.top + (workH - h) / 2;
        if (r->top < mi.rcWork.top)
            r->top = mi.rcWork.top;
        r->right = r->left + w;
        r->bottom = r->top + h;
    }
}

static void ApplyPlacement(int force)
{
    DWORD style, exStyle;
    RECT r, cur;

    if (!g_hwnd)
        return;
    ComputePlacement(&style, &exStyle, &r);
    GetWindowRect(g_hwnd, &cur);
    if (!force) {
        /* Windows adds bits like WS_CLIPSIBLINGS; only compare the frame type */
        const DWORD frame = WS_POPUP | WS_CAPTION | WS_THICKFRAME;
        int sameFrame = ((DWORD)GetWindowLongA(g_hwnd, GWL_STYLE) & frame) == (style & frame);
        if (sameFrame && (g_curMode == MODE_WINDOWED || EqualRect(&cur, &r)))
            return;  /* windowed: keep wherever/whatever size the player made it */
    }

    RF_Log("Placing window: %s %ldx%ld at %ld,%ld",
           g_curMode == MODE_BORDERLESS ? "borderless" : "windowed",
           r.right - r.left, r.bottom - r.top, r.left, r.top);
    g_placing = 1;
    SetWindowLongA(g_hwnd, GWL_STYLE, style);
    SetWindowLongA(g_hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(g_hwnd, HWND_NOTOPMOST, r.left, r.top, r.right - r.left, r.bottom - r.top,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    g_placing = 0;
    UpdateLayout();
    UpdateCursorClip();
    InvalidateRect(g_hwnd, NULL, FALSE);
}

void RF_SetGameSize(int w, int h)
{
    if (w <= 0 || h <= 0 || (w == g_gameW && h == g_gameH))
        return;
    RF_Log("Game resolution is %dx%d", w, h);
    g_gameW = w;
    g_gameH = h;
    ApplyPlacement(1);
}

/* ---- presenting -------------------------------------------------------- */

static void FillBars(HDC dc)
{
    HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RECT r;
    if (g_view.top > 0)       { SetRect(&r, 0, 0, g_clientW, g_view.top); FillRect(dc, &r, black); }
    if (g_view.bottom < g_clientH) { SetRect(&r, 0, g_view.bottom, g_clientW, g_clientH); FillRect(dc, &r, black); }
    if (g_view.left > 0)      { SetRect(&r, 0, g_view.top, g_view.left, g_view.bottom); FillRect(dc, &r, black); }
    if (g_view.right < g_clientW) { SetRect(&r, g_view.right, g_view.top, g_clientW, g_view.bottom); FillRect(dc, &r, black); }
}

/* Once a minute: frames shown and GDI/USER handle counts. Steady numbers
 * over a long session mean nothing is leaking. */
static void LogHealth(void)
{
    static DWORD frames, since;
    DWORD now = GetTickCount();
    frames++;
    if (!since)
        since = now;
    if (now - since >= 60000) {
        HANDLE me = GetCurrentProcess();
        RF_Log("Health: %lu frames in the last minute, GDI objects %lu, USER objects %lu",
               frames, GetGuiResources(me, GR_GDIOBJECTS), GetGuiResources(me, GR_USEROBJECTS));
        frames = 0;
        since = now;
    }
}

void RF_Present(IDirectDrawSurface *src)
{
    HDC sdc, wdc;
    int vw, vh;

    if (!g_hwnd || !src || IsIconic(g_hwnd))
        return;
    if (!g_clientW)
        UpdateLayout();
    if (RF_SurfGetDC(src, &sdc) != DD_OK)
        return;
    wdc = GetDC(g_hwnd);
    if (wdc) {
        vw = g_view.right - g_view.left;
        vh = g_view.bottom - g_view.top;
        FillBars(wdc);
        if (vw == g_gameW && vh == g_gameH) {
            BitBlt(wdc, g_view.left, g_view.top, vw, vh, sdc, 0, 0, SRCCOPY);
        } else {
            if (g_cfg.scaling == SCALE_SMOOTH) {
                SetStretchBltMode(wdc, HALFTONE);
                SetBrushOrgEx(wdc, 0, 0, NULL);
            } else {
                SetStretchBltMode(wdc, COLORONCOLOR);
            }
            StretchBlt(wdc, g_view.left, g_view.top, vw, vh, sdc, 0, 0, g_gameW, g_gameH, SRCCOPY);
        }
        ReleaseDC(g_hwnd, wdc);
    }
    RF_SurfReleaseDC(src, sdc);
    g_last = src;
    g_lastPresent = GetTickCount();
    LogHealth();
}

void RF_MarkPending(IDirectDrawSurface *src)
{
    g_pending = src;
    /* Safety net in case the engine stops pumping messages for a while. */
    if (GetTickCount() - g_lastPresent > 50)
        RF_FlushPending();
}

void RF_FlushPending(void)
{
    IDirectDrawSurface *s = g_pending;
    if (s) {
        g_pending = NULL;
        RF_Present(s);
    }
}

void RF_ForgetSurface(IDirectDrawSurface *s)
{
    if (g_pending == s) g_pending = NULL;
    if (g_last == s)    g_last = NULL;
}

void RF_WaitVSync(void)
{
    static HRESULT (WINAPI *pDwmFlush)(void);
    static int tried;
    if (!g_cfg.vsync)
        return;
    if (!tried) {
        HMODULE dwm = LoadLibraryA("dwmapi.dll");
        tried = 1;
        if (dwm)
            pDwmFlush = (HRESULT (WINAPI *)(void))GetProcAddress(dwm, "DwmFlush");
    }
    if (pDwmFlush)
        pDwmFlush();
}

/* ---- window procedure -------------------------------------------------- */

static LPARAM MapMouse(LPARAM lp)
{
    int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
    int vw = g_view.right - g_view.left, vh = g_view.bottom - g_view.top;
    if (vw <= 0 || vh <= 0)
        return lp;
    x = MulDiv(x - g_view.left, g_gameW, vw);
    y = MulDiv(y - g_view.top, g_gameH, vh);
    if (x < 0) x = 0; else if (x >= g_gameW) x = g_gameW - 1;
    if (y < 0) y = 0; else if (y >= g_gameH) y = g_gameH - 1;
    return MAKELPARAM(x, y);
}

static LRESULT CALLBACK RF_WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT r;

    switch (msg) {
    case WM_SIZE:
        if (wp != SIZE_MINIMIZED) {
            UpdateLayout();
            UpdateCursorClip();
            InvalidateRect(h, NULL, FALSE);
        }
        lp = MAKELPARAM(g_gameW, g_gameH);  /* the engine always sees its own size */
        break;

    case WM_MOVE:
        UpdateCursorClip();
        lp = 0;  /* client area is at 0,0 as far as the engine knows */
        break;

    case WM_ACTIVATE:
        g_active = LOWORD(wp) != WA_INACTIVE;
        UpdateCursorClip();
        break;

    case WM_ACTIVATEAPP:
        g_active = wp != 0;
        UpdateCursorClip();
        break;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        r = CallWindowProcA(g_gameProc, h, msg, wp, lp);
        ValidateRect(h, NULL);
        if (g_last)
            RF_Present(g_last);
        return r;

    case WM_DISPLAYCHANGE:
        /* desktop resolution changed: refit the borderless window */
        r = CallWindowProcA(g_gameProc, h, msg, wp, lp);
        if (g_curMode == MODE_BORDERLESS)
            ApplyPlacement(1);
        return r;

    case WM_DPICHANGED:
        /* moved to a monitor with different scaling (per-monitor DPI aware) */
        if (g_curMode == MODE_BORDERLESS) {
            ApplyPlacement(1);
        } else {
            const RECT *nr = (const RECT *)lp;
            SetWindowPos(h, NULL, nr->left, nr->top, nr->right - nr->left, nr->bottom - nr->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;

    case WM_DESTROY:
        ClipCursor(NULL);
        g_clipped = 0;
        break;

    case WM_NCDESTROY: {
        /* Forget the window: the engine may create a new one later, and a
         * destroyed handle can be reused by an unrelated window. */
        WNDPROC proc = g_gameProc;
        if (h == g_hwnd) {
            RF_Log("Game window destroyed");
            g_hwnd = NULL;
            g_gameProc = NULL;
            g_clientW = g_clientH = 0;
            g_pending = g_last = NULL;
        }
        return CallWindowProcA(proc, h, msg, wp, lp);
    }
    }
    return CallWindowProcA(g_gameProc, h, msg, wp, lp);
}

void RF_AttachWindow(HWND hwnd)
{
    if (g_hwnd || !hwnd)
        return;
    g_hwnd = hwnd;
    g_curMode = g_cfg.mode;
    g_gameProc = (WNDPROC)SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)RF_WndProc);
    g_active = GetForegroundWindow() == hwnd;
    RF_Log("Attached to game window %p", (void *)hwnd);
    ApplyPlacement(1);
}

/* ---- hooks for the engine module --------------------------------------- */

static HWND WINAPI hk_CreateWindowExA(DWORD ex, LPCSTR cls, LPCSTR name, DWORD style,
                                      int x, int y, int w, int h, HWND parent,
                                      HMENU menu, HINSTANCE inst, LPVOID param)
{
    HWND hwnd = CreateWindowExA(ex, cls, name, style, x, y, w, h, parent, menu, inst, param);
    if (hwnd && !g_hwnd && !parent && !(style & WS_CHILD)) {
        RF_Log("Game created its window (style %08lx ex %08lx, %dx%d)", style, ex, w, h);
        RF_AttachWindow(hwnd);
    }
    return hwnd;
}

static LONG WINAPI hk_SetWindowLongA(HWND hwnd, int idx, LONG val)
{
    if (hwnd && hwnd == g_hwnd) {
        if (idx == GWL_STYLE || idx == GWL_EXSTYLE)
            return GetWindowLongA(hwnd, idx);  /* we decide the window style */
        if (idx == GWL_WNDPROC) {
            LONG prev = (LONG)g_gameProc;
            g_gameProc = (WNDPROC)val;
            return prev;
        }
    }
    return SetWindowLongA(hwnd, idx, val);
}

static BOOL WINAPI hk_SetWindowPos(HWND hwnd, HWND after, int x, int y, int cx, int cy, UINT flags)
{
    if (hwnd && hwnd == g_hwnd && !g_placing) {
        UINT keep = flags & (SWP_SHOWWINDOW | SWP_HIDEWINDOW | SWP_NOACTIVATE);
        if (keep & (SWP_SHOWWINDOW | SWP_HIDEWINDOW))
            SetWindowPos(hwnd, NULL, 0, 0, 0, 0, keep | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
        if (!(flags & SWP_HIDEWINDOW))
            ApplyPlacement(0);
        return TRUE;
    }
    return SetWindowPos(hwnd, after, x, y, cx, cy, flags);
}

static BOOL WINAPI hk_GetClientRect(HWND hwnd, LPRECT r)
{
    if (hwnd && hwnd == g_hwnd && r) {
        SetRect(r, 0, 0, g_gameW, g_gameH);
        return TRUE;
    }
    return GetClientRect(hwnd, r);
}

static BOOL WINAPI hk_ClientToScreen_Engine(HWND hwnd, LPPOINT p)
{
    if (hwnd && hwnd == g_hwnd)
        return TRUE;  /* identity: see the comment at the top of this file */
    return ClientToScreen(hwnd, p);
}

static int IsMouseMessage(UINT m)
{
    return (m >= WM_MOUSEFIRST && m <= WM_MBUTTONDBLCLK) ||
           (m >= WM_XBUTTONDOWN && m <= WM_XBUTTONDBLCLK);
}

/* The engines read mouse messages straight out of their message loop (before
 * DispatchMessage), so coordinates are converted here, where they come out
 * of the queue. msg->pt is converted too: in the engine's world the screen
 * and its 800x600 client area are the same thing. */
static void ToggleMode(void)
{
    g_curMode = g_curMode == MODE_BORDERLESS ? MODE_WINDOWED : MODE_BORDERLESS;
    RF_Log("Alt+Enter: switching to %s", g_curMode == MODE_BORDERLESS ? "borderless" : "windowed");
    ApplyPlacement(1);
}

static void MapQueuedMessage(LPMSG msg, int removed)
{
    LPARAM m;
    POINT p;

    if (!g_hwnd || msg->hwnd != g_hwnd)
        return;
    /* Alt+Enter is ours: toggle borderless/windowed and hide the keypress
     * from the engine (it would otherwise also act on Enter). */
    if (g_cfg.altEnter && msg->wParam == VK_RETURN && (msg->lParam & (1 << 29)) &&
        (msg->message == WM_SYSKEYDOWN || msg->message == WM_SYSKEYUP || msg->message == WM_SYSCHAR)) {
        if (removed && msg->message == WM_SYSKEYDOWN && !(msg->lParam & (1 << 30)))
            ToggleMode();
        msg->message = WM_NULL;
        return;
    }
    if (IsMouseMessage(msg->message) && msg->message != WM_MOUSEWHEEL) {
        m = MapMouse(msg->lParam);
        static int logged;
        if ((msg->message == WM_LBUTTONDOWN || msg->message == WM_RBUTTONDOWN) && logged++ < 10)
            RF_Log("Click at window %d,%d -> game %d,%d", (short)LOWORD(msg->lParam),
                   (short)HIWORD(msg->lParam), (short)LOWORD(m), (short)HIWORD(m));
        msg->lParam = m;
        msg->pt.x = (short)LOWORD(m);
        msg->pt.y = (short)HIWORD(m);
    } else if (msg->message == WM_MOUSEWHEEL) {
        p.x = (short)LOWORD(msg->lParam);
        p.y = (short)HIWORD(msg->lParam);
        ScreenToClient(g_hwnd, &p);
        m = MapMouse(MAKELPARAM(p.x, p.y));
        msg->lParam = m;
        msg->pt.x = (short)LOWORD(m);
        msg->pt.y = (short)HIWORD(m);
    } else {
        p = msg->pt;
        ScreenToClient(g_hwnd, &p);
        m = MapMouse(MAKELPARAM(p.x, p.y));
        msg->pt.x = (short)LOWORD(m);
        msg->pt.y = (short)HIWORD(m);
    }
}

static BOOL WINAPI hk_PeekMessageA(LPMSG msg, HWND hwnd, UINT a, UINT b, UINT remove)
{
    BOOL r;
    RF_FlushPending();
    r = PeekMessageA(msg, hwnd, a, b, remove);
    if (r && msg)
        MapQueuedMessage(msg, (remove & PM_REMOVE) != 0);
    return r;
}

static BOOL WINAPI hk_GetMessageA(LPMSG msg, HWND hwnd, UINT a, UINT b)
{
    BOOL r;
    RF_FlushPending();
    r = GetMessageA(msg, hwnd, a, b);
    if (r > 0 && msg)
        MapQueuedMessage(msg, 1);
    return r;
}

/* Game pixels -> real screen pixels. */
static void GameToScreen(LPPOINT p)
{
    int vw = g_view.right - g_view.left, vh = g_view.bottom - g_view.top;
    p->x = g_view.left + MulDiv(p->x, vw, g_gameW);
    p->y = g_view.top + MulDiv(p->y, vh, g_gameH);
    ClientToScreen(g_hwnd, p);
}

/* The 2004 engines (Lost Worlds, Recharged) also move the window, read its
 * rectangle, warp/clip the mouse and minimise themselves on alt-tab. In the
 * engine's world the screen is its own 800x600 client area, so translate. */

static BOOL WINAPI hk_MoveWindow(HWND hwnd, int x, int y, int w, int h, BOOL repaint)
{
    if (hwnd && hwnd == g_hwnd) {
        ApplyPlacement(0);
        return TRUE;
    }
    return MoveWindow(hwnd, x, y, w, h, repaint);
}

static BOOL WINAPI hk_GetWindowRect(HWND hwnd, LPRECT r)
{
    if (hwnd && hwnd == g_hwnd && r) {
        SetRect(r, 0, 0, g_gameW, g_gameH);
        return TRUE;
    }
    return GetWindowRect(hwnd, r);
}

static BOOL WINAPI hk_SetCursorPos(int x, int y)
{
    POINT p;
    if (!g_hwnd)
        return SetCursorPos(x, y);
    p.x = x;
    p.y = y;
    GameToScreen(&p);
    return SetCursorPos(p.x, p.y);
}

static BOOL WINAPI hk_ClipCursor(const RECT *r)
{
    POINT a, b;
    RECT real;
    if (!g_hwnd)
        return ClipCursor(r);
    if (!r) {
        UpdateCursorClip();  /* our own ClipCursor setting still applies */
        return TRUE;
    }
    a.x = r->left;  a.y = r->top;
    b.x = r->right; b.y = r->bottom;
    GameToScreen(&a);
    GameToScreen(&b);
    SetRect(&real, a.x, a.y, b.x, b.y);
    return ClipCursor(&real);
}

static BOOL WINAPI hk_ShowWindow(HWND hwnd, int cmd)
{
    if (hwnd && hwnd == g_hwnd && g_curMode == MODE_BORDERLESS &&
        (cmd == SW_MINIMIZE || cmd == SW_SHOWMINIMIZED || cmd == SW_SHOWMINNOACTIVE ||
         cmd == SW_FORCEMINIMIZE)) {
        RF_Log("Ignored the game minimising itself (borderless mode keeps it on screen)");
        return IsWindowVisible(hwnd);
    }
    return ShowWindow(hwnd, cmd);
}

/* ---- hooks for other game modules (e.g. ReflexiveArcade.dll) -----------
 * These get a true mapping between game pixels and the real screen. */

static BOOL WINAPI hk_ClientToScreen_Aux(HWND hwnd, LPPOINT p)
{
    if (hwnd && hwnd == g_hwnd && p) {
        GameToScreen(p);
        return TRUE;
    }
    return ClientToScreen(hwnd, p);
}

static BOOL WINAPI hk_ScreenToClient_Aux(HWND hwnd, LPPOINT p)
{
    BOOL ok = ScreenToClient(hwnd, p);
    if (ok && hwnd && hwnd == g_hwnd && p) {
        LPARAM m = MapMouse(MAKELPARAM(p->x, p->y));
        p->x = (short)LOWORD(m);
        p->y = (short)HIWORD(m);
    }
    return ok;
}

void RF_HookWindowFunctions(HMODULE engine)
{
    RF_PatchIAT(engine, "user32.dll", "CreateWindowExA", hk_CreateWindowExA, NULL);
    RF_PatchIAT(engine, "user32.dll", "SetWindowLongA", hk_SetWindowLongA, NULL);
    RF_PatchIAT(engine, "user32.dll", "SetWindowPos", hk_SetWindowPos, NULL);
    RF_PatchIAT(engine, "user32.dll", "GetClientRect", hk_GetClientRect, NULL);
    RF_PatchIAT(engine, "user32.dll", "ClientToScreen", hk_ClientToScreen_Engine, NULL);
    RF_PatchIAT(engine, "user32.dll", "PeekMessageA", hk_PeekMessageA, NULL);
    RF_PatchIAT(engine, "user32.dll", "GetMessageA", hk_GetMessageA, NULL);
    RF_PatchIAT(engine, "user32.dll", "MoveWindow", hk_MoveWindow, NULL);
    RF_PatchIAT(engine, "user32.dll", "GetWindowRect", hk_GetWindowRect, NULL);
    RF_PatchIAT(engine, "user32.dll", "SetCursorPos", hk_SetCursorPos, NULL);
    RF_PatchIAT(engine, "user32.dll", "ClipCursor", hk_ClipCursor, NULL);
    RF_PatchIAT(engine, "user32.dll", "ShowWindow", hk_ShowWindow, NULL);
}

void RF_HookAuxModule(HMODULE mod)
{
    RF_PatchIAT(mod, "user32.dll", "GetClientRect", hk_GetClientRect, NULL);
    RF_PatchIAT(mod, "user32.dll", "ClientToScreen", hk_ClientToScreen_Aux, NULL);
    RF_PatchIAT(mod, "user32.dll", "ScreenToClient", hk_ScreenToClient_Aux, NULL);
}
