/* RicochetFix - DirectDraw interception.
 *
 * Ricochet uses plain DirectDraw 1 (IDirectDraw / IDirectDrawSurface, no
 * QueryInterface for newer versions). We wrap the engine's DirectDraw object
 * by patching its vtable:
 *
 *   - Exclusive fullscreen is downgraded to DDSCL_NORMAL and SetDisplayMode
 *     is faked, so the monitor never changes resolution.
 *   - The primary surface the engine asks for is replaced by an ordinary
 *     off-screen surface of the game's size (plus a fake back buffer if it
 *     asked for a flip chain). Whatever the engine draws or blits onto it
 *     is then scaled into the real window by RF_Present().
 *
 * vtables in ddraw.dll are shared by every object of the same interface, so
 * each hook first checks whether `this` is one of ours and otherwise calls
 * straight through to the original function.
 */
#include "rf.h"

#define MAX_VT 4
#define MAX_TRACKED 16
#define MAX_DD 8

typedef struct { IDirectDrawVtbl *vt; IDirectDrawVtbl orig; } DDVT;
typedef struct { IDirectDrawSurfaceVtbl *vt; IDirectDrawSurfaceVtbl orig; } SurfVT;
typedef struct { IDirectDrawSurface *s; IDirectDrawSurface *back; IDirectDraw *dd; int isBack; } Tracked;

static DDVT    g_ddvt[MAX_VT];   static int g_nddvt;
static SurfVT  g_svt[MAX_VT];    static int g_nsvt;
static Tracked g_tr[MAX_TRACKED];
static IDirectDraw *g_dd[MAX_DD];
static int g_fakeMode;           /* engine called SetDisplayMode */
static DDPIXELFORMAT g_fakePF;   /* pixel format of the faked display mode */

/* The engine writes pixels in the depth it asked for (e.g. 16-bit 565), so
 * surfaces created after a faked mode switch must use that depth, exactly
 * as they would have on a real 16-bit display. */
static int MakePixelFormat(DWORD bpp, DDPIXELFORMAT *pf)
{
    ZeroMemory(pf, sizeof(*pf));
    pf->dwSize = sizeof(*pf);
    pf->dwFlags = DDPF_RGB;
    pf->dwRGBBitCount = bpp;
    switch (bpp) {
    case 16: pf->dwRBitMask = 0xF800;   pf->dwGBitMask = 0x07E0;   pf->dwBBitMask = 0x001F;   return 1;
    case 24:
    case 32: pf->dwRBitMask = 0xFF0000; pf->dwGBitMask = 0x00FF00; pf->dwBBitMask = 0x0000FF; return 1;
    }
    ZeroMemory(pf, sizeof(*pf));  /* unsupported depth: don't force a format at all */
    return 0;
}

static HRESULT (WINAPI *o_DirectDrawCreate)(GUID *, LPDIRECTDRAW *, IUnknown *);

/* ---- bookkeeping ------------------------------------------------------- */

static const IDirectDrawVtbl *DO(IDirectDraw *dd)
{
    int i;
    for (i = 0; i < g_nddvt; i++)
        if (g_ddvt[i].vt == dd->lpVtbl)
            return &g_ddvt[i].orig;
    return dd->lpVtbl;
}

static const IDirectDrawSurfaceVtbl *SO(IDirectDrawSurface *s)
{
    int i;
    for (i = 0; i < g_nsvt; i++)
        if (g_svt[i].vt == s->lpVtbl)
            return &g_svt[i].orig;
    return s->lpVtbl;
}

HRESULT RF_SurfGetDC(IDirectDrawSurface *s, HDC *dc)    { return SO(s)->GetDC(s, dc); }
HRESULT RF_SurfReleaseDC(IDirectDrawSurface *s, HDC dc) { return SO(s)->ReleaseDC(s, dc); }

static int IsEngineDD(IDirectDraw *dd)
{
    int i;
    for (i = 0; i < MAX_DD; i++)
        if (g_dd[i] == dd)
            return 1;
    return 0;
}

static Tracked *Find(IDirectDrawSurface *s)
{
    int i;
    if (!s)
        return NULL;
    for (i = 0; i < MAX_TRACKED; i++)
        if (g_tr[i].s == s)
            return &g_tr[i];
    return NULL;
}

static Tracked *FindFront(IDirectDrawSurface *s)
{
    Tracked *t = Find(s);
    return t && !t->isBack ? t : NULL;
}

static int Track(IDirectDraw *dd, IDirectDrawSurface *s, IDirectDrawSurface *back, int isBack)
{
    int i;
    for (i = 0; i < MAX_TRACKED; i++) {
        if (!g_tr[i].s) {
            g_tr[i].s = s;
            g_tr[i].back = back;
            g_tr[i].dd = dd;
            g_tr[i].isBack = isBack;
            return 1;
        }
    }
    RF_Log("WARNING: too many tracked surfaces");
    return 0;
}

static void Untrack(IDirectDrawSurface *s)
{
    Tracked *t = Find(s);
    if (t)
        ZeroMemory(t, sizeof(*t));
    RF_ForgetSurface(s);
}

/* ---- IDirectDrawSurface hooks ------------------------------------------ */

static ULONG WINAPI s_Release(IDirectDrawSurface *s)
{
    Tracked *t = Find(s);
    IDirectDrawSurface *back = t ? t->back : NULL;
    const IDirectDrawSurfaceVtbl *o = SO(s);
    ULONG r = o->Release(s);
    if (t && r == 0) {
        int isBack = t->isBack, i;
        Untrack(s);
        if (isBack) {
            /* The game released the back buffer more often than it got it:
             * make sure the front no longer points at the freed surface. */
            for (i = 0; i < MAX_TRACKED; i++)
                if (g_tr[i].back == s)
                    g_tr[i].back = NULL;
        } else {
            if (back) {
                Untrack(back);
                SO(back)->Release(back);  /* drop the reference we kept */
            }
            RF_Log("Virtual primary surface released");
        }
    }
    return r;
}

/* Clip a blit to the virtual primary, adjusting the source rectangle by the
 * same proportion. Returns 0 if nothing is left to draw. */
static int ClipBlit(LPRECT dst, IDirectDrawSurface *src, LPRECT srcRect,
                    RECT *outDst, RECT *outSrc, int *haveSrc)
{
    RECT bounds, c, sr;
    int dw, dh, sw, sh;

    SetRect(&bounds, 0, 0, g_gameW, g_gameH);
    *haveSrc = 0;
    if (!IntersectRect(&c, dst, &bounds))
        return 0;
    *outDst = c;
    if (EqualRect(&c, dst) || !src)
        return 1;

    if (srcRect) {
        sr = *srcRect;
    } else {
        DDSURFACEDESC d;
        ZeroMemory(&d, sizeof(d));
        d.dwSize = sizeof(d);
        if (SO(src)->GetSurfaceDesc(src, &d) != DD_OK)
            return 1;
        SetRect(&sr, 0, 0, d.dwWidth, d.dwHeight);
    }
    dw = dst->right - dst->left; dh = dst->bottom - dst->top;
    sw = sr.right - sr.left;     sh = sr.bottom - sr.top;
    if (dw <= 0 || dh <= 0)
        return 0;
    sr.left   += MulDiv(c.left - dst->left, sw, dw);
    sr.right  -= MulDiv(dst->right - c.right, sw, dw);
    sr.top    += MulDiv(c.top - dst->top, sh, dh);
    sr.bottom -= MulDiv(dst->bottom - c.bottom, sh, dh);
    *outSrc = sr;
    *haveSrc = 1;
    return 1;
}

static HRESULT WINAPI s_Blt(IDirectDrawSurface *s, LPRECT dst, LPDIRECTDRAWSURFACE src,
                            LPRECT srcRect, DWORD flags, LPDDBLTFX fx)
{
    const IDirectDrawSurfaceVtbl *o = SO(s);
    HRESULT r;
    if (!FindFront(s))
        return o->Blt(s, dst, src, srcRect, flags, fx);
    if (dst) {
        RECT d, sr;
        int haveSrc;
        if (!ClipBlit(dst, src, srcRect, &d, &sr, &haveSrc))
            return DD_OK;
        r = o->Blt(s, &d, src, haveSrc ? &sr : srcRect, flags, fx);
    } else {
        r = o->Blt(s, dst, src, srcRect, flags, fx);
    }
    RF_MarkPending(s);
    return r;
}

static HRESULT WINAPI s_BltFast(IDirectDrawSurface *s, DWORD x, DWORD y,
                                LPDIRECTDRAWSURFACE src, LPRECT srcRect, DWORD trans)
{
    HRESULT r = SO(s)->BltFast(s, x, y, src, srcRect, trans);
    if (FindFront(s))
        RF_MarkPending(s);
    return r;
}

static HRESULT WINAPI s_Flip(IDirectDrawSurface *s, LPDIRECTDRAWSURFACE target, DWORD flags)
{
    Tracked *t = FindFront(s);
    if (!t)
        return SO(s)->Flip(s, target, flags);
    RF_Present(t->back ? t->back : s);
    RF_WaitVSync();
    return DD_OK;
}

static HRESULT WINAPI s_GetAttachedSurface(IDirectDrawSurface *s, LPDDSCAPS caps,
                                           LPDIRECTDRAWSURFACE *out)
{
    Tracked *t = FindFront(s);
    if (t && t->back && out) {
        t->back->lpVtbl->AddRef(t->back);
        *out = t->back;
        return DD_OK;
    }
    return SO(s)->GetAttachedSurface(s, caps, out);
}

static void FakeCaps(IDirectDrawSurface *s, DWORD *caps)
{
    Tracked *t = Find(s);
    if (!t)
        return;
    *caps &= ~DDSCAPS_OFFSCREENPLAIN;
    if (t->isBack) {
        *caps |= DDSCAPS_BACKBUFFER | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
    } else {
        *caps |= DDSCAPS_PRIMARYSURFACE | DDSCAPS_VISIBLE;
        if (t->back)
            *caps |= DDSCAPS_FRONTBUFFER | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
    }
}

static HRESULT WINAPI s_GetCaps(IDirectDrawSurface *s, LPDDSCAPS caps)
{
    HRESULT r = SO(s)->GetCaps(s, caps);
    if (r == DD_OK && caps)
        FakeCaps(s, &caps->dwCaps);
    return r;
}

static HRESULT WINAPI s_GetSurfaceDesc(IDirectDrawSurface *s, LPDDSURFACEDESC d)
{
    HRESULT r = SO(s)->GetSurfaceDesc(s, d);
    if (r == DD_OK && d)
        FakeCaps(s, &d->ddsCaps.dwCaps);
    return r;
}

static HRESULT WINAPI s_ReleaseDC(IDirectDrawSurface *s, HDC dc)
{
    HRESULT r = SO(s)->ReleaseDC(s, dc);
    if (FindFront(s))
        RF_MarkPending(s);
    return r;
}

static HRESULT WINAPI s_Unlock(IDirectDrawSurface *s, LPVOID p)
{
    HRESULT r = SO(s)->Unlock(s, p);
    if (FindFront(s))
        RF_MarkPending(s);
    return r;
}

static HRESULT WINAPI s_SetClipper(IDirectDrawSurface *s, LPDIRECTDRAWCLIPPER c)
{
    if (Find(s))
        return DD_OK;  /* the virtual primary is in game pixels; a window clipper would cut it up */
    return SO(s)->SetClipper(s, c);
}

static void PatchSurfaceVT(IDirectDrawSurface *s)
{
    IDirectDrawSurfaceVtbl *vt = s->lpVtbl;
    DWORD old;
    int i;

    for (i = 0; i < g_nsvt; i++)
        if (g_svt[i].vt == vt)
            return;
    if (g_nsvt == MAX_VT || !VirtualProtect(vt, sizeof(*vt), PAGE_EXECUTE_READWRITE, &old))
        return;
    g_svt[g_nsvt].vt = vt;
    g_svt[g_nsvt].orig = *vt;
    g_nsvt++;
    vt->Release = s_Release;
    vt->Blt = s_Blt;
    vt->BltFast = s_BltFast;
    vt->Flip = s_Flip;
    vt->GetAttachedSurface = s_GetAttachedSurface;
    vt->GetCaps = s_GetCaps;
    vt->GetSurfaceDesc = s_GetSurfaceDesc;
    vt->ReleaseDC = s_ReleaseDC;
    vt->Unlock = s_Unlock;
    vt->SetClipper = s_SetClipper;
    VirtualProtect(vt, sizeof(*vt), old, &old);
    RF_Log("Hooked IDirectDrawSurface vtable %p", (void *)vt);
}

/* ---- IDirectDraw hooks ------------------------------------------------- */

static ULONG WINAPI dd_Release(IDirectDraw *dd)
{
    ULONG r = DO(dd)->Release(dd);
    if (r == 0) {
        int i;
        for (i = 0; i < MAX_DD; i++)
            if (g_dd[i] == dd)
                g_dd[i] = NULL;
        /* DirectDraw frees every surface it created when the object goes
         * away. Forget ours so nothing (e.g. a late WM_PAINT) touches them
         * and a new surface at a reused address isn't mistaken for one. */
        for (i = 0; i < MAX_TRACKED; i++) {
            if (g_tr[i].s && g_tr[i].dd == dd) {
                RF_ForgetSurface(g_tr[i].s);
                ZeroMemory(&g_tr[i], sizeof(g_tr[i]));
            }
        }
    }
    return r;
}

static HRESULT WINAPI dd_SetCooperativeLevel(IDirectDraw *dd, HWND hwnd, DWORD flags)
{
    DWORD nf;
    HRESULT r;
    if (!IsEngineDD(dd))
        return DO(dd)->SetCooperativeLevel(dd, hwnd, flags);
    if (hwnd && !g_hwnd && !(GetWindowLongA(hwnd, GWL_STYLE) & WS_CHILD))
        RF_AttachWindow(hwnd);
    nf = DDSCL_NORMAL | (flags & (DDSCL_MULTITHREADED | DDSCL_FPUSETUP | DDSCL_FPUPRESERVE));
    r = DO(dd)->SetCooperativeLevel(dd, hwnd, nf);
    RF_Log("SetCooperativeLevel(%08lx -> %08lx) = %08lx", flags, nf, (unsigned long)r);
    return r;
}

static HRESULT WINAPI dd_SetDisplayMode(IDirectDraw *dd, DWORD w, DWORD h, DWORD bpp)
{
    if (!IsEngineDD(dd))
        return DO(dd)->SetDisplayMode(dd, w, h, bpp);
    RF_Log("SetDisplayMode(%lux%lu, %lu bpp) faked - the monitor keeps its resolution", w, h, bpp);
    g_fakeMode = 1;
    if (!MakePixelFormat(bpp, &g_fakePF))
        RF_Log("WARNING: %lu bpp is not emulated; using the desktop format", bpp);
    RF_SetGameSize((int)w, (int)h);
    return DD_OK;
}

static HRESULT WINAPI dd_RestoreDisplayMode(IDirectDraw *dd)
{
    if (!IsEngineDD(dd))
        return DO(dd)->RestoreDisplayMode(dd);
    return DD_OK;
}

static HRESULT WINAPI dd_GetDisplayMode(IDirectDraw *dd, LPDDSURFACEDESC d)
{
    HRESULT r = DO(dd)->GetDisplayMode(dd, d);
    if (r == DD_OK && d && g_fakeMode && IsEngineDD(dd)) {
        d->dwWidth = g_gameW;
        d->dwHeight = g_gameH;
        if (g_fakePF.dwSize) {
            d->ddpfPixelFormat = g_fakePF;
            d->lPitch = g_gameW * (g_fakePF.dwRGBBitCount / 8);
        }
    }
    return r;
}

static HRESULT WINAPI dd_FlipToGDISurface(IDirectDraw *dd)
{
    if (!IsEngineDD(dd))
        return DO(dd)->FlipToGDISurface(dd);
    return DD_OK;
}

/* Modern monitors (and Wine's virtual desktops) often don't list the old
 * 4:3 modes, and the engine refuses to start without 800x600. The mode
 * switch is faked anyway, so add the classic modes to the list. */
#define MAX_MODES 1024
typedef struct { DDSURFACEDESC m[MAX_MODES]; int n; } ModeList;

static HRESULT WINAPI CollectMode(LPDDSURFACEDESC d, LPVOID ctx)
{
    ModeList *l = (ModeList *)ctx;
    if (l->n < MAX_MODES)
        l->m[l->n++] = *d;
    return DDENUMRET_OK;
}

static int HasMode(ModeList *l, DWORD w, DWORD h, DWORD bpp)
{
    int i;
    for (i = 0; i < l->n; i++)
        if (l->m[i].dwWidth == w && l->m[i].dwHeight == h &&
            l->m[i].ddpfPixelFormat.dwRGBBitCount == bpp)
            return 1;
    return 0;
}

static int ModeMatches(const DDSURFACEDESC *filter, const DDSURFACEDESC *m)
{
    if (!filter)
        return 1;
    if ((filter->dwFlags & DDSD_WIDTH) && filter->dwWidth != m->dwWidth)
        return 0;
    if ((filter->dwFlags & DDSD_HEIGHT) && filter->dwHeight != m->dwHeight)
        return 0;
    if ((filter->dwFlags & DDSD_PIXELFORMAT) &&
        filter->ddpfPixelFormat.dwRGBBitCount != m->ddpfPixelFormat.dwRGBBitCount)
        return 0;
    return 1;
}

static HRESULT WINAPI dd_EnumDisplayModes(IDirectDraw *dd, DWORD flags, LPDDSURFACEDESC filter,
                                          LPVOID ctx, LPDDENUMMODESCALLBACK cb)
{
    static const DWORD sizes[][2] = { { 640, 480 }, { 800, 600 }, { 1024, 768 } };
    static const DWORD depths[] = { 16, 32 };
    ModeList *l;
    HRESULT r;
    int i, j, k, added = 0;

    if (!IsEngineDD(dd) || !cb)
        return DO(dd)->EnumDisplayModes(dd, flags, filter, ctx, cb);
    l = (ModeList *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*l));
    if (!l)
        return DO(dd)->EnumDisplayModes(dd, flags, filter, ctx, cb);
    r = DO(dd)->EnumDisplayModes(dd, flags, NULL, l, CollectMode);
    if (r == DD_OK) {
        int real = l->n;
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 2; j++) {
                DDSURFACEDESC m;
                if (HasMode(l, sizes[i][0], sizes[i][1], depths[j]) || l->n >= MAX_MODES)
                    continue;
                /* copy the pixel format of a real mode with the same depth */
                ZeroMemory(&m, sizeof(m));
                for (k = 0; k < real; k++) {
                    if (l->m[k].ddpfPixelFormat.dwRGBBitCount == depths[j]) {
                        m = l->m[k];
                        break;
                    }
                }
                if (k == real) {
                    m.dwSize = sizeof(m);
                    m.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT;
                    m.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
                    m.ddpfPixelFormat.dwFlags = DDPF_RGB;
                    m.ddpfPixelFormat.dwRGBBitCount = depths[j];
                    if (depths[j] == 16) {
                        m.ddpfPixelFormat.dwRBitMask = 0xF800;
                        m.ddpfPixelFormat.dwGBitMask = 0x07E0;
                        m.ddpfPixelFormat.dwBBitMask = 0x001F;
                    } else {
                        m.ddpfPixelFormat.dwRBitMask = 0xFF0000;
                        m.ddpfPixelFormat.dwGBitMask = 0x00FF00;
                        m.ddpfPixelFormat.dwBBitMask = 0x0000FF;
                    }
                }
                m.dwWidth = sizes[i][0];
                m.dwHeight = sizes[i][1];
                m.lPitch = sizes[i][0] * (depths[j] / 8);
                l->m[l->n++] = m;
                added++;
            }
        }
        if (added)
            RF_Log("EnumDisplayModes: added %d classic modes the monitor doesn't list", added);
        for (i = 0; i < l->n; i++)
            if (ModeMatches(filter, &l->m[i]) && cb(&l->m[i], ctx) == DDENUMRET_CANCEL)
                break;
    }
    HeapFree(GetProcessHeap(), 0, l);
    return r;
}

/* The virtual surfaces live in system memory: they can never be "lost"
 * (alt-tab, lock screen, UAC prompt) and the CPU renderer and GDI read and
 * write them fastest there. A real flip chain would restore its back buffer
 * together with the front; two separate video memory surfaces wouldn't. */
static HRESULT CreatePlain(IDirectDraw *dd, IDirectDrawSurface **out)
{
    const DWORD memCaps = DDSCAPS_SYSTEMMEMORY;
    DDSURFACEDESC sd;
    HRESULT r;
    ZeroMemory(&sd, sizeof(sd));
    sd.dwSize = sizeof(sd);
    sd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    sd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | memCaps;
    sd.dwWidth = g_gameW;
    sd.dwHeight = g_gameH;
    if (g_fakePF.dwSize) {
        sd.dwFlags |= DDSD_PIXELFORMAT;
        sd.ddpfPixelFormat = g_fakePF;
    }
    r = DO(dd)->CreateSurface(dd, &sd, out, NULL);
    if (r != DD_OK) {
        sd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;  /* let the driver pick */
        r = DO(dd)->CreateSurface(dd, &sd, out, NULL);
    }
    if (r == DD_OK) {
        DDBLTFX fx;
        ZeroMemory(&fx, sizeof(fx));
        fx.dwSize = sizeof(fx);
        PatchSurfaceVT(*out);
        SO(*out)->Blt(*out, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
    }
    return r;
}

static HRESULT WINAPI dd_CreateSurface(IDirectDraw *dd, LPDDSURFACEDESC d,
                                       LPDIRECTDRAWSURFACE *out, IUnknown *unk)
{
    IDirectDrawSurface *front = NULL, *back = NULL;
    HRESULT r;

    if (!IsEngineDD(dd) || !d || !out || !(d->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)) {
        DDSURFACEDESC copy;
        if (IsEngineDD(dd) && d && g_fakePF.dwSize && !(d->dwFlags & DDSD_PIXELFORMAT) &&
            !(d->ddsCaps.dwCaps & (DDSCAPS_TEXTURE | DDSCAPS_ZBUFFER | DDSCAPS_PRIMARYSURFACE))) {
            copy = *d;
            copy.dwFlags |= DDSD_PIXELFORMAT;
            copy.ddpfPixelFormat = g_fakePF;
            d = &copy;
        }
        r = DO(dd)->CreateSurface(dd, d, out, unk);
        if (r == DD_OK && out && *out && IsEngineDD(dd))
            PatchSurfaceVT(*out);
        return r;
    }

    r = CreatePlain(dd, &front);
    if (r != DD_OK) {
        RF_Log("ERROR: could not create virtual primary (%08lx)", (unsigned long)r);
        return r;
    }
    if (((d->dwFlags & DDSD_BACKBUFFERCOUNT) && d->dwBackBufferCount) ||
        (d->ddsCaps.dwCaps & DDSCAPS_FLIP)) {
        r = CreatePlain(dd, &back);
        if (r != DD_OK) {
            RF_Log("ERROR: could not create virtual back buffer (%08lx)", (unsigned long)r);
            SO(front)->Release(front);
            return r;
        }
    }
    if (!Track(dd, front, back, 0) || (back && !Track(dd, back, NULL, 1))) {
        Untrack(front);
        if (back)
            SO(back)->Release(back);
        SO(front)->Release(front);
        return DDERR_OUTOFMEMORY;
    }
    *out = front;
    RF_Log("Primary surface requested (caps %08lx, %lu back buffers) -> virtual %dx%d%s",
           d->ddsCaps.dwCaps, (d->dwFlags & DDSD_BACKBUFFERCOUNT) ? d->dwBackBufferCount : 0,
           g_gameW, g_gameH, back ? " with back buffer" : "");
    return DD_OK;
}

static void PatchDDVT(IDirectDraw *dd)
{
    IDirectDrawVtbl *vt = dd->lpVtbl;
    DWORD old;
    int i;

    for (i = 0; i < g_nddvt; i++)
        if (g_ddvt[i].vt == vt)
            return;
    if (g_nddvt == MAX_VT || !VirtualProtect(vt, sizeof(*vt), PAGE_EXECUTE_READWRITE, &old))
        return;
    g_ddvt[g_nddvt].vt = vt;
    g_ddvt[g_nddvt].orig = *vt;
    g_nddvt++;
    vt->Release = dd_Release;
    vt->SetCooperativeLevel = dd_SetCooperativeLevel;
    vt->SetDisplayMode = dd_SetDisplayMode;
    vt->RestoreDisplayMode = dd_RestoreDisplayMode;
    vt->GetDisplayMode = dd_GetDisplayMode;
    vt->FlipToGDISurface = dd_FlipToGDISurface;
    vt->CreateSurface = dd_CreateSurface;
    vt->EnumDisplayModes = dd_EnumDisplayModes;
    VirtualProtect(vt, sizeof(*vt), old, &old);
    RF_Log("Hooked IDirectDraw vtable %p", (void *)vt);
}

static HRESULT WINAPI hk_DirectDrawCreate(GUID *guid, LPDIRECTDRAW *out, IUnknown *unk)
{
    HRESULT r = o_DirectDrawCreate(guid, out, unk);
    RF_Log("DirectDrawCreate = %08lx", (unsigned long)r);
    if (r == DD_OK && out && *out) {
        int i;
        for (i = 0; i < MAX_DD; i++) {
            if (!g_dd[i]) {
                g_dd[i] = *out;
                break;
            }
        }
        PatchDDVT(*out);
    }
    return r;
}

void RF_HookDirectDraw(HMODULE engine)
{
    if (RF_PatchIAT(engine, "ddraw.dll", "DirectDrawCreate", hk_DirectDrawCreate,
                    (void **)&o_DirectDrawCreate))
        RF_Log("Hooked DirectDrawCreate");
    else
        RF_Log("WARNING: engine does not import DirectDrawCreate");
}
