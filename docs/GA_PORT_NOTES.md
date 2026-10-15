# Fixes from this port that apply to BattleTanx: Global Assault

These are fixes found while porting BattleTanx (this repo) that should carry
over to the Global Assault port (btgarecomp). The two games share an engine,
and both ports pin the same RT64 commit (`43373749`). STATUS.md here has the
full investigation for each round.

Checked against btgarecomp:
- **`main`:** has RT64 patch 0001 only.
- **Android branch (`claude/modest-ramanujan-s0wyga`):** also has 0009, the
  linear-IDs patch that is 0002 here.

On a copy of RT64 with GA's 0001 applied, all five RT64 patches below apply
cleanly, both with and without GA Android's 0009. Every patch also passes
`git apply --reverse --check` with all of them applied, which is what
`ApplyLibPatches.cmake` relies on to skip a patch that is already applied.

## 1. RT64 patches: copy into `lib-patches/rt64/`

Rename them to continue GA's numbering. Keep their relative order:
`ApplyLibPatches` applies the files in sorted name order.

| This repo | What it fixes | Value for GA |
|---|---|---|
| `0007-camera-view-interpolation.patch` | RT64 blended the camera's view matrix entry by entry, while objects interpolate in world space. On interpolated frames, a chase camera turning around its tank threw the tank sideways (2.1 units at mid-frame for a 15° turn, about 25 px at 1080p), then it snapped back on the real frame. Now the view's rotation is slerped and the camera **position** is lerped. | **High.** GA round 130's report, "motion slightly less smooth ... mostly when rotating the camera", is this. Confirmed here: hard turns went from "extreme jitter" to good. |
| `0006-exact-projection-for-estimated-view.patch` | GA's 0001 makes real frames use the game's exact view-projection, but interpolated frames still use RT64's estimated view × projection. That estimate drifts a little each frame, moving world depth by about 10 units on interpolated frames only. Sprites and effects drawn right against geometry pass the depth test on real frames and fail in between, so they flash. Now the projection is recomputed as `inverse(view) × VP`, so view × projection matches the game's matrix exactly. | **High.** It's the other half of GA round 122 (0001 fixed only real frames). Fixed explosions flashing at Display rate here. |
| `0004-vi-rate-from-latest-factor.patch` | RT64 treats the game's frame rate as unknown (0) unless the last 3 VI counts are equal. With mixed 2/3/4-VI frames, interpolation turned off and on every few frames and its timing reset. Now the rate comes from the latest VI count. | **Medium.** GA runs a steady 2 VIs in gameplay (round 130 log), so mostly no effect there, but it helps whenever frames slip: busy scenes, loads, the intro. Harmless otherwise. |
| `0003-auto-match-max-distance.patch` | RT64's automatic matching has no distance limit. When objects with the same model enter or leave the draw range, one coming in got paired with one going out, hundreds of units away, and flew across the screen on interpolated frames. Pairs more than 256 units apart are now rejected. | **Medium.** GA objects without an ID go through this same matching. 256 units was above every legitimate per-frame move here; shells move about 110. |
| `0005-id-match-max-distance.patch` | The same 256-unit limit for pairing by ID, applied in `matchTransform`. An ID that gets reused for a new object elsewhere starts fresh instead of sliding across. | **Low to medium.** Only matters with IDs, i.e. the Android branch's interpolation IDs. |

The 256-unit limit is set in world units for this engine. If something
fast in GA ever stops interpolating, it's moving more than 256 units per
frame; raise `MaxPositionDifference` in 0003/0005.

## 2. Build: regenerate RecompiledFuncs when the config changes

Nothing in GA's CMake reruns N64Recomp when `battletanxga.us.rev0.toml` or
the symbol file changes. So `cmake --build` after a `git pull` keeps stale
hooks. This repo hit exactly that: two rounds of fixes never reached the
test build. Add after `target_sources(RecompiledFuncs ...)` (names adjusted
for GA):

```cmake
if (EXISTS "${CMAKE_SOURCE_DIR}/BattleTanx Global Assault (USA).z64" AND (FUNC_C_SOURCES OR FUNC_CXX_SOURCES)
        AND NOT FUNC_C_SOURCES MATCHES "recompiled_funcs_placeholder")
    add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/recompiled_funcs.stamp
        COMMAND $<TARGET_FILE:N64RecompCLI> battletanxga.us.rev0.toml
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/recompiled_funcs.stamp
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS ${CMAKE_SOURCE_DIR}/battletanxga.us.rev0.toml
                ${CMAKE_SOURCE_DIR}/BattleTanxGASyms/battletanxga.us.rev0.syms.toml
                N64RecompCLI
        BYPRODUCTS ${FUNC_C_SOURCES} ${FUNC_CXX_SOURCES}
        COMMENT "Regenerating RecompiledFuncs/ from battletanxga.us.rev0.toml"
        VERBATIM)
    add_custom_target(RecompiledFuncsGen DEPENDS ${CMAKE_BINARY_DIR}/recompiled_funcs.stamp)
    add_dependencies(RecompiledFuncs RecompiledFuncsGen)
endif()
```

Check the ROM file name and the variable names against GA's CMakeLists.
`BYPRODUCTS` is what makes Ninja recompile the regenerated files in the same
build. The first build after adding this regenerates everything once.

## 3. Interpolation IDs: lessons for GA's Android branch

These only matter if GA's Android-branch `interpolation_ids.cpp` reaches
`main`, or for GA situations like the ones below:
- **Shared matrices:**
  - Static objects whose matrix is loaded several times a frame (one load
    per model part) are safe to tag with the matrix address when the
    matrix is unchanged since last frame.
  - Moving ones drawn in a stable number of parts (debris) are safe too.
  - Only matrices whose load count changes need automatic matching.
- **Camera-facing sprites built in a per-frame pool:** this game's fire and
  smoke. No address is stable, so they are tracked in world space instead
  (nearest to last position + last velocity) and given IDs of their own.
- **Sprite rotation:** this engine randomises a sprite's roll every frame,
  so rotation interpolation is turned off for sprites.

## Already in GA (no action)

Frame-step snapping (0.375 per VI), the draw distance pin, the cutscene
letterbox, the cutscene projection guard and the audio latency cap all
started in GA and were ported here from it.

To find these fixes in this repo:
- RT64 patches: `lib-patches/rt64/`
- Matrix ID rules: `src/game/interpolation_ids.cpp`
- Build regeneration: the `RecompiledFuncsGen` block in `CMakeLists.txt`
