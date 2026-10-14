## RicochetFix v0.1.0

Borderless fullscreen and proper scaling for **Ricochet Infinity**, **Ricochet Lost Worlds** and **Ricochet Lost Worlds: Recharged**. One DLL works for all three.

### Install
1. Download `RicochetFix-0.1.0.zip` below.
2. Extract `winmm.dll` and `RicochetFix.ini` next to the game's exe:
   - Ricochet Infinity: `RicochetInfinity.exe`
   - Ricochet Lost Worlds: `Ricochet.exe`
   - Lost Worlds: Recharged: `RicochetRecharged.exe`
3. Play. Delete the two files to uninstall.

On Linux / Steam Deck (Wine or Proton), use the launch option `WINEDLLOVERRIDES="winmm=n,b" %command%`.

### What it does
- Borderless fullscreen at your desktop resolution, with no resolution switching
- 800x600 scaled up with the 4:3 shape kept: smooth, sharp, integer or stretch
- **Alt+Enter** switches between borderless and a resizable window
- Instant alt-tab; the mouse lines up with the scaled picture
- Fixes "Couldn't set resolution" on monitors without an 800x600 mode

### Status
- **Lost Worlds** and **Recharged** were tested under Wine: menus, mouse, Alt+Enter and exiting.
- **Infinity** loads and hooks correctly but hasn't been tested on Windows yet.
- This is the first release. Please report problems with your `RicochetFix.log` attached.

Unofficial fan project, not affiliated with Reflexive Entertainment. It contains no game code. MIT licensed and provided as is.
