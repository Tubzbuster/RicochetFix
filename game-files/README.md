```
================================================================================
  GAME FILES  -  for analysis only                        [RicochetFix/RF04]
================================================================================
```

> **!!! STOP !!!** These are copyrighted game files. **Make sure this
> repository is set to PRIVATE before you upload anything here.**
> (GitHub: Settings -> General -> Danger Zone -> Change visibility.)
> If the repo needs to stay public, put the files in a separate private repo
> instead.

```text
================================================================================
  WHAT GOES WHERE
================================================================================
  game-files/
  |-- infinity/ ........ everything from the Ricochet Infinity install folder
  |-- lost-worlds/ ..... everything from the Ricochet Lost Worlds folder
  `-- recharged/ ....... everything from the Lost Worlds: Recharged folder
================================================================================
```

Only copy the files in the checklist below. A whole `.rar` / `.zip` of the
install folder is fine too; just drop it into the matching subfolder.

```text
================================================================================
  UPLOAD CHECKLIST  (per game)
================================================================================
  [ ]  The main .exe, plus Data3.bin if there is one (Infinity and Recharged
       keep the engine in Data3.bin; Lost Worlds keeps it in the .exe)
  [ ]  Every .dll in the game folder
  [ ]  Any .ini / .cfg / .xml / .txt config files
  [ ]  files.txt  - a folder listing. In the game folder, open Command
                    Prompt and run:   dir /s > files.txt
  [ ]  version.txt - where you got the game (Steam, GOG, Reflexive's own
                     installer, a CD...) and the version, if the game shows it
================================================================================
```

```text
================================================================================
  WHAT EACH FILE IS FOR
================================================================================
```

**The `.exe`: the engine.** This is the file that matters most. We disassemble
it (open it in a tool like Ghidra and read the machine code, without running
it) to find:

- **Its imports.** These are the system DLLs it loads, such as `ddraw.dll`,
  `d3d8.dll`, `d3d9.dll`, `dinput8.dll` or `winmm.dll`. One of these names
  becomes the name of the RicochetFix proxy DLL. Getting it wrong means the
  mod never loads.
- **Window creation.** This is where it calls `CreateWindowEx` and with which
  window styles. Changing those styles is how borderless windowed works.
- **Display mode changes.** These are the `ChangeDisplaySettings` calls and
  the DirectX "set exclusive fullscreen" calls. We stop these so the game no
  longer switches your monitor's resolution.
- **Renderer setup.** This is where the back buffer is created at 800x600 or
  1024x768. We make it render at your native resolution and scale the picture
  with the correct aspect ratio.
- **Hardcoded sizes and aspect ratios.** These are the constants we'd have to
  patch in memory for true widescreen.
- **Differences between the two games.** These tell us how one DLL can tell
  Infinity and Lost Worlds apart and patch each one correctly.

**The `.dll` files: what's bundled.** These show us which libraries the game
ships with, such as a sound engine, a video codec, or a DRM/launcher wrapper
from a store. A store wrapper can change how the exe loads, so we have to
know about it. We never modify or bypass DRM.

**Config files: free settings.** The engine may already have hidden settings
like `windowed=1` or a resolution entry. If so, the fix can simply set them
instead of patching code.

**`files.txt` and `version.txt`: context.** Different releases of the same
game can have different exes. Knowing exactly which one we're looking at
means the fix can detect it and won't break other versions.

```text
================================================================================
  NOT NEEDED  -  please don't upload
================================================================================
  x  Levels and level packs
  x  Graphics, music, sound effects, videos
  x  Save games, profiles, high scores
  x  Installers / uninstallers
================================================================================
```

These files are **never** compiled into the mod or included in a release.
RicochetFix releases contain only our own DLL and an `.ini` file.
