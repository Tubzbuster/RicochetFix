```
================================================================================
  SIDE-BAR BORDERS  (v0.2 - Border=ambient implemented)
================================================================================
```

> **Decision (2026-10-15):** `Border=ambient` was chosen and implemented
> (`src/border.c`) and is the default, with `Border=black` as the
> alternative. The `hangar` frame, the "RICOCHETFIX" plate and
> `Border=image` were dropped: the fix shouldn't add designs of its own to
> the games. The rest of this page is the original proposal, kept for
> reference.
>
> **Measured under Wine at 1080p:** 0.36 ms average per border update,
> 3.4 ms worst case, at most 30 updates a second. Static scenes are skipped
> entirely. Verified at 1080p, 1440p and 4K.

In borderless mode on a 16:9 monitor, the 4:3 picture leaves a black bar on
each side (240 px each at 1920x1080). This proposal fills those bars with a
frame that matches the games' look. Mockups were rendered over a real 1080p
Infinity screenshot. They aren't committed, because the screenshot is game
artwork.

```text
================================================================================
  MODES  (RicochetFix.ini, [Display] Border=)
================================================================================
  black ..... today's behaviour (plain black bars)
  hangar .... procedural gunmetal frame (proposed default)
  ambient ... blurred, dimmed stretch of the picture's own edges
  image ..... the player's own BorderImage=border.bmp, scaled into the bars
================================================================================
```

### `hangar`: what it's made of

Everything is drawn by code. No Ricochet artwork ships with the fix.

1. **Base.** Gunmetal panel with a soft vertical gradient and horizontal
   brushed-metal streaks made from noise.
2. **Hex grid.** A faint hex pattern in the accent colour at about 10%
   opacity, a nod to the Lost Worlds menu backdrop.
3. **Rail.** A recessed channel 40 px out from the picture, with a rivet every
   120 px and a small glowing accent lamp at the top and bottom.
4. **Neon strip.** A glowing accent-coloured line 12 px from the picture, a
   2 px bright core with a soft glow around it.
5. **Steel lip.** A 9 px bevelled edge against the picture, lit from the top
   left, with a dark groove, so the picture looks mounted in the frame.
6. **Plate.** A small etched "RICOCHETFIX" plate, bottom right. Optional:
   `BorderLabel=0` hides it.

**Accent colour per game**, picked from the exe's version info. It can be
overridden with `BorderAccent=RRGGBB`.

```text
  Ricochet Infinity ............. cyan          40E0E6  (matches the HUD)
  Ricochet Lost Worlds .......... logo red      D2282D
  Lost Worlds: Recharged ........ electric blue 4696FF
  unknown ....................... cyan
================================================================================
```

### How it would be built

- **Drawn once, not every frame.** The border is rendered into a cached
  bitmap (`CreateDIBSection`) only when the layout changes: startup, resize,
  Alt+Enter, a resolution or DPI change. Each frame then copies the two bar
  rectangles with `BitBlt`, which is cheaper than today's `FillRect`.
- **No leaks.** One cached bitmap and one memory DC. They're replaced only on
  a layout change, with the old ones deleted first, and freed on window
  destroy. The `Health:` log line will show GDI objects staying flat.
- **Plain GDI only.** Gradients use `GradientFill` (msimg32) or per-pixel DIB
  writes. Noise and the hex grid are drawn into the DIB. The glow is a
  pre-blurred alpha ramp blended per pixel. No new dependencies.
- **Scale-aware.** Rivet spacing, line widths and the glow scale with the bar
  height, so 1440p and 4K look the same as 1080p.
- **`ambient`** needs per-frame work: `StretchBlt` (HALFTONE) a 48 px edge
  strip into a 4x34 bitmap, then back up to the bar size and darken it. That
  costs about the same as the main picture scale, so it's not the default.
- **`image`** loads a 24/32-bit BMP with `LoadImage`, and only BMP, to stay
  dependency-free. It's scaled to the bar height and mirrored for the right
  side.
- **Windowed mode** keeps black bars, since windowed sizing is already 4:3.

### Open questions for approval

1. Is `hangar` the right default, or should the default stay `black`?
2. Keep the "RICOCHETFIX" plate, or leave it off by default?
3. Are the per-game accent colours right?
4. Is `ambient` worth including in v0.2?
