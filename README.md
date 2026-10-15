```
================================================================================
 ______ _                _          _  ______ _
 | ___ (_)              | |        | | |  ___(_)       
 | |_/ /_  ___ ___   ___| |__   ___| |_| |_   ___  __
 |    /| |/ __/ _ \ / __| '_ \ / _ \ __|  _| | \ \/ /  FAQ / README / Mod Guide
 | |\ \| | (_| (_) | (__| | | |  __/ |_| |   | |>  <
 \_| \_|_|\___\___/ \___|_| |_|\___|\__\_|   |_/_/\_\
================================================================================
  Games ........ Ricochet Infinity
                 Ricochet Lost Worlds
                 Ricochet Lost Worlds: Recharged
  Platform ..... Windows (PC), Wine / Proton / Steam Deck
  Version ...... 0.2.0 
  Author ....... tubzbuster & RicochetFix contributors
  License ...... MIT (see LICENSE and DISCLAIMER.md)
  Updated ...... 2026-10-14
================================================================================

  This document is best viewed in a monospaced font at 80 columns, just like
  the good old days. Use Ctrl+F with the codes below to jump to a section.
```

```
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

```
================================================================================
[RF00]                         VERSION HISTORY
================================================================================
```

**v0.2.0 (2026-10-15)**: Ambient borders. On widescreen monitors, the bars
beside the 4:3 picture now show a soft, dimmed blur of the picture's own
edges instead of plain black (`Border=ambient`, the default; `Border=black`
keeps the old look). Nothing of our own is drawn into the game. It's sized
for 1080p and scales with your resolution. It's cheap: 0.36 ms on average
per update, and it skips static scenes. Black bars are no longer repainted
every frame.

**v0.1.0 (2026-10-14)**: First release. Code review fixes before release:
- the virtual screen surfaces now live in system memory, so they can't be
  "lost" on alt-tab, the lock screen or a UAC prompt
- no dangling surfaces when the game closes DirectDraw
- no leaked surfaces if creating the back buffer fails
- the game window is forgotten when it's destroyed, so a new one is
  picked up correctly
- only modules from the game folder are ever hooked
- the window refits after a desktop resolution or DPI change
- the mouse is never left trapped on exit
- the DLL refuses to load cleanly (instead of crashing later) if the real
  system DLL is missing
- the log has a once-a-minute health line (frames, GDI/USER objects)

**v0.0.0 (2026-10-14)**: The repo exists. Research phase.

```
================================================================================
[RF01]                  INTRODUCTION (READ THIS FIRST)
================================================================================
```
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

```
================================================================================
[RF02]                      ONE DLL, THREE GAMES
================================================================================
```

All three games run on the same Reflexive Entertainment engine. They use the
same DirectDraw 1 graphics code, the same window code and the same `.CFG`
settings file. RicochetFix works by intercepting those shared Windows and
DirectDraw calls, **not** by patching hardcoded addresses. So the **same
`winmm.dll` works for all three**, and it finds the engine automatically.

```
  GAME                         ENGINE LIVES IN     COLOUR   STATUS
  ---------------------------  ------------------  -------  ------------------
  Ricochet Infinity            Data3.bin           32-bit   [WORKING]
  Ricochet Lost Worlds         Ricochet.exe        16-bit   [WORKING]
  Ricochet LW: Recharged       Data3.bin           16-bit   [WORKING]
```

```
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

  [OK] tested   [?] hooks load fine, gameplay not yet verified
```

```
================================================================================
[RF03]                        RICOCHET INFINITY
================================================================================
  Released ..... 2007, Reflexive Entertainment
  Launcher ..... RicochetInfinity.exe   
  Engine ....... Data3.bin              
  Settings ..... Ricochet Infinity.CFG  
  Graphics ..... DirectDraw 1, 800x600, 32-bit, software rendered (AGG)
  Extras ....... Flash 8 front end, online level catalog (libcurl),
                 Raw Input for multi-mouse "Mouse Party"
  Status ....... First Pass eh whatever icba
================================================================================
```

**Install:** put `winmm.dll` and `RicochetFix.ini` next to
`RicochetInfinity.exe`.

**Things to look out for:**

- **Full Screen=0 or 1.** Both should work. With `Full Screen=1`,
  RicochetFix replaces fullscreen with borderless. With `Full Screen=0`, it
  takes over the game's own small window.
- **Mouse Party** uses Raw Input, which reports mouse *movement* rather than
  position. It isn't affected by scaling, but please report how it feels.
- **Flash screens** (intros, ads in trial builds) are drawn by a separate
  Flash child window. They may show unscaled in the top-left corner.

```
================================================================================
[RF04]                       RICOCHET LOST WORLDS
================================================================================
  Released ..... 2004, Reflexive Entertainment
  Executable ... Ricochet.exe           
  Trial DLL .... RLWres.dll             
  Settings ..... Ricochet Lost Worlds.CFG
  Graphics ..... DirectDraw 1, 800x600, 16-bit, exclusive fullscreen only
  Status ....... [WORKING]
================================================================================
```

**Install:** put `winmm.dll` and `RicochetFix.ini` next to `Ricochet.exe`.

```
================================================================================
[RF05]                 RICOCHET LOST WORLDS: RECHARGED
================================================================================
  Released ..... 2004, Reflexive Entertainment
  Launcher ..... RicochetRecharged.exe  
  Trial DLL .... Recharged.dll          
  Engine ....... Data3.bin              
  Settings ..... Ricochet Lost Worlds Recharged.CFG
  Graphics ..... DirectDraw 1, 800x600, 16-bit, exclusive fullscreen
  Status ....... [WORKING]
================================================================================
```

**Install:** put `winmm.dll` and `RicochetFix.ini` next to
`RicochetRecharged.exe`.

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

```
  WINEDLLOVERRIDES="winmm=n,b" %command%
```

```
================================================================================
[RF07]                    SETTINGS (RicochetFix.ini)
================================================================================
  KEY              VALUES                              DEFAULT
  ---------------  ----------------------------------  -----------
  Mode             borderless / windowed / off         borderless
  Scaling          smooth / sharp / integer / stretch  smooth
  Border           ambient / black                    ambient
  WindowScale      0 = auto (~85% of screen), 1, 2..   0
  VSync            1 / 0                               1
  ClipCursor       1 / 0  (keep mouse in the picture)  1
  AltEnterToggle   1 / 0                               1
  Log              1 / 0  ([Debug] section)            1
================================================================================
```

```
================================================================================
[RF08]                           HOW IT WORKS
================================================================================
```

Windows looks in the game folder for a DLL before the system folder. So our
`winmm.dll` loads instead of the real one, and passes every normal `winmm`
call straight through to it. Once loaded, it hooks the engine:

```
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

```
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
the game folder. Once a minute the log writes a `Health:` line with the frame
rate and the GDI/USER object counts. Those counts should stay flat during a
long session. If they keep climbing, please report it. Say which game it is and what you expected to see.

```
================================================================================
[RF10]                          FOR DEVELOPERS
================================================================================
```

Plain C, cross-compiled with MinGW-w64 (32-bit, because the games are
32-bit). From Linux:

```
sudo apt install gcc-mingw-w64-i686 make python3
make            # -> build/winmm.dll
make dist       # -> build/RicochetFix-<version>.zip
```

```
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

```
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
