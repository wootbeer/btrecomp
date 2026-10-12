// Closes the 1-pixel gap at the right and bottom screen edges, and with it
// lets RT64's Expand mode widen the 3D view (btgarecomp rounds 94, 97, 98).
//
// The game's viewport display lists (one per 1-4 player layout) set their
// scissor to x <= 319 / y <= 239 instead of 320 / 240. Scissor lower-right
// corners are exclusive, so the last column and row are never drawn: on a
// TV that was overscan, here it's a strip of clear colour or stale pixels.
// RT64 also only widens a projection in Expand when its scissor spans the
// whole screen width, so the 3D view stayed 4:3 until this is fixed. Only
// edges touching the outer screen border are moved; split-screen inner
// seams (x 159 / y 119) are left as designed.
//
// The lists are data: a 0x320-byte block at ROM 0xD7040, right after the
// code, which the graphics init (func_8007D4E8) copies to a buffer (0x803C8880, the end
// of BSS) and whose address it stores at 0x801257C0; the frame calls the
// lists through segment 1. It's the sequel's block, at the same offsets.
// The copy is fixed in memory, each word only while it still holds the
// ROM's value.
#include <cstdint>

#include "recomp.h"

namespace {
    constexpr uint32_t kBlockBufferPtr = 0x801257C0;
    constexpr uint32_t kBlockSize = 0x320;

    struct ScissorWordFix {
        uint32_t offset; // in the block
        uint32_t original;
        uint32_t fixed;
    };

    // w1 of each G_SETSCISSOR: (lrx << 12) | lry, in 10.2 fixed point.
    constexpr ScissorWordFix kFixes[] = {
        { 0x144, 0x004FC3BC, 0x005003C0 }, // full screen: 319,239 -> 320,240
        { 0x15C, 0x004FC1DC, 0x005001DC }, // 2P top:       right edge
        { 0x174, 0x004FC3BC, 0x005003C0 }, // 2P bottom:    right + bottom
        { 0x1A4, 0x004FC1DC, 0x005001DC }, // top-right:    right edge
        { 0x1BC, 0x0027C3BC, 0x0027C3C0 }, // bottom-left:  bottom edge
        { 0x1D4, 0x004FC3BC, 0x005003C0 }, // bottom-right: right + bottom
    };
}

extern "C" void btx_crash_note_rdram(uint8_t* rdram); // src/main/crash_handler.cpp

// Called once per VI from the func_80079530 patch (patches/recompui_patches.c).
extern "C" void btx_fix_screen_edge_scissors(uint8_t* rdram, recomp_context*) {
    btx_crash_note_rdram(rdram);
    uint32_t buffer = *(uint32_t*)(rdram + (kBlockBufferPtr - 0x80000000u));
    // Not loaded yet, or not a KSEG0 RDRAM address.
    if (buffer < 0x80000000u || buffer + kBlockSize > 0x80800000u) {
        return;
    }
    for (const auto& fix : kFixes) {
        uint32_t* word = (uint32_t*)(rdram + (buffer + fix.offset - 0x80000000u));
        if (*word == fix.original) {
            *word = fix.fixed;
        }
    }
}
