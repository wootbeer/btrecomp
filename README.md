# BattleTanx (1998 N64) Recompiled and Port

You will need the game file from a licensed copy in order to play it.

A native PC port of BattleTanx, made by statically recompiling the N64 game
with [N64Recomp](https://github.com/N64Recomp/N64Recomp) and running it on
[N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) with
[RT64](https://github.com/rt64/rt64) for rendering. It shares its engine with
the sequel, and is built the same way as
[BattleTanx: Global Assault Recompiled](https://github.com/wootbeer/btgarecomp).

Unofficial, and not affiliated with the rights holders. **No game data is
included.** You need your own dump of the game to build or play this.

## Game Files
You need your own dump of BattleTanx (USA), revision 1.0.
SHA-1 of the `.z64`:
535860D941738AC1210C20A9B80114FEA0E0FF17
`.n64` and `.v64` dumps work too; the launcher converts them.

Add a blank portable.txt in the application's folder to make portable if you want to save settings locally.

## Building
See [BUILDING.md](BUILDING.md).

## Status
Early: see [PROGRESS.md](PROGRESS.md) for what works and
[STATUS.md](STATUS.md) for the working log.

## Credits
- [N64Recomp and N64ModernRuntime](https://github.com/N64Recomp) by Mr-Wiseguy
  and contributors, which this port is built with
- [RT64](https://github.com/rt64/rt64) by Dario and contributors, the renderer
- [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) by the N64Recomp
  contributors, the launcher and input layer
- [n64sym](https://github.com/shygoo/n64sym) by shygoo, used to identify the
  library functions in the symbol file
- [rabbitizer](https://github.com/Decompollaborate/rabbitizer), the MIPS
  decoder used by the analysis scripts in `tools/`

## License
The project's own code is GPL-3.0; see [COPYING](COPYING). The game itself remains
the property of its rights holders.
