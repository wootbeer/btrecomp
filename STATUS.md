# Status

The project's working log, newest first, as "rounds" (the same format as
the sequel's port, [btgarecomp](https://github.com/wootbeer/btgarecomp)'s
STATUS.md, which this project refers to often: the two games share an
engine). PROGRESS.md has the short version.

# Log

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
