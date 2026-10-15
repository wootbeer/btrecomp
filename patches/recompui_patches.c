#include "patches.h"
#include "ui_funcs.h"

// func_8007F204 is an original, unpatched game function this patch calls by
// name; patches.toml's func_reference_syms_file lets N64Recomp resolve it as
// a call into the rest of the recompiled game.
extern void* func_8007F204(s32 flag);

// src/game/screen_edge_scissor_fix.cpp: widens the game's off-by-one
// screen-edge scissors in its viewport display lists.
extern void btx_fix_screen_edge_scissors(void);

// src/game/pacing_log.cpp: frame pacing statistics (BTX_PACING_LOG=1).
extern void btx_pacing_vi(u32 pending, u32 threshold, u32 frame_ready);

// func_80079530 is the game's VI swap routine: the VI dispatch thread runs it
// once per VI, and it is the only caller of osViSwapBuffer. That makes it the
// place to pump recompui's per-frame UI callbacks (recomp_run_ui_callbacks)
// and to apply the screen-edge scissor fix.
// A RECOMP_PATCH replaces the whole function, so the pump runs at a real call
// boundary; the rest is the original, line for line from the disassembly.
// It's the same routine as the sequel's func_800A1858, with the same fields
// in the state struct passed in:
//   0x1EC (u16): swap-due threshold, compared against 0x1F0.
//   0x1F0 (u16): pending-swap counter; cleared once a swap happens.
//   0x1F2 (u16): one-shot "unblank display" flag.
RECOMP_PATCH void func_80079530(void* state) {
    btx_fix_screen_edge_scissors();
    recomp_run_ui_callbacks();

    u16* swap_pending = (u16*) ((u8*) state + 0x1F0);
    u16* swap_threshold = (u16*) ((u8*) state + 0x1EC);
    u16* unblank_pending = (u16*) ((u8*) state + 0x1F2);

    if (*swap_pending < *swap_threshold) {
        btx_pacing_vi(*swap_pending, *swap_threshold, 0);
        return;
    }

    void* frame_buffer = func_8007F204(1);
    btx_pacing_vi(*swap_pending, *swap_threshold, frame_buffer != 0);
    if (frame_buffer == 0) {
        return;
    }

    if (*unblank_pending != 0) {
        osViBlack(0);
        *unblank_pending = 0;
    }

    osViSwapBuffer(frame_buffer);
    *swap_pending = 0;
}
