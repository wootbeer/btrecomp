// Room for the frame's main display list (issue #1: 4 players froze the game,
// with memory use in the gigabytes).
//
// func_8007D4E8 sets up the graphics state's two frame slots (+0x90 +
// 0x18 * slot): the main display list goes at +0x4, 0x8000 bytes from
// 0x8030AFF0 + slot * 0x8000, and the second slot's buffer ends right where
// the frames' start lists begin (+0x0, from 0x8031AFF0). The game never checks
// the main list's length. With the interpolation tags (interpolation_ids.cpp,
// 16 bytes per tagged object) a 4-player match's main list reached 0x81B0
// bytes (0x5660 without them): it ran into the start lists, the two frames'
// lists ended up calling each other, and RT64 followed them forever, through
// stale commands that also loaded bogus textures (a 0 x 0 one here; on the
// owner's player's PC, gigabytes).
//
// So the two main lists move to 128 KB buffers in the upper half of RDRAM,
// which the game never uses (checked in round 22; the radar arrows' lists are
// at 0x807E0000). The game reads each slot's buffer from the slot every
// frame, so nothing else needs to know.
#include <cstdint>

#include "recomp.h"

namespace {
    constexpr uint32_t kGfxStatePtr = 0x801257D0;
    constexpr uint32_t kMainListField = 0x90 + 0x4;
    constexpr uint32_t kSlotSize = 0x18;
    constexpr uint32_t kNewMainLists = 0x80700000;

    gpr kseg0(uint32_t address) {
        return (gpr)(int32_t)address;
    }
}

// The size of each relocated main list, for interpolation_ids.cpp's tag limit.
extern const uint32_t btx_main_list_size = 0x20000;

// func_8007D4E8, after its slot loop (0x8007D770).
extern "C" void btx_relocate_main_lists(uint8_t* rdram) {
    const gpr state = kseg0((uint32_t)MEM_W(0, kseg0(kGfxStatePtr)));
    for (uint32_t slot = 0; slot < 2; slot++) {
        MEM_W(kMainListField + kSlotSize * slot, state) = (int32_t)(kNewMainLists + slot * btx_main_list_size);
    }
}
