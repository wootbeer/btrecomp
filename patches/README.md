# patches/

C code compiled for the N64 (MIPS) and recompiled alongside the game, for
changes that replace a whole game function. A `RECOMP_PATCH` function here
takes the place of the original function of the same name.

- `recompui_patches.c`: replaces `func_80079530`, the game's per-VI swap
  routine, with the same logic plus the frontend's per-frame UI pump.

The build is automatic: `make` here produces `patches.elf` (through WSL on
Windows, since the Windows clang builds have no MIPS backend), then N64Recomp
recompiles it using `patches.toml` in the repo root. `syms.ld` gives native
runtime functions dummy addresses so the patches can call them.

Smaller fixes don't live here: instruction patches and hooks that run native
code at a point in a game function are in `battletanx.us.rev0.toml`, with
the native code in `src/game/` and `src/main/`.
