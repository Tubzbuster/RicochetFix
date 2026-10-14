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
  RICOCHET LOST WORLDS  (GameHouse release, v1.0.19, 2004)
================================================================================
  FILE                      WHAT IT IS
  ------------------------  ----------------------------------------------------
  Ricochet.exe              THE ENGINE. Game code is inside the exe (2.4 MB,
                            image base 0x400000, extra "GARR" section).
  RLWres.dll                GameHouse trial / licence dialog. Not touched.
  Ricochet Lost Worlds.CFG  [Options] Screen Width/Height, Use Legacy Color
                            Mode. No "Full Screen" key: fullscreen only.
  Data.dat                  Graphics, sound, levels. Not needed.
================================================================================

================================================================================
  RICOCHET LOST WORLDS: RECHARGED  (GameHouse release, v1.1.29.0, 2004)
================================================================================
  FILE                      WHAT IT IS
  ------------------------  ----------------------------------------------------
  RicochetRecharged.exe     Small launcher (55 KB). Loads Data3.bin.
  Recharged.dll             GameHouse trial / licence dialog. Not touched.
  Data3.bin                 THE ENGINE. A DLL exporting StartGame, like
                            Infinity's. Imports ClipCursor/SetCursorPos too.
  Ricochet Lost Worlds Recharged.CFG   same keys as Lost Worlds + volumes
================================================================================
```

The installs we were sent also contain `ddraw.dll` = **DDrawCompat v0.7.1**
(narzoul/DDrawCompat), a third-party DirectDraw compatibility layer that
players add themselves. It isn't part of the game.

```text
================================================================================
  WHAT ALL THREE HAVE IN COMMON
================================================================================
```

- **DirectDraw 1 only.** Each engine imports just `DDRAW!DirectDrawCreate`,
  and none of them contain the GUIDs for IDirectDraw2/4/7, so they never
  `QueryInterface` for newer interfaces. Everything goes through the
  original `IDirectDraw` / `IDirectDrawSurface` vtables.
- **Startup sequence** (seen in the logs of all three):
  `SetCooperativeLevel(EXCLUSIVE|FULLSCREEN|ALLOWMODEX)`, then
  `EnumDisplayModes`, which looks for 800x600 and quits with "Couldn't set
  resolution" if it isn't listed. Then `SetDisplayMode(800, 600, bpp)` with
  16-bit for Lost Worlds and Recharged and 32-bit for Infinity. Then
  `CreateSurface(PRIMARY | FLIP | COMPLEX, 2 back buffers)`.
- **Software rendering.** Frames are drawn on the CPU (AGG in Infinity) into
  the back buffer, then flipped.
- **The same window code.** The window is created with `CreateWindowExA` as
  `WS_POPUP|WS_VISIBLE|WS_SYSMENU` with `WS_EX_APPWINDOW`, then restyled
  with `SetWindowLongA` / `SetWindowPos` / `MoveWindow`. Windowed-mode blits
  are positioned with `GetClientRect` + `ClientToScreen` + `OffsetRect`.
- **Mouse input is read in the message loop.** The engines look at
  `WM_MOUSEMOVE` / button messages as they come out of `PeekMessageA`,
  before `DispatchMessageA`. Subclassing the window procedure is **not**
  enough to remap coordinates; it has to happen at `PeekMessageA`. The
  engine draws its own cursor sprite.
- **The resolution is fixed at 800x600.** Setting `Screen Width=1024` /
  `Screen Height=768` in Infinity's CFG still gives an 800x600 picture
  (tested by tubzbuster on Windows).

```text
================================================================================
  ENGINE NOTES: INFINITY DETAIL
================================================================================
```

- The single GDI `BitBlt` in Infinity's `Data3.bin` (`0x1008D9FF`, next to
  `CreateDIBSection` at `0x1008D9AD`) copies between two *memory* DCs while
  building a frame object. It is **not** the on-screen present; that goes
  through DirectDraw in both windowed and fullscreen mode.
- Window creation is at `0x101BC774`. The windowed/fullscreen restyle is at
  `0x101BC845`-`0x101BC9B3`. `ChangeDisplaySettingsA(NULL, 0)` at
  `0x101BC975` only restores the desktop on exit.
- `CMouseRawInputWin32` loads Raw Input (`RegisterRawInputDevices` and so on)
  dynamically for Mouse Party (several mice at once).
- A Flash 8 player (`FlashPlayerControl.dll` + `Flash8.ocx`) is created as a
  `WS_CHILD` window (`CreateWindowExA` at `0x1008E348`).

```text
================================================================================
  HOW RICOCHETFIX USES THIS
================================================================================
```

1. **Loading.** It is installed as `winmm.dll` (all three engines import
   WINMM), with `ddraw.dll` as a fallback name. The engine is found by
   behaviour, as the module importing `DDRAW!DirectDrawCreate`: the exe for
   Lost Worlds, `Data3.bin` otherwise. `LoadLibraryA` is watched so a
   `Data3.bin` that loads later is still caught.
2. **Fake fullscreen.** The cooperative level is downgraded to
   `DDSCL_NORMAL`. `SetDisplayMode` is faked but remembers the depth.
   `EnumDisplayModes` gets 640x480 / 800x600 / 1024x768 at 16/32-bit added.
3. **Virtual primary.** The primary flip chain becomes two off-screen
   800x600 surfaces in the requested pixel format. `Flip`, `Blt`, `Unlock`
   and `ReleaseDC` on them trigger a present.
4. **Present.** GDI `StretchBlt` (HALFTONE or COLORONCOLOR) into the real
   window, with black bars, at the aspect-correct or integer size.
5. **Coordinates.** For the engine, its window is always 800x600 at 0,0
   (`GetClientRect`, `GetWindowRect`, identity `ClientToScreen`). Mouse
   messages and `msg.pt` are mapped at `PeekMessageA`. `SetCursorPos` and
   `ClipCursor` are mapped back to real screen pixels.
