================================================================================
  RicochetFix - borderless, scaled Ricochet for modern PCs
================================================================================
  Works with: Ricochet Infinity
              Ricochet Lost Worlds
              Ricochet Lost Worlds: Recharged
  Project:    https://github.com/Tubzbuster/RicochetFix
================================================================================

INSTALL
-------
  1. Copy winmm.dll and RicochetFix.ini into the game folder:

       Ricochet Infinity ........ next to RicochetInfinity.exe
       Ricochet Lost Worlds ..... next to Ricochet.exe
       Lost Worlds: Recharged ... next to RicochetRecharged.exe

  2. Start the game the way you normally do. That's it.

  To uninstall, delete winmm.dll and RicochetFix.ini.

WHAT IT DOES
------------
  * Runs the game in a borderless window covering your whole screen instead
    of switching your monitor to 800x600.
  * Scales the picture up to your resolution, keeping the 4:3 shape (black
    bars at the sides on a widescreen monitor).
  * Alt+Enter switches between borderless and a normal resizable window.
  * Alt-tab is instant and never minimises the game.
  * Mouse clicks line up with the scaled picture.

Settings are in RicochetFix.ini (open it with Notepad).

TROUBLESHOOTING
---------------
  * Nothing changed? Check that RicochetFix.log appears in the game folder
    after starting the game. If it doesn't, the DLL isn't being loaded.
    Rename winmm.dll to ddraw.dll and try again (both names work).
  * If you use DDrawCompat or dgVoodoo2 (they install as ddraw.dll), keep
    RicochetFix as winmm.dll. Try with and without them.
  * Linux / Steam Deck (Wine or Proton): set the launch option
      WINEDLLOVERRIDES="winmm=n,b" %command%
  * Anything else: open an issue on GitHub and attach RicochetFix.log.

RicochetFix is unofficial and not affiliated with Reflexive Entertainment.
It contains no game code or data. You need your own copy of the game.
Released under the MIT License, provided as is, without warranty.
