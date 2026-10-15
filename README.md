# BattleTanx (1998 N64) Recompiled and Port

You will need the game file from a licensed copy in order to play it.

A native PC port of BattleTanx, made by statically recompiling the N64 game
with [N64Recomp](https://github.com/N64Recomp/N64Recomp) and running it on
[N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) with
[RT64](https://github.com/rt64/rt64) for rendering. It uses the same engine as
the sequel, so it's built the same way as
[BattleTanx: Global Assault Recompiled](https://github.com/wootbeer/btgarecomp).

Unofficial, and not affiliated with the rights holders. **No game data is
included.** You need your own dump of the game to build or play this.

## Features
- Runs natively on Windows and Linux, including the Steam Deck.
- Widescreen. The HUD moves out to the edges of the screen.
- High framerate. Set Framerate to Display in the graphics settings to
  match your monitor.
- Controller Pak saves.
- Keyboard and controller support, all remappable. Fire (Z) is on the right
  trigger by default.

## Game Files
You need your own data file, BattleTanx (USA).
SHA-1 of the `.z64`:
535860D941738AC1210C20A9B80114FEA0E0FF17
`.n64` and `.v64` files work too.

Start the game, pick **Load ROM** and select your file. It only has to be
done once.

Add a blank portable.txt in the application's folder to make it portable,
if you want to save settings and saves locally.

## Download
Get the latest build from the
[Releases](https://github.com/wootbeer/btrecomp/releases) page.
- **Windows:** unzip it anywhere and run `BattleTanxRecompiled.exe`.
- **Linux / Steam Deck:** extract it and run `BattleTanxRecompiled`. On the
  Deck you can add it to Steam as a non-Steam game to play in Game Mode.

## Building
See [BUILDING.md](BUILDING.md).

## Issues and Limitations

Known:
- Beta. Tested on Windows.

Else:
- Please see the "Issues" section in GitHub. Screenshots or a short video
  help a lot.
- If the game crashes, attach `crash_log.txt` from the game's folder.

## Changes
See [CHANGELOG.md](CHANGELOG.md).

## Credits
- [N64Recomp and N64ModernRuntime](https://github.com/N64Recomp) by Mr-Wiseguy
  and contributors, which this port is built with
- [RT64](https://github.com/rt64/rt64) by Dario and contributors, the renderer
- [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) by the N64Recomp
  contributors, the launcher and input layer
- [n64sym](https://github.com/shygoo/n64sym) by shygoo, used to name the
  library functions
- [rabbitizer](https://github.com/Decompollaborate/rabbitizer), the MIPS
  decoder used by the scripts in `tools/`

## License
The project's own code is GPL-3.0; see [COPYING](COPYING). Third-party
licenses ship in the `licenses` folder of each release. The game itself
remains the property of its rights holders.
