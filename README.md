```
================================================================================
 ______ _                _          _  ______ _
 | ___ (_)              | |        | | |  ___(_)       Ricochet, but in 2026.
 | |_/ /_  ___ ___   ___| |__   ___| |_| |_   ___  __
 |    /| |/ __/ _ \ / __| '_ \ / _ \ __|  _| | \ \/ /  FAQ / README / Mod Guide
 | |\ \| | (_| (_) | (__| | | |  __/ |_| |   | |>  <
 \_| \_|_|\___\___/ \___|_| |_|\___|\__\_|   |_/_/\_\
================================================================================
  Games ........ Ricochet Infinity, Ricochet Lost Worlds (+ Recharged)
  Platform ..... Windows (PC), Wine/Proton should work too
  Version ...... 0.0.0 (nothing to download yet, research phase)
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
  [RF02] ........ Planned Features
  [RF03] ........ How It Works (The Proxy DLL)
  [RF04] ........ The Game Files (What They're For)
  [RF05] ........ Installation
  [RF06] ........ For Developers
  [RF07] ........ Disclaimer & Legal
  [RF08] ........ Credits
================================================================================
```

```text
================================================================================
[RF00]                         VERSION HISTORY
================================================================================
```

**v0.0.0 (2026-10-14)**: The repo exists. We've set up the folder that the
game files go in so they can be analysed. No mod to download yet.

```text
================================================================================
[RF01]                  INTRODUCTION (READ THIS FIRST)
================================================================================
```

> **!!! WARNING !!!** RicochetFix is in the **research phase**. There is
> nothing to install yet. This README describes the plan.

**The goal:** Ricochet Infinity and Ricochet Lost Worlds are still two of the
best brick breakers ever made. But they're from the 4:3, 1024x768 era, so on
a modern PC you get exclusive fullscreen that minimises when you alt-tab,
a stretched or tiny picture on widescreen monitors, and no borderless
windowed mode at all.

**What RicochetFix is:** a single drag-and-drop DLL. Drop it in the game
folder next to the `.exe` and the game loads it automatically. There's no
launcher, no injector and no patching of the game files on disk. Delete the
DLL and the game is exactly as it was.

**Both games, one fix:** Infinity and Lost Worlds run on the same Reflexive
Entertainment engine. The DLL works out which game it has been loaded into
and applies the matching patches.

```text
================================================================================
[RF02]                        PLANNED FEATURES
================================================================================
```

```text
  FEATURE                                   STATUS
  ----------------------------------------  ------------------------------
  Borderless windowed (fullscreen window)    [PLANNED]
  Plain windowed mode, any size              [PLANNED]
  Native desktop resolution rendering        [PLANNED]
  Correct aspect ratio (pillarboxed 4:3)     [PLANNED]
  Sharp scaling (no blurry stretch)          [PLANNED]
  Alt-tab without minimising / crashing      [PLANNED]
  RicochetFix.ini for all settings           [PLANNED]
  Log file for troubleshooting               [PLANNED]
  True widescreen (use the side space)       [RESEARCH]   depends on engine
  Ricochet Infinity support                  [RESEARCH]
  Ricochet Lost Worlds support               [RESEARCH]
  Ricochet Recharged support                 [MAYBE]      same engine
```

The playfield in these games is almost certainly a fixed 4:3 layout. So the
first goal is a **sharp, correctly proportioned picture at your native
resolution**, with black bars at the sides. True widescreen, where the extra
side space is actually used, depends on how the renderer and UI are built.
We'll only know once the executables have been analysed.

```text
================================================================================
[RF03]                  HOW IT WORKS (THE PROXY DLL)
================================================================================
```

When a Windows game starts, it loads the system DLLs it needs, such as
`ddraw.dll`, `d3d8.dll`, `d3d9.dll`, `dinput8.dll` or `winmm.dll`. Windows
looks in the game's own folder **before** the system folder. So a DLL with the
same name placed next to the `.exe` gets loaded instead. That's a
**proxy DLL**:

```text
   Ricochet Infinity.exe
          |
          |  "load d3dX.dll please"
          v
   +--------------------+        forwards every call        +----------------+
   |  RicochetFix  DLL  |  ------------------------------>  |  the real      |
   |  (game folder)     |                                   |  system DLL    |
   +--------------------+                                   +----------------+
          |
          +-- hooks window creation ......... borderless / windowed
          +-- hooks display mode changes .... no more resolution switching
          +-- hooks the renderer setup ...... render at native res, scale to fit
          +-- patches engine code in memory . widescreen / aspect (if possible)
```

Which DLL name we use depends on what the game imports. Finding that out is
step one of the analysis in [RF04].

```text
================================================================================
[RF04]               THE GAME FILES (WHAT THEY'RE FOR)
================================================================================
```

To build the fix we need to look at the game's own code. **The game files are
only used for analysis**: disassembling the executable to find where it
creates its window, sets the display mode and sets up the renderer. Full
details, the folder layout and an upload checklist are in
[game-files/README.md](game-files/README.md).

```text
  FILE TYPE                WHY WE NEED IT
  -----------------------  --------------------------------------------------
  The game .exe            The engine itself. Where every hook and patch goes.
  .dll files in the folder Tells us what the game loads (DirectX version,
                           sound library, DRM wrapper, etc.).
  .ini / .cfg / .xml       Existing resolution / windowed settings we can
                           reuse instead of patching.
  Folder listing           So we know the install layout and file versions.
  Level / art / sound data NOT NEEDED. Please don't upload it.
```

> **!!! IMPORTANT !!!** The games are copyrighted by their owners. **Only
> upload the game files to a PRIVATE repository**, and never include them in
> a release. RicochetFix releases will only ever contain our own DLL.

```text
================================================================================
[RF05]                           INSTALLATION
================================================================================
```

Nothing to install yet. When there is, it will look like this:

```text
  1. Download RicochetFix-x.y.z.zip from the Releases page.
  2. Extract the DLL and RicochetFix.ini into the game folder,
     next to "Ricochet Infinity.exe" or "Ricochet Lost Worlds.exe".
  3. (Optional) edit RicochetFix.ini to pick borderless/windowed and size.
  4. Play. To uninstall, delete the two files.
```

```text
================================================================================
[RF06]                          FOR DEVELOPERS
================================================================================
```

The plan is a small C/C++ DLL that can be cross-compiled from Linux with
MinGW (`i686-w64-mingw32`, because the games are 32-bit) or built on Windows
with Visual Studio. The build instructions will go here once there's code.

```text
  RicochetFix/
  |-- README.md ............ you are here
  |-- DISCLAIMER.md ........ the legal bit
  |-- LICENSE .............. MIT
  |-- docs/ENGINE-NOTES.md . what the analysis found
  `-- game-files/ .......... PRIVATE ONLY: game binaries for analysis
      |-- README.md ........ what goes here and why
      |-- infinity/ ........ Ricochet Infinity files
      `-- lost-worlds/ ..... Ricochet Lost Worlds files
```

Pull requests are welcome. It's early days, so open an issue first for
anything big.

```text
================================================================================
[RF07]                         DISCLAIMER & LEGAL
================================================================================
```

RicochetFix is free, open-source software released under the
[MIT License](LICENSE). It is provided **"AS IS", WITHOUT WARRANTY OF ANY
KIND**. You use it at your own risk. The authors and contributors aren't
liable for any damage, data loss, lost save games, or other consequences of
using it.

RicochetFix is an **unofficial, fan-made project**. It isn't affiliated with,
endorsed by, sponsored by, or approved by Reflexive Entertainment, Amazon, or
any publisher or store of the Ricochet games. It contains no game code or
artwork and doesn't circumvent DRM or any other technical protection. You
need your own legitimate copy of the game.

Read the full text in [DISCLAIMER.md](DISCLAIMER.md).

```text
================================================================================
[RF08]                              CREDITS
================================================================================
  * Reflexive Entertainment ............ for making the best brick breakers
  * The Ricochet level-making community . for keeping the games alive
  * dgVoodoo2, DxWnd & ThirteenAG ...... for showing old games can be saved
  * GameFAQs FAQ writers of the 2000s ... for the vibes
================================================================================
        This document Copyright (c) 2026 tubzbuster & RicochetFix contributors
                 Released under the MIT License. Go break some bricks.
================================================================================
```

<br>

<p align="center"><sub><sup>
Ricochet, Ricochet Infinity, Ricochet Lost Worlds and Ricochet Recharged are trademarks of their respective owners.
All other trademarks are the property of their respective owners and are used here for identification purposes only.
RicochetFix is not affiliated with, endorsed by, or sponsored by any of these companies.
</sup></sub></p>
