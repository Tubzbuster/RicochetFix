## RicochetFix v0.2.0

Borderless fullscreen and proper scaling for **Ricochet Infinity**, **Ricochet Lost Worlds** and **Ricochet Lost Worlds: Recharged**. One DLL works for all three.

### New in 0.2.0: ambient borders
On widescreen monitors, the bars beside the 4:3 picture now show a soft, dimmed blur of the picture's own edges instead of plain black. Nothing of our own is drawn into the game.
- `Border=ambient` (default) or `Border=black` in `RicochetFix.ini`
- Sized for 1080p and scaled to your resolution, so it looks the same at 1440p and 4K
- Lightweight: about 0.36 ms per update on average, at most 30 updates a second, and nothing at all in static scenes
- Falls back to black bars if anything goes wrong; its cost is reported in the log's `Health:` line

Upgrading from 0.1.0: replace `winmm.dll`, and add `Border=ambient` (or `Border=black`) to your `RicochetFix.ini`. Ambient is used automatically if the line is missing.

### Install
1. Download `RicochetFix-0.2.0.zip` below.
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

Please report problems with your `RicochetFix.log` attached.

Unofficial fan project, not affiliated with Reflexive Entertainment. It contains no game code. MIT licensed and provided as is.
