```
================================================================================
  ENGINE NOTES  -  what's inside Ricochet                 [RicochetFix/RF04]
================================================================================
```

Notes from the static analysis of the game files. These cover addresses,
imports and behaviour only. No game code or data is stored in this repo.

```text
================================================================================
  RICOCHET INFINITY  (Reflexive Arcade release, 2007)
================================================================================
  FILE                      WHAT IT IS
  ------------------------  ----------------------------------------------------
  RicochetInfinity.exe      Tiny (160 KB) launcher. Loads Data3.bin and calls
                            its StartGame export. No game code at all.
  RicochetInfinity.RWG      Original launcher with the Reflexive Arcade DRM
                            wrapper (.text section is encrypted).
  Data3.bin                 THE ENGINE. A 32-bit DLL (image base 0x10000000,
                            exports StartGame). Every hook goes in here.
  ReflexiveArcade/*.dll     Reflexive Arcade store / trial / unlock system.
                            Not touched by RicochetFix.
  FlashPlayerControl.dll,   Embedded Flash player (menus / ads).
  Flash8.ocx
  libcurl.dll               Online level catalog / downloads.
  Ricochet Infinity.CFG     Settings file. See "Built-in settings" below.
  Data1 / Data2 / *.dat     Graphics, sound, levels. Not needed.
================================================================================
```

**Engine build:** `Data3.bin`, PE timestamp `2007-08-17 15:58:43`, SizeOfImage
`0x4BE000`, SHA-256
`1af0528eb3259438ecac7c18420d54507419477abcef446047caf0e7508a11d1`.
The DLL uses this to recognise the build before patching anything.

```text
================================================================================
  HOW IT DRAWS
================================================================================
```

- **The engine renders in software.** Every frame is drawn on the CPU into
  an 800x600 frame buffer, using the Anti-Grain Geometry (AGG) library
  (`CFrame_BlitX_AGG`, `CAggRenderingBufferFrameProperty`, `CBlurRenderer`).
  There is no Direct3D at all.
- **Windowed mode** presents each frame through GDI: one `CreateDIBSection`
  (`0x1008D9AD`) and a single `BitBlt` to the window (`0x1008D9FF`).
- **Fullscreen mode** uses DirectDraw (`DirectDrawCreate`, the only DDRAW
  import) to switch the monitor to the game's resolution and blit the frame.
- **Window creation** is at `0x101BC774`. The class is registered just before
  it. The window is created as `WS_EX_APPWINDOW` with
  `WS_POPUP | WS_VISIBLE | WS_SYSMENU` (`0x90080000`). The windowed/fullscreen
  switch then restyles it with `AdjustWindowRectEx`, `SetWindowLongA` and
  `SetWindowPos` (`0x101BC845`-`0x101BC9B3`).
- `ChangeDisplaySettingsA(NULL, 0)` at `0x101BC975` only *restores* the
  desktop mode on exit.
- Mouse input arrives as normal window messages (`WM_MOUSEMOVE` etc.) and
  goes through `ClientToScreen` / `GetClientRect`.

```text
================================================================================
  BUILT-IN SETTINGS  (Ricochet Infinity.CFG)
================================================================================
  [Options]
  Screen Width=800
  Screen Height=600
  Full Screen=1
  ...
================================================================================
```

The engine also reads `/ScreenWidth`, `/ScreenHeight` and `/ScreenBitDepth`
command-line style keys, and has a `BAD SCREEN MODE` error. It's still
unknown whether sizes other than 800x600 actually work or just break the
layout. **This needs testing on a real PC.**

```text
================================================================================
  WHAT THIS MEANS FOR THE FIX
================================================================================
```

1. **Proxy DLL name.** `Data3.bin` imports `DDRAW.dll` and `WINMM.dll` from
   the game folder search path. `winmm.dll` is the safest proxy because it
   doesn't clash with dgVoodoo2, which also uses `ddraw.dll`. The DLL hooks
   `Data3.bin`'s imports (IAT) once it has been loaded.
2. **Borderless windowed.** Force the engine's own windowed mode, then
   resize its window to cover the monitor as a `WS_POPUP`. The game keeps
   drawing 800x600. No DirectDraw mode switch happens, so alt-tab is instant.
3. **Scaling.** Replace the single windowed-mode `BitBlt` with a scaled
   present. That means aspect-correct 4:3 with black bars, a choice of
   sharp/smooth/integer scaling, and possibly a GPU (D3D11) present later for
   quality and speed.
4. **Mouse.** Map mouse message coordinates from the large window back into
   the game's 800x600 space, and make `GetClientRect` report 800x600, so the
   menus and paddle still line up.
5. **True widescreen** would need the engine to lay out a wider frame buffer.
   Because the playfield and UI are built for 800x600, this stays a research
   item until the CFG resolution test above is done.
