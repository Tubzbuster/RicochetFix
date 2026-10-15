/* RicochetFix - what fills the bars around the 4:3 picture.
 *
 * Border=ambient: each bar shows a heavily blurred, dimmed continuation of
 * the picture edge next to it, so the bars pick up the colours on screen
 * without adding any artwork of our own. Border=black: plain black bars.
 *
 * How ambient stays cheap (it runs inside the game's own frame):
 *   1. Sample: the picture edge next to a bar is copied with a plain BitBlt
 *      (no stretching) and box-averaged in C into a tiny grid, e.g. 4x34
 *      cells for a side bar. All of the "blur" comes from that grid.
 *   2. Smooth: the grid eases towards each new sample, so flashes in the
 *      game don't flicker the bars.
 *   3. Skip: if no cell changed by a visible amount, nothing is redrawn.
 *      Static scenes (menus, pauses) cost almost nothing.
 *   4. Render: the bar is rendered at 1/4 resolution (bilinear from the
 *      grid, with the brightness fade) and scaled up by GDI. The content is
 *      so smooth that the scaling can't be seen. The dark seam next to the
 *      picture is drawn separately at full resolution so it stays crisp.
 *   5. Rate: sampling happens at most 30 times a second.
 *
 * Every size is defined for a 1080-pixel-high screen and scaled by
 * (screen height / 1080), so 1440p and 4K look exactly like 1080p.
 *
 * GDI resources: a capture DIB (game-frame sized) and a low-res bar DIB,
 * each with a memory DC. Created on demand, the bar DIB is recreated only
 * when the layout changes, and everything is freed in RF_BorderFree().
 */
#include "rf.h"

#define REF_H        1080   /* sizes below are for a 1080p screen */
#define STRIP_PX     48     /* picture edge sampled, in screen px at 1080p */
#define GAP_PX       3      /* dark seam between picture and bar, at 1080p */
#define CELLS_LONG   34     /* grid cells along the bar */
#define CELLS_SHORT  4      /* grid cells across the bar */
#define LOWRES       4      /* bars are rendered at 1/LOWRES resolution */
#define NEAR_LEVEL   166    /* 0..256: brightness next to the picture (65%) */
#define FAR_LEVEL    90     /* 0..256: far edge, relative to NEAR_LEVEL (35%) */
#define UPDATE_MS    33     /* sample at most ~30 times a second */
#define EASE         85     /* 0..256: how far each sample moves the grid (~1/3) */
#define VISIBLE      192    /* 8.8 fixed point: change worth redrawing (~0.75 level) */

enum { BAR_LEFT, BAR_RIGHT, BAR_TOP, BAR_BOTTOM, BAR_COUNT };

typedef struct {
    RECT dst;                 /* bar rectangle in client pixels */
    int side;                 /* BAR_* */
    int cols, rows;           /* grid size */
    int valid;                /* grid holds a sample */
    int changed;              /* grid moved visibly since the last render */
    int grid[CELLS_LONG * CELLS_SHORT * 3];  /* smoothed colours, 8.8 fixed point */
} Bar;

typedef struct { HDC dc; HBITMAP bmp, old; DWORD *bits; int w, h; } Dib;

static Bar     g_bars[BAR_COUNT];
static int     g_nbars;
static RECT    g_lastView;
static int     g_lastCW, g_lastCH;
static int     g_dirty = 1;            /* bars must be (re)drawn */
static DWORD   g_lastSample;
static DWORD   g_lastBlack;
static int     g_failed;               /* resources unavailable: fall back to black */

static Dib     g_cap;                  /* copy of the picture edge */
static Dib     g_low;                  /* low-res bar rendering */
static int    *g_lut;                  /* per-layout lookup tables for Render() */

/* cost measurement for the Health log line */
static LONGLONG g_costSum, g_costMax, g_qpf;
static DWORD    g_costN;

/* ---- GDI resources ----------------------------------------------------- */

static void DibFree(Dib *d)
{
    if (d->dc) {
        if (d->old)
            SelectObject(d->dc, d->old);
        DeleteDC(d->dc);
    }
    if (d->bmp)
        DeleteObject(d->bmp);
    ZeroMemory(d, sizeof(*d));
}

/* (Re)create a 32-bit top-down DIB of at least w x h. */
static int DibEnsure(Dib *d, int w, int h)
{
    BITMAPINFO bi;

    if (d->dc && d->w >= w && d->h >= h)
        return 1;
    DibFree(d);
    d->dc = CreateCompatibleDC(NULL);
    if (!d->dc)
        return 0;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    d->bmp = CreateDIBSection(d->dc, &bi, DIB_RGB_COLORS, (void **)&d->bits, NULL, 0);
    if (!d->bmp || !d->bits) {
        DibFree(d);
        return 0;
    }
    d->old = (HBITMAP)SelectObject(d->dc, d->bmp);
    d->w = w;
    d->h = h;
    return 1;
}

void RF_BorderFree(void)
{
    DibFree(&g_cap);
    DibFree(&g_low);
    if (g_lut)
        HeapFree(GetProcessHeap(), 0, g_lut);
    g_lut = NULL;
    g_nbars = 0;
    g_lastCW = g_lastCH = 0;          /* force a fresh layout next time */
    SetRectEmpty(&g_lastView);
    g_failed = 0;
    g_dirty = 1;
}

static void Fail(const char *what)
{
    if (!g_failed)
        RF_Log("WARNING: %s - using black bars", what);
    g_failed = 1;
    g_dirty = 1;
}

/* ---- layout ------------------------------------------------------------ */

static void AddBar(int side, int l, int t, int r, int b)
{
    int alongX = side == BAR_TOP || side == BAR_BOTTOM;
    Bar *bar;
    if (r - l <= 0 || b - t <= 0)
        return;
    bar = &g_bars[g_nbars++];
    ZeroMemory(bar, sizeof(*bar));
    SetRect(&bar->dst, l, t, r, b);
    bar->side = side;
    bar->cols = alongX ? CELLS_LONG : CELLS_SHORT;
    bar->rows = alongX ? CELLS_SHORT : CELLS_LONG;
}

static int LowSize(int n)
{
    return (n + LOWRES - 1) / LOWRES;
}

/* Recompute the bars when the window or picture rectangle changes. */
static void Layout(int cw, int ch, const RECT *v)
{
    int needW = 0, needH = 0, i;

    if (cw == g_lastCW && ch == g_lastCH && EqualRect(v, &g_lastView))
        return;
    g_lastCW = cw;
    g_lastCH = ch;
    g_lastView = *v;
    g_nbars = 0;
    AddBar(BAR_LEFT, 0, 0, v->left, ch);              /* full height */
    AddBar(BAR_RIGHT, v->right, 0, cw, ch);           /* full height */
    AddBar(BAR_TOP, v->left, 0, v->right, v->top);    /* between the sides */
    AddBar(BAR_BOTTOM, v->left, v->bottom, v->right, ch);
    g_dirty = 1;
    if (g_cfg.border != BORDER_AMBIENT || !g_nbars)
        return;

    for (i = 0; i < g_nbars; i++) {
        int w = LowSize(g_bars[i].dst.right - g_bars[i].dst.left);
        int h = LowSize(g_bars[i].dst.bottom - g_bars[i].dst.top);
        if (w > needW) needW = w;
        if (h > needH) needH = h;
    }
    /* low-res bar DIB and lookup tables follow the layout */
    DibFree(&g_low);
    if (g_lut)
        HeapFree(GetProcessHeap(), 0, g_lut);
    g_lut = (int *)HeapAlloc(GetProcessHeap(), 0, sizeof(int) * 4 * ((size_t)needW + needH));
    if (!g_lut || !DibEnsure(&g_low, needW, needH))
        Fail("could not create the border bitmap");
}

/* ---- ambient: sample --------------------------------------------------- */

/* Copy the picture edge next to a bar and box-average it into the grid. */
static void Sample(Bar *bar, HDC sdc, int gameW, int gameH, const RECT *v, int ch)
{
    int vw = v->right - v->left, vh = v->bottom - v->top;
    double s = (double)ch / REF_H;
    int sx, sy, sw, sh, cx, cy, x, y, k;

    /* STRIP_PX screen pixels at 1080p, expressed in game pixels */
    sw = (int)(STRIP_PX * s * gameW / (vw > 0 ? vw : 1) + 0.5);
    sh = (int)(STRIP_PX * s * gameH / (vh > 0 ? vh : 1) + 0.5);
    if (sw < bar->cols) sw = bar->cols;
    if (sh < bar->rows) sh = bar->rows;
    if (sw > gameW) sw = gameW;
    if (sh > gameH) sh = gameH;

    switch (bar->side) {
    case BAR_LEFT:   sx = 0;          sy = 0;          sh = gameH; break;
    case BAR_RIGHT:  sx = gameW - sw; sy = 0;          sh = gameH; break;
    case BAR_TOP:    sx = 0;          sy = 0;          sw = gameW; break;
    default:         sx = 0;          sy = gameH - sh; sw = gameW; break;
    }

    /* plain copy (no stretching): the fastest GDI path; converts 16-bit too */
    BitBlt(g_cap.dc, 0, 0, sw, sh, sdc, sx, sy, SRCCOPY);
    GdiFlush();  /* make sure GDI has finished writing before we read */

    for (cy = 0; cy < bar->rows; cy++) {
        int y0 = cy * sh / bar->rows, y1 = (cy + 1) * sh / bar->rows;
        for (cx = 0; cx < bar->cols; cx++) {
            int x0 = cx * sw / bar->cols, x1 = (cx + 1) * sw / bar->cols;
            unsigned r = 0, g = 0, b = 0, n = 0;
            int *cell = &bar->grid[(cy * bar->cols + cx) * 3];
            int c[3];

            for (y = y0; y < y1; y++) {
                const DWORD *p = g_cap.bits + (size_t)y * g_cap.w;
                for (x = x0; x < x1; x++) {
                    r += p[x] >> 16 & 255;
                    g += p[x] >> 8 & 255;
                    b += p[x] & 255;
                }
                n += x1 - x0;
            }
            if (!n)
                continue;
            c[0] = (int)((r << 8) / n);
            c[1] = (int)((g << 8) / n);
            c[2] = (int)((b << 8) / n);
            for (k = 0; k < 3; k++) {
                int d = bar->valid ? ((c[k] - cell[k]) * EASE) >> 8 : c[k] - cell[k];
                cell[k] += d;
                if (d >= VISIBLE || d <= -VISIBLE)
                    bar->changed = 1;
            }
        }
    }
    if (!bar->valid)
        bar->changed = 1;
    bar->valid = 1;
}

/* ---- ambient: render --------------------------------------------------- */

/* Map n output pixels onto m grid cells: first cell, second cell, weight. */
static void Weights(int n, int m, int *i0, int *i1, int *f)
{
    int i;
    for (i = 0; i < n; i++) {
        int u = (int)(((i + 0.5) * m / n - 0.5) * 256);  /* 8.8 position */
        if (u < 0) u = 0;
        if (u > (m - 1) * 256) u = (m - 1) * 256;
        i0[i] = u >> 8;
        i1[i] = i0[i] + 1 < m ? i0[i] + 1 : i0[i];
        f[i] = u & 255;
    }
}

/* Render one bar at low resolution, scale it into the window, add the seam. */
static void Render(HDC wdc, const Bar *bar, int ch)
{
    int w = bar->dst.right - bar->dst.left, h = bar->dst.bottom - bar->dst.top;
    int lw = LowSize(w), lh = LowSize(h);
    int alongX = bar->side == BAR_TOP || bar->side == BAR_BOTTOM;
    int nearHigh = bar->side == BAR_LEFT || bar->side == BAR_TOP;  /* picture at high coords */
    int n = alongX ? lh : lw;                                     /* pixels across */
    int gap = (int)(GAP_PX * (double)ch / REF_H + 0.5);
    int *xi0, *xi1, *xf, *yi0, *yi1, *yf, *ramp;
    int x, y, k;
    RECT seam;

    if (!g_lut || lw > g_low.w || lh > g_low.h)
        return;
    xi0 = g_lut;     xi1 = xi0 + lw;  xf = xi1 + lw;
    yi0 = xf + lw;   yi1 = yi0 + lh;  yf = yi1 + lh;
    ramp = yf + lh;                   /* n <= max(lw, lh) entries */
    Weights(lw, bar->cols, xi0, xi1, xf);
    Weights(lh, bar->rows, yi0, yi1, yf);

    /* brightness: NEAR_LEVEL next to the picture, fading to
     * NEAR_LEVEL * FAR_LEVEL at the far edge */
    for (k = 0; k < n; k++) {
        int d = nearHigh ? (n - 1 - k) : k;
        int t = n > 1 ? (d * 256) / (n - 1) : 0;
        ramp[k] = (NEAR_LEVEL * (256 - (((256 - FAR_LEVEL) * t) >> 8))) >> 8;
    }

    for (y = 0; y < lh; y++) {
        int row[CELLS_LONG * 3];
        const int *r0 = &bar->grid[yi0[y] * bar->cols * 3];
        const int *r1 = &bar->grid[yi1[y] * bar->cols * 3];
        DWORD *out = g_low.bits + (size_t)y * g_low.w;
        int fy = yf[y];

        for (k = 0; k < bar->cols * 3; k++)            /* vertical pass on the grid */
            row[k] = r0[k] + (((r1[k] - r0[k]) * fy) >> 8);

        for (x = 0; x < lw; x++) {                     /* horizontal pass per pixel */
            const int *a = &row[xi0[x] * 3], *b = &row[xi1[x] * 3];
            int fx = xf[x];
            int m = alongX ? ramp[y] : ramp[x];
            int rr = ((a[0] + (((b[0] - a[0]) * fx) >> 8)) * m) >> 16;
            int gg = ((a[1] + (((b[1] - a[1]) * fx) >> 8)) * m) >> 16;
            int bb = ((a[2] + (((b[2] - a[2]) * fx) >> 8)) * m) >> 16;
            out[x] = (DWORD)(rr << 16 | gg << 8 | bb);
        }
    }

    SetStretchBltMode(wdc, COLORONCOLOR);
    StretchBlt(wdc, bar->dst.left, bar->dst.top, w, h, g_low.dc, 0, 0, lw, lh, SRCCOPY);

    /* crisp dark seam between picture and bar, at full resolution */
    if (gap < 1)
        gap = 1;
    seam = bar->dst;
    switch (bar->side) {
    case BAR_LEFT:   seam.left = seam.right - gap;   break;
    case BAR_RIGHT:  seam.right = seam.left + gap;   break;
    case BAR_TOP:    seam.top = seam.bottom - gap;   break;
    default:         seam.bottom = seam.top + gap;   break;
    }
    FillRect(wdc, &seam, (HBRUSH)GetStockObject(BLACK_BRUSH));
}

/* ---- per frame --------------------------------------------------------- */

static void Black(HDC wdc)
{
    HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
    int i;
    for (i = 0; i < g_nbars; i++)
        FillRect(wdc, &g_bars[i].dst, black);
}

void RF_BorderInvalidate(void)
{
    g_dirty = 1;
}

/* Called after each frame is drawn. wdc = window DC, sdc = game frame DC. */
void RF_BorderDraw(HDC wdc, HDC sdc, int gameW, int gameH, const RECT *v, int cw, int ch)
{
    DWORD now = GetTickCount();
    LARGE_INTEGER t0, t1;
    int i, sample, drew = 0;

    Layout(cw, ch, v);
    if (!g_nbars)
        return;

    if (g_cfg.border != BORDER_AMBIENT || g_failed) {
        /* black bars only need painting when something may have covered
         * them, plus a cheap safety refresh twice a second */
        if (g_dirty || now - g_lastBlack >= 500) {
            Black(wdc);
            g_dirty = 0;
            g_lastBlack = now;
        }
        return;
    }

    sample = now - g_lastSample >= UPDATE_MS;
    if (!sample && !g_dirty)
        return;                                    /* nothing to do this frame */

    QueryPerformanceCounter(&t0);
    if (sample) {
        if (!DibEnsure(&g_cap, gameW, gameH)) {
            Fail("could not create the border capture bitmap");
            Black(wdc);
            return;
        }
        for (i = 0; i < g_nbars; i++)
            Sample(&g_bars[i], sdc, gameW, gameH, v, ch);
        g_lastSample = now;
    }
    for (i = 0; i < g_nbars; i++) {
        Bar *bar = &g_bars[i];
        if (bar->valid && (g_dirty || bar->changed)) {
            Render(wdc, bar, ch);
            bar->changed = 0;
            drew = 1;
        }
    }
    g_dirty = 0;
    QueryPerformanceCounter(&t1);

    if (drew || sample) {
        t1.QuadPart -= t0.QuadPart;
        g_costSum += t1.QuadPart;
        if (t1.QuadPart > g_costMax)
            g_costMax = t1.QuadPart;
        g_costN++;
    }
}

/* For the Health log line: average and worst border update in ms, then reset. */
void RF_BorderStats(double *avgMs, double *maxMs, unsigned long *updates)
{
    LARGE_INTEGER f;
    if (!g_qpf) {
        QueryPerformanceFrequency(&f);
        g_qpf = f.QuadPart ? f.QuadPart : 1;
    }
    *updates = g_costN;
    *avgMs = g_costN ? (double)g_costSum * 1000.0 / g_qpf / g_costN : 0;
    *maxMs = (double)g_costMax * 1000.0 / g_qpf;
    g_costSum = g_costMax = 0;
    g_costN = 0;
}
