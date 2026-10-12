# Status

The project's working log, newest first, as "rounds" (the same format as
the sequel's port, [btgarecomp](https://github.com/wootbeer/btgarecomp)'s
STATUS.md, which this project refers to often: the two games share an
engine). PROGRESS.md has the short version.

# Log

## 2026-10-12, round 2: in game; widescreen; Linux and Steam Deck packaging

**Tested in the development sandbox** (Linux, Xvfb, Mesa's llvmpipe
software Vulkan, driven with xdotool): the launcher with **Load ROM**,
then boot, 3DO logo, title screen, the attract intro, Controller Pak note
creation, Game Setup, the campaign story intro, and mission 1 ("Ground
Zero") with the HUD, firing and explosions. No hangs, no crashes. Round 1
had one fix to make first: the entry point's symbol included the `break`
after its call, so when the boot function returned the runtime stopped on
it. `tools/find_functions.py` now ends a path at a `break` (a trap that
never runs: IDO's divide-by-zero checks and this one).

**Screen-edge scissor** (`src/game/screen_edge_scissor_fix.cpp`;
btgarecomp rounds 94 and 97). The same off-by-one as the sequel: the
viewport display lists scissor to 319 x 239, leaving the last column and
row undrawn, and RT64's Expand won't widen a view whose scissor doesn't
span the screen. The lists are a 0x320-byte block at ROM `0xD7040` (just
after the code), copied by `func_8007D4E8` to `0x803C8880`, with its address
at `0x801257C0`; the six edge scissors are at the sequel's offsets. Fixed in
the copy once per VI from the `func_80079530` patch.

**Widescreen culling** (`src/game/widescreen.cpp`; btgarecomp round 98).
With the scissor fixed, RT64 widens the 3D view in Expand, but the game
culled against its 4:3 cone. Here the cull factor is one global
(`0x8012585C`, set per player count by `func_80081AD0` from the same
2.2 / 1.12 / 1.12 / 2.29 table the sequel uses, at `0x80071490`) rather than
a camera field. `func_8008228C` builds each player's cull record and passes
k on the stack; a hook where both of its paths meet (`0x800823D0`) divides
k by the Expand factor for full-width views (1 player, both 2-player
halves, the 3-player top view). Confirmed in the sandbox: the world reaches
the window edges. The HUD is still in the centred 4:3 area.

**Linux and Steam Deck** (as in DinoTale and btgarecomp round 134):
`tools/package-linux.sh` builds a `.tar.gz` with the binary, a launcher
script, a `.desktop` entry and icon, assets and license notices;
`CMakePresets.json` has Linux presets. The game now finds `assets/` next to
its executable when the working directory doesn't have it, which is the
case when Steam launches it as a non-Steam game. Tested here by unpacking
the package and starting it from `/`. The binary needs glibc 2.38+, SDL2,
GTK 3, FreeType and Vulkan from the system. Not yet run on a Deck.

**Crash logs** from btgarecomp round 134: exception code and module, C++
exception type and message, and `crash_log.txt` for uncaught exceptions.

## 2026-10-11, round 1: project set up; symbols, config, audio, first build

**Starting point.** The repository was empty. Two references:
- btgarecomp, the sequel's port: polished, and its frontend, runtime glue,
  saves and build setup carry over directly. Its game-specific fixes don't,
  since every address differs, but the engine is the same, so each one has a
  counterpart to find here.
- bdragoncore/battle-tanx-recomp, an earlier BattleTanx recompilation that
  ran but was buggy. Not used as a base. Its symbol file has about 100
  overlapping function entries, and several libultra functions under the
  wrong names (`0x801124C0` `osInvalDCache` as `osWritebackDCache`,
  `0x80111CBC` `__osEnqueueAndYield` as `__osDispatchThread`, `0x8011BFD0`
  `__osSiRawStartDma` as `osSiRawStartDma`, `0x8011D090` `__osSpSetPc` as
  `osSpGetStatus`, among others). N64Recomp swaps libultra functions for the
  runtime's own by name, so those names put the wrong code in place, which
  goes some way to explain the bugs. Its notes on which loops hang were
  useful leads, and matched the sequel's.

**ROM layout** (`syms/rom_info.md`): ROM `0x1000` loads to `0x80071000`, code
to `0x80125550`, data to `0x80147040`, then BSS. No overlays. Library code
from `0x80110480`; the RSP microcode text sits just before it.

**Symbol file, generated** (`BattleTanxSyms/README.md`).
`tools/find_functions.py` finds functions by following control flow, so a
function's size is what it reaches, never the gap to the next symbol. That
rules out by construction the sequel's main crash class (functions merged
into the one before, failing at runtime with "Failed to find function").
1729 functions, 79 jump tables. Library names from n64sym, kept only in the
library region (below it, n64sym's hits are tiny-signature false positives
such as "osVoiceInit" on game code) and corrected where it named two
functions the same (`syms/library_names.txt`).

**N64Recomp config** (`battletanx.us.rev0.toml`): N64Recomp recompiles all
1729 functions without errors. Two stubs (the game's cache-invalidation
loops, `cache` instructions). Every division is guarded
(`tools/gen_div_guards.py`, 108 sites): the `div` is turned into a nop and a
hook divides only when the divisor isn't zero, otherwise giving the R4300's
results. (The sequel's guards, for comparison, sit in front of the original
`div` without removing it, so its unguarded division still runs.)

**Busy-wait loops**: yields added at the counterparts of the sequel's
hang sites, from the start rather than one hang at a time:

| Here | Sequel | What it waits for |
|---|---|---|
| `func_80079064` (2 loop heads) | `func_800A1384` | VI manager state settled |
| `func_8007A5A4` `0x8007A70C` | `func_80097844` | audio task slot `0x80125764` free (stale register) |
| `func_8007D850` `0x8007D890` | | graphics state field `0xD4` |
| `func_8007E248` (2 loops) | `func_8007A818` | a free framebuffer (stale register, then retry) |
| `func_800A8EE0` | `func_80079FF0` | `*0x802E17D0 == 0` |

**Audio**: the game uses libultra's standard `aspMain` (the sequel uses
`n_aspMain`). `aspMain.us.rev0.toml` recompiles it from the ROM (text
`0x8010F660`, 0xE20 bytes; data `0x80145AA0`, whose halfwords 8-23 are the
command dispatch table).

**Frontend and runtime glue**, from the sequel with names changed: window,
audio output with the latency cap, input, local multiplayer, Controller Pak
saves (`.mpk`), crash reports, theme, Windows packaging. Game-specific parts
redone for this ROM: the per-frame UI pump is a patch of `func_80079530`
(the sequel's `func_800A1858`, same structure), the SI access queue shim
uses this ROM's addresses, the ROM hash is `0x8e485e76e7448bf3`.
