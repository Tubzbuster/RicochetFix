```
================================================================================
 ______ _                _          _  ______ _
 | ___ (_)              | |        | | |  ___(_)       Ricochet, but in 2026.
 | |_/ /_  ___ ___   ___| |__   ___| |_| |_   ___  __
 |    /| |/ __/ _ \ / __| '_ \ / _ \ __|  _| | \ \/ /  FAQ / README / Mod Guide
 | |\ \| | (_| (_) | (__| | | |  __/ |_| |   | |>  <
 \_| \_|_|\___\___/ \___|_| |_|\___|\__\_|   |_/_/\_\
================================================================================
  Games ........ Ricochet Infinity
                 Ricochet Lost Worlds
                 Ricochet Lost Worlds: Recharged
  Platform ..... Windows (PC), Wine / Proton / Steam Deck
  Version ...... 0.1.0 (first build, needs testing on real Windows PCs)
  Author ....... tubzbuster & RicochetFix contributors
  License ...... MIT (see LICENSE and DISCLAIMER.md)
  Updated ...... 2026-10-14
================================================================================

  This document is best viewed in a monospaced font at 80 columns, just like
  the good old days. Use Ctrl+F with the codes below to jump to a section.
```

```text
================================================================================
                           TABLE OF CONTENTS
================================================================================
  [RF00] ........ Version History
  [RF01] ........ Introduction (READ THIS FIRST)
  [RF02] ........ One DLL, Three Games
  [RF03] ........ Ricochet Infinity
  [RF04] ........ Ricochet Lost Worlds
  [RF05] ........ Ricochet Lost Worlds: Recharged
  [RF06] ........ Installation
  [RF07] ........ Settings (RicochetFix.ini)
  [RF08] ........ How It Works
  [RF09] ........ Troubleshooting
  [RF10] ........ For Developers
  [RF11] ........ Disclaimer & Legal
  [RF12] ........ Credits
================================================================================
```

```text
================================================================================
[RF00]                         VERSION HISTORY
================================================================================
```

**v0.1.0 (2026-10-14)**: First build. A single `winmm.dll` adds borderless
fullscreen, scaling to any resolution with the 4:3 shape kept, a resizable
windowed mode (Alt+Enter) and correct mouse mapping to all three games.
Lost Worlds and Recharged were tested end to end under Wine. Infinity loads
and hooks correctly but still needs a real Windows test.

**v0.0.0 (2026-10-14)**: The repo exists. Research phase.

```text
================================================================================
[RF01]                  INTRODUCTION (READ THIS FIRST)
================================================================================
```

> **!!! WARNING !!!** This is the **first build**. It has been tested under
> Wine on Linux, but not yet on a real Windows PC. Please try it and report
> back with your `RicochetFix.log` (see [RF09]).

**The problem:** The Ricochet games are still some of the best brick
breakers ever made. But they are locked to **800x600**, and in fullscreen they
switch your whole monitor to that resolution. That means a blurry or
stretched picture on a modern screen, slow and flickery alt-tabbing, and no
borderless mode. Windowed mode is a tiny 800x600 box.

**The fix:** RicochetFix is **one drag-and-drop DLL**. Put it in the game
folder and the game:

- runs **borderless** across your whole monitor, with no resolution switch
- is **scaled** to your native resolution with the correct 4:3 shape (black
  bars on widescreen monitors), using smooth, sharp or integer scaling
- switches to a **resizable window** and back with **Alt+Enter**
- **alt-tabs instantly** and never minimises itself
- keeps **mouse clicks lined up** with the scaled picture

Nothing on disk is patched. Delete the DLL and the game is exactly as it was.

**Why not true widescreen?** All three games always draw an 800x600 picture.
Testing showed the engine ignores other sizes in its `.CFG` file. Every
menu and level is built for that 4:3 canvas, so there's nothing to show in
the extra side space. Sharp, correctly shaped scaling is the best result.

```text
================================================================================
[RF02]                      ONE DLL, THREE GAMES
================================================================================
```

All three games run on the same Reflexive Entertainment engine. They use the
same DirectDraw 1 graphics code, the same window code and the same `.CFG`
settings file. RicochetFix works by intercepting those shared Windows and
DirectDraw calls, **not** by patching hardcoded addresses. So the **same
`winmm.dll` works for all three**, and it finds the engine automatically.

```text
  GAME                         ENGINE LIVES IN     COLOUR   STATUS
  ---------------------------  ------------------  -------  ------------------
  Ricochet Infinity            Data3.bin           32-bit   [NEEDS WIN TEST]
  Ricochet Lost Worlds         Ricochet.exe        16-bit   [WORKING - Wine]
  Ricochet LW: Recharged       Data3.bin           16-bit   [WORKING - Wine]
```

```text
  FEATURE                                   INF      LW       LWR
  ----------------------------------------  -------  -------  -------
  Borderless fullscreen, no mode switch     [?]      [OK]     [OK]
  Scaled to native res, 4:3 kept            [?]      [OK]     [OK]
  Smooth / sharp / integer scaling          [?]      [OK]     [OK]
  Mouse lines up with scaled picture        [?]      [OK]     [OK]
  Alt+Enter borderless <-> windowed         [?]      [OK]     [OK]
  Resizable window                          [?]      [OK]     [OK]
  Missing 800x600 mode on monitor handled   [OK]     [OK]     [OK]
  True widescreen                           [NO]     [NO]     [NO]

  [OK] tested under Wine   [?] hooks load fine, gameplay not yet verified
```

```text
================================================================================
[RF03]                        RICOCHET INFINITY
================================================================================
  Released ..... 2007, Reflexive Entertainment
  Launcher ..... RicochetInfinity.exe   (tiny loader, no game code)
  Engine ....... Data3.bin              (a DLL, exports StartGame)
  Settings ..... Ricochet Infinity.CFG  (Screen Width/Height, Full Screen)
  Graphics ..... DirectDraw 1, 800x600, 32-bit, software rendered (AGG)
  Extras ....... Flash 8 front end, online level catalog (libcurl),
                 Raw Input for multi-mouse "Mouse Party"
  Status ....... [NEEDS WINDOWS TEST]
================================================================================
```

**Install:** put `winmm.dll` and `RicochetFix.ini` next to
`RicochetInfinity.exe`.

**What we know:** RicochetFix finds the engine in `Data3.bin`, takes over
its window and fakes the 800x600 32-bit fullscreen mode. All of that is
confirmed in the log. Under Wine the game itself only shows a blank white
screen, with or without RicochetFix, so the menus couldn't be checked there.

**Things to look out for:**

- **Full Screen=0 or 1.** Both should work. With `Full Screen=1`,
  RicochetFix replaces fullscreen with borderless. With `Full Screen=0`, it
  takes over the game's own small window.
- **Mouse Party** uses Raw Input, which reports mouse *movement* rather than
  position. It isn't affected by scaling, but please report how it feels.
- **Flash screens** (intros, ads in trial builds) are drawn by a separate
  Flash child window. They may show unscaled in the top-left corner.

```text
================================================================================
[RF04]                       RICOCHET LOST WORLDS
================================================================================
  Released ..... 2004, Reflexive Entertainment
  Executable ... Ricochet.exe           (the engine is inside the exe)
  Trial DLL .... RLWres.dll             (GameHouse trial / licence screen)
  Settings ..... Ricochet Lost Worlds.CFG
  Graphics ..... DirectDraw 1, 800x600, 16-bit, exclusive fullscreen only
  Status ....... [WORKING - tested under Wine]
================================================================================
```

**Install:** put `winmm.dll` and `RicochetFix.ini` next to `Ricochet.exe`.

**Tested:** the licence dialog, then borderless at 1920x1080 with 4:3
pillarboxing. The game's own cursor lines up with the real mouse, menus
respond to clicks, and Alt+Enter toggles windowed and back.

**Notes:**

- This game has **no windowed mode of its own**. It always switches the
  monitor to 800x600 16-bit. RicochetFix fakes that switch, so the game
  thinks it got 800x600 fullscreen while it really runs in a borderless
  window at your desktop resolution.
- If your monitor doesn't offer 800x600, the original game quits with
  "Couldn't set resolution". RicochetFix fixes that by adding the classic
  640x480 / 800x600 / 1024x768 modes to the list the game sees.
- Many installs include **DDrawCompat** as `ddraw.dll`. RicochetFix (as
  `winmm.dll`) can run alongside it. If you have problems, try temporarily
  renaming `ddraw.dll` to `ddraw.dll.off`.

```text
================================================================================
[RF05]                 RICOCHET LOST WORLDS: RECHARGED
================================================================================
  Released ..... 2004, Reflexive Entertainment
  Launcher ..... RicochetRecharged.exe  (small loader)
  Trial DLL .... Recharged.dll          (GameHouse trial / licence screen)
  Engine ....... Data3.bin              (a DLL, exports StartGame)
  Settings ..... Ricochet Lost Worlds Recharged.CFG
  Graphics ..... DirectDraw 1, 800x600, 16-bit, exclusive fullscreen
  Status ....... [WORKING - tested under Wine]
================================================================================
```

**Install:** put `winmm.dll` and `RicochetFix.ini` next to
`RicochetRecharged.exe`.

**Tested:** the licence dialog, then borderless with scaling, mouse-accurate
menus, the Select Player screen, and Alt+Enter windowed/borderless
switching.

**Notes:** Recharged is a halfway point between the other two. It uses the
Lost Worlds graphics and gameplay, but the engine sits in `Data3.bin` like
Infinity. RicochetFix finds it automatically when the launcher loads it. The
DDrawCompat note from [RF04] applies here too.

```text
================================================================================
[RF06]                           INSTALLATION
================================================================================
```

```text
  1. Download RicochetFix-x.y.z.zip from the Releases page (or build it,
     see [RF10]).
  2. Extract winmm.dll and RicochetFix.ini into the game folder:

       Ricochet Infinity ............ next to RicochetInfinity.exe
       Ricochet Lost Worlds ......... next to Ricochet.exe
       Lost Worlds: Recharged ....... next to RicochetRecharged.exe

  3. Start the game normally. A RicochetFix.log file appearing in the
     folder means it's working.
  4. To uninstall, delete winmm.dll and RicochetFix.ini.
```

**Linux / Steam Deck (Wine or Proton):** Wine prefers its own `winmm.dll`,
so tell it to load ours. In Steam, set the game's launch options to:

```text
  WINEDLLOVERRIDES="winmm=n,b" %command%
```

```text
================================================================================
[RF07]                    SETTINGS (RicochetFix.ini)
================================================================================
  KEY              VALUES                              DEFAULT
  ---------------  ----------------------------------  -----------
  Mode             borderless / windowed / off         borderless
  Scaling          smooth / sharp / integer / stretch  smooth
  WindowScale      0 = auto (~85% of screen), 1, 2..   0
  VSync            1 / 0                               1
  ClipCursor       1 / 0  (keep mouse in the picture)  1
  AltEnterToggle   1 / 0                               1
  Log              1 / 0  ([Debug] section)            1
================================================================================
```

```text
================================================================================
[RF08]                           HOW IT WORKS
================================================================================
```

Windows looks in the game folder for a DLL before the system folder. So our
`winmm.dll` loads instead of the real one, and passes every normal `winmm`
call straight through to it. Once loaded, it hooks the engine:

```text
   game  ---- DirectDrawCreate / SetDisplayMode / CreateSurface ---->  ddraw
     |             |
     |             +-- RicochetFix: "sure, you're now 800x600 fullscreen"
     |                 (it isn't: the monitor stays at its own resolution)
     |
     |   draws its 800x600 frame into a *virtual* screen surface
     v
   RicochetFix scales that frame into a borderless window
   (4:3, black bars, smooth/sharp/integer), and maps the mouse back
   from screen pixels to the game's 800x600 pixels.
```

- **DirectDraw.** Exclusive fullscreen is turned into normal windowed
  DirectDraw. `SetDisplayMode` is faked in the colour depth the game asked
  for (16 or 32-bit). The "primary surface" and its back buffer become
  ordinary 800x600 off-screen surfaces that RicochetFix shows on screen.
- **Window.** The game's own window is restyled as borderless fullscreen or
  a normal resizable window. The game is told its window is still 800x600.
- **Mouse.** The engines read mouse messages directly from their message
  loop, so coordinates are converted there, as they come out of the queue.
- **Missing modes.** The classic 4:3 modes are added to the mode list if
  your monitor doesn't report them.

Engine notes from the reverse engineering are in
[docs/ENGINE-NOTES.md](docs/ENGINE-NOTES.md).

```text
================================================================================
[RF09]                         TROUBLESHOOTING
================================================================================
  PROBLEM                         TRY THIS
  ------------------------------  ----------------------------------------------
  No RicochetFix.log appears      The DLL isn't loading. Check it is next to
                                  the right .exe (see [RF06]). Or rename
                                  winmm.dll to ddraw.dll (both names work, but
                                  not if DDrawCompat/dgVoodoo is ddraw.dll).
  Black or garbled picture        Send the log. Try Mode=windowed. Try without
                                  DDrawCompat / dgVoodoo.
  Mouse is offset                 Send the log and your screen resolution and
                                  Windows display scaling (100%, 150%...).
  Picture is blurry               Scaling=sharp or Scaling=integer.
  Game runs too fast / slow       Try VSync=0 or VSync=1 and report it.
  Want the old behaviour back     Mode=off, or delete winmm.dll.
================================================================================
```

When reporting a problem, open an issue and attach `RicochetFix.log` from
the game folder. Say which game it is and what you expected to see.

```text
================================================================================
[RF10]                          FOR DEVELOPERS
================================================================================
```

Plain C, cross-compiled with MinGW-w64 (32-bit, because the games are
32-bit). From Linux:

```bash
sudo apt install gcc-mingw-w64-i686 make python3
make            # -> build/winmm.dll
make dist       # -> build/RicochetFix-<version>.zip
```

```text
  RicochetFix/
  |-- README.md ............. you are here
  |-- RicochetFix.ini ....... default settings shipped with the DLL
  |-- Makefile .............. build
  |-- src/
  |   |-- main.c ............ DllMain, finds the engine module
  |   |-- ddraw_hooks.c ..... fake fullscreen, virtual primary surface
  |   |-- window.c .......... borderless/windowed, scaling, mouse mapping
  |   |-- proxy.c ........... forwards winmm/ddraw exports to the real DLL
  |   |-- iat.c ............. import table patching
  |   |-- config.c, log.c ... RicochetFix.ini and RicochetFix.log
  |   `-- *_exports.txt ..... export lists (tools/gen_proxy.py -> stubs)
  |-- dist/README.txt ....... readme shipped in the release zip
  |-- docs/ENGINE-NOTES.md .. reverse engineering notes
  `-- game-files/ ........... PRIVATE ONLY: game binaries for analysis
```

Pull requests are welcome. It's early days, so open an issue first for
anything big.

```text
================================================================================
[RF11]                         DISCLAIMER & LEGAL
================================================================================
```

RicochetFix is free, open-source software released under the
[MIT License](LICENSE). It is provided **"AS IS", WITHOUT WARRANTY OF ANY
KIND**. You use it at your own risk. The authors and contributors aren't
liable for any damage, data loss, lost save games, or other consequences of
using it.

RicochetFix is an **unofficial, fan-made project**. It isn't affiliated with,
endorsed by, sponsored by, or approved by Reflexive Entertainment, Amazon,
GameHouse, or any publisher or store of the Ricochet games. It contains no
game code or artwork and doesn't circumvent DRM, trial limits or any other
technical protection. You need your own legitimate copy of the game.

Read the full text in [DISCLAIMER.md](DISCLAIMER.md).

```text
================================================================================
[RF12]                              CREDITS
================================================================================
  * Reflexive Entertainment ............ for making the best brick breakers
  * The Ricochet level-making community . for keeping the games alive
  * DDrawCompat, dgVoodoo2, DxWnd ...... for showing old games can be saved
  * The Wine project ................... for being our test bench
  * GameFAQs FAQ writers of the 2000s ... for the vibes
================================================================================
        This document Copyright (c) 2026 tubzbuster & RicochetFix contributors
                 Released under the MIT License. Go break some bricks.
================================================================================
```

<br>

<p align="center"><sub><sup>
Ricochet, Ricochet Infinity, Ricochet Lost Worlds and Ricochet Lost Worlds: Recharged are trademarks of their respective owners.
All other trademarks are the property of their respective owners and are used here for identification purposes only.
RicochetFix is not affiliated with, endorsed by, or sponsored by any of these companies.
</sup></sub></p>
