# Status

The project's working log, newest first, as "rounds" (the same format as
the sequel's port, [btgarecomp](https://github.com/wootbeer/btgarecomp)'s
STATUS.md, which this project refers to often: the two games share an
engine). PROGRESS.md has the short version.

# Log

## 2026-10-12, round 5: shared matrices off the IDs (cannon particles)

**Report.** Round 4 confirmed on Windows: the flashing is gone at Display
rate, and the widescreen HUD looks right. New: the particles from the
tank's cannon fly all over the place.

**Cause.** Measured in the sandbox while firing: of the ~210 ready Mtx
loaded per frame, 31-35 are loaded more than once in the frame (one by 15
draws), each under a different parent transform: effect and particle
matrices shared by many draws. Each had one ID, and RT64 pairs an ID's
repeats in order, so whenever the number or order of particles changed,
each was interpolated toward a different particle from the last frame.

**Fix** (`src/game/interpolation_ids.cpp`): a ready Mtx loaded more than
once in this frame or the last is treated as shared and left to RT64's
automatic matching, as shared float matrices already were. Only Mtx used
once a frame keep their address as ID.

## 2026-10-12, round 4: widescreen HUD; extended GBI really enabled; Vulkan clipping

**Widescreen HUD** (`src/game/hud_anchor.cpp`). This game has no HUD script
interpreter like the sequel's; fixed code draws it. `func_800F5B6C` picks one
function by player count (stored at `0x8037A140`): `func_800F6934` for one
player, `func_800F8120`/`func_800F9F34`/`func_800FC674` for 2-4. Found by
tracing the 2D primitives' call sites in a temporary build (texture rect
loader `func_800F3D40`, fill rect `func_800F4DFC`). They draw into the HUD's
own list: three rotating 0x8000-byte buffers at `0x80362120` (index
`0x801416F4`), head `0x8037A174`. A hook where the HUD function returns
(`0x800F6664`, before the end-of-list command) rewrites the frame's HUD
once: rectangles are grouped with their neighbours on the same row
(strings, numbers and their icons move together), groups entirely left of
x 140 are anchored left, right of 180 right, others stay centred, and
anchored groups move 16 px further out (the TV-safe margin), with the
scissor widened while anchored. The buffer holds only plain RDP commands
(dumped and checked), so rewriting it in place is safe. Only with 1-2
players, in Expand. Checked in the sandbox: tank icon and lives top left,
ammo top right, radar bottom left at the window edges, health bar centred.

**gEXEnable was wrong for this game's microcode.** RT64's hook is
`G_SPNOOP`, opcode 0x00 in F3DEX 1.21 (this game) and 0xE0 in F3DEX2 (the
sequel). Round 3 copied the sequel's `0xE0525464`, so RT64 never enabled
its extended GBI and ran each extended command's second word as an ordinary
command: round 3's matrix groups did nothing (luckily their parameter words
decode as harmless commands), and the first HUD anchoring crashed RT64 (a
left offset word `0xFFC00000` ran as `G_SETCIMG` to `0x00C00000`, width 1).
Both now use `0x00525464`, so round 3's interpolation IDs are active from
this round on.

**Vulkan clipping** (from the sequel's Android branch,
`lib-patches/plume/`): Vulkan requires scissors and clear rects inside the
render target, where Direct3D 12 just clips them. The window-wide scissor
for anchored HUD elements starts left of the target, so on Vulkan (the
sandbox here, Linux, the Steam Deck) everything drawn with it was dropped.
The patches clip them the D3D12 way. `cmake/ApplyLibPatches.cmake` now
takes a folder for nested submodules (plume is inside RT64).

## 2026-10-12, round 3: interpolation IDs (jitter and flashing at Display rate)

**Report.** Plays fine (Windows build). With Refresh Rate at Display there's
jitter and flashing textures everywhere; fine at Original.

**Cause** (as in the sequel's Android port, btgarecomp
`src/game/interpolation_ids.cpp`): with no IDs, RT64 matches each model with
one from the last frame by comparing positions, and gets it wrong often
enough to show; and its default AUTO position component stops
interpolating whenever an object's speed jumps, flipping between smoothed
and snapped frames.

**This game's matrices.** No draw queue here (the sequel added one).
Models load matrices through a block of small functions (`0x8007E41C`
to `0x8007EBDC`): float matrix to a pooled Mtx (some cache it at float
matrix + 0x40 for the frame), the per-view camera matrix (the pool's first
Mtx), or a ready Mtx, each writing gSPMatrix straight into one of three
display lists. The graphics state is `*0x801257D0`: list heads at
`+0xD8` (main), `+0xDC`, `+0xE0`, Mtx pool pointer at `+0xE8`, frame index at
`+0xD0`, and per-frame slots at `+0x90 + 0x18 * index` with the list buffers
(0x8000, 0x6000 of which the game uses 0x5C00, 0x400) and the pool
(0xC000). `func_8007D850` resets them each frame; `func_8007D998` builds the
master list: the frame clear (`func_8007DC5C`), then the three lists.

**Fix** (`src/game/interpolation_ids.cpp`): a hook in each emitter writes a
gEXMatrixGroup ahead of its gSPMatrix: the float matrix's address as the
ID (automatic if that float matrix made more than one Mtx this frame or
last, i.e. shared scratch), a ready Mtx's own address (automatic if from
the pool), and automatic for the camera matrix. Components are
gEXMatrixGroupDecomposedNormal's (always interpolate), also set as the
frame's base group at the end of the frame clear. Groups replace the top
entry instead of pushing. Each group is 16 bytes in a list the game sized
for itself, so tagging stops well short of a list's end (ending with an
automatic group); the game's own draws keep their room. RT64 patch from the
sequel (`lib-patches/rt64/0002`): an ID with more transforms than last
frame pairs the extra ones with its last transform from last frame.

Runs in the sandbox through the menus into mission 1; the jitter itself
needs checking on real hardware. (Correction, round 4: the gEXEnable used
here is F3DEX2's, so these groups only took effect from round 4.)

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
