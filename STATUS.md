# Status

The project's working log, newest first, as "rounds" (the same format as
the sequel's port, [btgarecomp](https://github.com/wootbeer/btgarecomp)'s
STATUS.md, which this project refers to often: the two games share an
engine). PROGRESS.md has the short version.

# Log

## 2026-10-15, round 15: letterboxed 3D cutscenes black at the sides

**Report** (user screenshot, Expand): beside the 4:3 frame of a 3D
cutscene ("Civilization came to an end on May 6, 2001"), the scene's
clear colour shows instead of black; the 2D story slides are fine. The
sequel's rounds 101/105 problem.

**Cause.** A 3D cutscene draws through its camera's cutscene viewport:
entry `0x1E0` of the static viewport block (segment 1), a viewport centred
at (160,77) with the scissor at (34,19)-(286,134). The rest of the 4:3
frame stays black, but the frame clear (`func_8007DC5C`) fills the colour
buffer across the whole screen, and RT64 stretches a full-width fill to
the window in Expand.

**Fix** (`src/game/cutscene_letterbox.cpp`, called from the frame-clear
hook): when a camera's viewport list (+0xF4; cameras 0x348 bytes from
`0x802DE620`, count at `0x801B4AB4`, as `func_800A5EB0` reads them) is the
cutscene one, append a black fill of the whole screen (stretched to the
window like the clear), then the clear colour again over just the box
(34,19)-(285,133), which RT64 keeps in the 4:3 frame. The clear colour is
read from the clear's own fill colour command; if the clear didn't fill
the colour buffer this frame, only the black is drawn. Checked by forcing
the gameplay camera onto the cutscene viewport in a temporary build:
black beside and around the box, the scene and its sky inside.

## 2026-10-15, round 14: the camera interpolated as a camera (jitter turning)

**Report** (first build with rounds 8-13 really in it): still jitter
driving forward at the Display framerate, and extreme jitter when turning
hard.

**Cause.** RT64 interpolates the view matrix it splits from the game's
camera x perspective matrix component by component (the projection's
rigid body has decomposition off). The objects are interpolated
separately in world space. For a chase camera swinging round its tank the
two don't agree: the tank, steady in front of the camera on every real
frame, slid sideways in the in-between frames. Driving straight cancels
out to first order; turning doesn't.

**Fix** (`lib-patches/rt64/0007-camera-view-interpolation.patch`,
`ProjectionProcessor::processScene`): when the view is interpolated, its
orientation is slerped and the camera's position (minus the translation
times the inverse 3x3, not the matrix's translation) is lerped, then the
view is rebuilt from both. Falls back to the old blend for invalid or
mirrored decompositions. Checked with a standalone test (chase camera 100
units behind, tank turning 15 degrees and moving in a frame): the tank's
view-space position at mid-frame was off by 2.1 units sideways (about 1
degree, ~25 px at 1080p), now 0.0 (0.8 units in depth from moving along
the chord). Sandbox renders turning and driving normally.

## 2026-10-15, round 13: the build regenerates RecompiledFuncs/ itself

**Found while porting:** nothing reran N64Recomp after a change to
`battletanx.us.rev0.toml`, and the rebuild steps given to the user since
round 8 didn't either. So the hooks added in rounds 8 and 11 (effect
sprite tracking at `0x80080E48`, the frame step at `0x8007B0AC`) and round
12's stub never reached the user's build: the round 8 explosion fix was
never tested. Now `CMakeLists.txt` has a custom command that runs
N64RecompCLI when the config or the symbol file changes (stamp file in the
build directory), with the generated sources as byproducts so the same
build recompiles them; `RecompiledFuncs` depends on it. Checked: a touched
config regenerates and recompiles; an unchanged build does nothing. Only
once `RecompiledFuncs/` exists and the ROM is present (a fresh clone still
generates by hand first).

**The sequel's fixes, audited.** Ported or already present: busy-wait
yields, screen-edge scissors, widescreen culling, HUD anchoring, the
frame-step snapping (round 11), the draw distance pin (round 12), the
audio latency cap (in `main.cpp` since the start), the crash handler.
Checked and not needed: the cutscene camera's far plane (round 12 note);
`(int)step` stalls (the sequel's rounds 127/131: all ten truncations here
multiply the step by a whole factor first); functions merged into another
symbol and called through a pointer (the sequel's round 129 scan, adapted:
92 candidates, none referenced as a pointer). Left: the cutscene sides in
Expand (waiting on a screenshot).

## 2026-10-15, round 12: draw distance pinned at the level's maximum

The sequel's round 110 again. The draw distance at `0x80125858` (also the
far plane: `func_80083394` rebuilds the projection with it, and keeps its
square at `0x801C9174` for culling) is adjusted once a second by
`func_8007AD2C`: with the frame count above 20 (26 in some modes) it grows
by 50 towards the level's maximum (`func_80086AE8`, the table at
`0x80125A30`), otherwise `func_800862B4` takes 100 off, down to 1800. Any
slow second pulled distant scenery in and it popped back later, and the
far plane (with the fog) changed as it went: the sandbox's varying
projection depth terms in round 10. Each level starts at its maximum
(`func_80081AD0`), so `func_800862B4` is stubbed and the distance stays
there. Sandbox (below 20 fps, the worst case): 2000 throughout.

Also checked, the sequel's round 108 (cutscene camera with far plane 0):
not needed here. Both of this game's perspective setups have valid planes
(near 16; far 5000 or the level's distance), and the draw distance is
2000 from the first frame.

## 2026-10-15, round 11: frame step snapped to VIs; flying debris and fireballs keep their IDs

**Reports.** 144 Hz monitor; the driving stutter is visible in game, with
or without MSAA, only at the Display framerate. Explosions still flicker,
"only the explosions that fly away from the object". Cutscenes show extra
things beside them in Expand, as the sequel's did (screenshot requested:
the attract-mode intro and the story slides look right in the sandbox).

**Stutter: the sequel's rounds 88-89 again.** `func_8007AD2C`, the
per-frame timer, measures the time since the last frame with `osGetTime()`
and stores it as a float at `0x801257B0` (raw x 30 / 1,000,000, with the
same clock-rate scaling as the sequel, so one VI is 0.375). The host clock
makes it wobble (measured: 0.7495-0.7599, 1.1245-1.1434), so objects move
unevenly while frames are shown on time; the display rate spreads each
uneven step over its in-between frames. Fix (`src/game/frame_dt_fix.cpp`,
hook before `0x8007B0AC`): rounded to whole VIs, as in the sequel. A first
try at 0.5 per VI would have run everything a third fast; the measured
values (0.75, 1.125, 1.5) settled it.

**Flying debris.** Matrices kept in objects, loaded several times a frame
and moving (debris, burning wreckage) were on automatic matching since
round 5; during the player's explosion up to 95 a frame, and RT64 swapped
identical pieces. They now keep their address as ID when the object drew
the same number of parts in the two previous frames (anything whose count
changes stays automatic, as round 5 had it).

**Fireballs.** Sprites are now paired with the one from the last frame
closest to where it was heading (last position plus last movement), so
fast ones thrown off an explosion stay paired with themselves; a sprite
seen once gets twice the room. Their rotation is no longer interpolated:
the game gives each a new random spin every frame, and interpolating it
turned them through the angles in between. Sandbox (long idle run, the
player killed): ID-tagged transforms that jump or flip in a frame went
from 20 to 1.

## 2026-10-15, round 10: exact projection on interpolated frames (explosions flashing)

**Reports.**
- The pacing log from the user's PC: after loading, every frame took
  exactly 2 VIs and none was late. The game's pacing is steady, so the
  driving stutter is between RT64 and the display. Asked for the monitor
  refresh rate and GPU, and for tests with MSAA off and with a Manual
  framerate.
- Explosions flash only at the Display framerate, never at Original.

**Cause.** The world is drawn with one combined camera x perspective
matrix. On real frames RT64 renders it as is (lib patch 0001); on
interpolated frames it rebuilds it from its own split into view and
projection (`matrixDecomposeViewProj`), and that projection varies from
frame to frame with the fixed-point input (measured in the sandbox: depth
terms -1.01612 to -1.01793 and -32.22 to -32.28). At a typical distance
that moves the world's depth by around 10 units on interpolated frames
only. Fire and smoke sprites are drawn with the game's exact camera-less
projection, often right against the ground or a wreck, so they passed the
depth test on real frames and failed in between: flashing, worst for the
player's own explosion in front of the camera. Ruled out in the sandbox
along the way: scene pairing (the sprites' scene pairs only with itself)
and sprite pairing (round 8 holds); far fewer mis-paired debris shards
than needed to explain it.

**Fix** (`lib-patches/rt64/0006-exact-projection-for-estimated-view.patch`):
after the split, the projection's first three columns are recomputed as
inverse(view) x the game's matrix, so view x projection is exactly the
game's matrix (the w column already was). Interpolated frames blend
between exact projections, consistent with real frames and the sprites.
Sandbox: view x projection now matches to float precision (0.0005 on
values around 3000), projections still classify as perspective, and the
picture is unchanged.

## 2026-10-15, round 9: frame pacing log (intermittent stutter driving)

**Report.** Driving is mostly smooth now, but stutter comes and goes; the
user also still sees explosions flash.

**Video** (driving, 60 fps): the road moves a steady 3-4 px per display
frame, but every 10-15 frames one frame repeats and a neighbouring step is
doubled. That's pacing: now and then a game frame takes 3 VIs instead of
2, and RT64 learns a frame's length only once it's over, so it runs out of
in-between frames. The swap routine (`func_80079530`) swaps once the
threshold (2 VIs) has passed and the next frame is finished, so a 3-VI
frame means the frame was finished late. Not the yields (they don't
sleep), nor the VI timer (same runtime as the sequel, steady 30 fps).

**Added** `src/game/pacing_log.cpp`, called from the patched swap routine
every VI (`btx_pacing_vi`, `patches/syms.ld` `0x8F0000F8`). With
`BTX_PACING_LOG=1` it writes `pacing_log.txt` every 300 VIs: VIs per shown
frame (1/2/3/4+) and "late" VIs, where a swap was due but the frame wasn't
finished. Needs a log from the user's PC.

**Explosions.** In the sandbox the fire sprites pair with themselves
(round 8) and their camera-less projection scene pairs with itself, never
with the world's. Asked the user whether explosions also flash at the
Original framerate (the game's own effect) and for a clip.

## 2026-10-14, round 8: effect sprites tracked in world space (flicker)

**Report.** Round 7 is much smoother. Some particle effects still flicker
(user video: the player's tank exploding, fire and smoke under the death
camera).

**Video.** Fire sprites show in some display frames of a game frame and
not in others (a 4-frame cycle at 15 fps): in the interpolated frames a
sprite slides in from another sprite's place.

**Cause** (sandbox, temporary logs). Fire and smoke are camera-facing
sprites drawn by `func_80080C60` (call at `0x80080E48`). Their matrices
are in camera space and go into the per-frame pool, so they were on
RT64's automatic matching, all with the same model. When the camera moves
(the death camera orbits), their camera-space positions jump and RT64
paired them with each other. The function draws them from a list it
rebuilds every frame (nodes at `0x801C0xxx` hold a different sprite each
frame), so no address identifies a sprite; but each node holds the
sprite's world position (floats at +0x4/+0x8/+0xC), which doesn't depend
on the camera.

**Fix:**
- `src/game/interpolation_ids.cpp` (hook at `0x80080E48`): each sprite
  is paired with the nearest unclaimed sprite from the last frame in
  world space, within 64 units, and keeps its ID (`0x1xxxxxxx`, below
  the addresses used as IDs); new sprites get new IDs. The next ready Mtx
  load uses that ID instead of automatic matching.
- `lib-patches/rt64/0005-id-match-max-distance.patch`: ID pairing also
  stops at 256 units, like automatic matching since round 6.

**Result** (sandbox, firing): of 3208 sprite draws, every one with a
partner last frame was paired with itself (177 new sprites unpaired, as
they should be).

## 2026-10-14, round 7: interpolation switching off and on (own tank jitter)

**Report.** Round 6 helped, but the player's tank still jitters at
Display rate when driving (user video, 60 fps).

**Video.** The scenery moves every display frame (the camera is
interpolated), while the tank jumps about 9 px (at 1080p) and back every
two or three display frames.

**Measured** in the sandbox (temporary logs in RT64):
- The tank's matrices are paired correctly every frame, all at one
  orientation, with steps of 11, 16 or 22 units: frames of two, three or
  four VIs (about 5.5 units per VI).
- On real frames the tank is at exactly the same screen position (the
  camera is locked to it), so the game's own frames are steady.
- RT64 takes the game's frame rate from the last three VI counts between
  presents (`VIHistory::logicalRateFromFactors`), and returns 0, unknown,
  unless all three are equal. With this game's mixed 2/3/4-VI frames it
  was 0 for over half the frames: interpolation switched off for those,
  back on for the next, and its frame timing reset each time.

**Fix** (`lib-patches/rt64/0004-vi-rate-from-latest-factor.patch`): the
rate comes from the latest VI count alone (60/2 = 30, 60/3 = 20, ...).
That is how long the previous frame was shown, the time covered by the
frame being interpolated, so each game frame gets display frames in
proportion to its length. In the sandbox the rate is now never 0 after
startup.

## 2026-10-14, round 6: scenery flying across the screen while driving (Display rate)

**Report.** Round 5 confirmed: the cannon looks right. New: lots of
flickering and jitter when driving forward and back, only at Display rate.

**Measured** in the sandbox with a temporary log in RT64's frame matching
(`GameFrame::match`), with a manual 120 Hz refresh rate (under Xvfb RT64
sees no display rate, so Display never interpolates there):
- In gameplay the camera is part of the projection (one combined
  view x perspective Mtx, reloaded a few times a frame), and nearly every
  object is a ready Mtx loaded with `G_MTX_LOAD`: the level's scenery has
  one world-space Mtx per object (`0x801Dxxxx-0x801Fxxxx` here), drawn
  once per part of its model, so many are loaded several times a frame.
- Round 5 put every such Mtx on RT64's automatic matching, which pairs
  by model and nearest transform with no distance limit. Driving forward
  or back brings objects into the draw range and takes others out, and
  RT64 paired an object coming in with another of the same model going
  out: 210-1150 units apart, sometimes rotated 90 degrees. For the
  interpolated frames the object flew across the screen.
- Nothing in the ready path is short-lived but culled scenery, and no
  ready Mtx loaded more than once changes from frame to frame.

**Fix:**
- `src/game/interpolation_ids.cpp`: a ready Mtx loaded more than once
  keeps its address as ID when its contents are the same as last frame
  (hash of its 64 bytes): static scenery, all of whose loads are the same
  matrix. One that changes stays on automatic matching as in round 5, so
  whatever moved the cannon's particles is handled as before.
- `lib-patches/rt64/0003-auto-match-max-distance.patch`: automatic
  matching rejects a pair more than 256 units apart (shells, the fastest
  thing measured, move about 110 a frame). Left unpaired, an object is
  drawn where it is. This covers pooled Mtx too (destructible objects,
  tanks), which can't have IDs.

**Result** in the sandbox, driving forward and back, then firing: no
object with an ID moves more than 40 units or turns in a frame, and the
only large automatic moves are shells in flight and a few small
camera-space effects. Before: several scenery pairs per frame while
driving. Needs a check on real hardware at Display rate.

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
