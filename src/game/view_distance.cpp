// Fog and Draw Distance on the Graphics tab (round 29, from the sequel's
// port, btgarecomp rounds 158-161). Both default to the game's own behaviour.
//
// Fog: every frame func_8007DC5C sets the fog with the game's only
// G_MOVEWORD G_MW_FOG, 0x64009D00 = gSPFogPosition(995, 1000), so geometry
// fades into the fog colour over the last 0.5% of the depth range, ending at
// the far plane. With the fog off the word becomes 0 (multiplier and offset
// 0): no vertex gets any fog.
//
// Draw distance: the int at 0x80125858 is how far things are drawn (the
// drawers compare squared distances with its square at 0x801C9174) and sets
// the far plane, and so where the fog sits. func_80081AD0 sets it at each
// level start from the level's maximum (func_80086AE8, cut to 16 bits), then
// squares it at 0x80082148; its copy at 0x801C9178 comes from the same
// register. Once a second func_800862E8 adds 50 while it's under the level's
// maximum and leaves a larger value alone (the step down, func_800862B4, is
// stubbed: round 12). Far triples the level's value, Very Far multiplies it
// by 6, and Map Edge uses kMaxDrawDistance.
//
// The ceiling: the square is a 32-bit int (overflows past 46,340), and the
// level's own value is a 16-bit one, so nothing goes past 32,000.
#include <atomic>
#include <cstdint>

#include "recomp.h"

#include "btx_config.h"

namespace {
    std::atomic<bool> fog_enabled{ true };
    std::atomic<int> distance_level{ 0 };

    constexpr int32_t kMaxDrawDistance = 32000;
}

void btx::set_fog_enabled(bool enabled) {
    fog_enabled.store(enabled);
}

void btx::set_draw_distance(int level) {
    distance_level.store(level);
}

// func_8007DC5C before 0x8007E0BC: $v1 is the fog word about to be stored.
extern "C" void btx_fog_word(uint8_t* rdram, recomp_context* ctx) {
    if (!fog_enabled.load()) {
        ctx->r3 = 0; // gSPFogFactor(0, 0)
    }
}

// func_80081AD0 before 0x80082148: $v0 is the level's draw distance, about to
// be squared and stored.
extern "C" void btx_level_draw_distance(uint8_t* rdram, recomp_context* ctx) {
    int32_t distance = (int32_t)ctx->r2;
    switch (distance_level.load()) {
        case 1: distance *= 3; break; // Far
        case 2: distance *= 6; break; // Very Far
        case 3: distance = kMaxDrawDistance; break; // Map Edge
        default: return;
    }
    if (distance > kMaxDrawDistance) {
        distance = kMaxDrawDistance;
    }
    ctx->r2 = (gpr)distance;
}

// Room for the farther draw distances. At Map Edge the first campaign level
// queued more than the 0x800 (2048) draw items func_80080728 allows a frame
// (16 bytes each at 0x801B6C20, count 0x801B6C10), and the rest were dropped,
// so distant things blinked (as in the sequel's port, round 161). The other
// limits had room: 87 of 300 mesh entries, 0xEC0 of the 0xC000-byte matrix
// pool. When the draw distance is above Original, the items go to unused
// memory at kNewItemPool with room for kNewItemLimit. The choice is latched
// once a frame when func_800805DC empties the queue, so one frame never mixes
// the two pools. The queue's readers follow the items by pointer.
namespace {
    constexpr uint32_t kNewItemPool = 0x80740000; // 0x2000 x 16 = 0x20000 bytes
    constexpr int32_t kOldItemLimit = 0x800;
    constexpr int32_t kNewItemLimit = 0x2000;

    bool extended_frame = false;
}

// func_800805DC's entry: the frame's draw queue is emptied.
extern "C" void btx_draw_queue_frame(uint8_t* rdram, recomp_context* ctx) {
    extended_frame = distance_level.load() > 0;
}

// func_80080728 before 0x80080890 `beq $a1, $v0`: $v0 is the item limit,
// compared with the count in $a1.
extern "C" void btx_draw_item_limit(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = extended_frame ? kNewItemLimit : kOldItemLimit;
}

// func_80080728 before 0x800808BC `addu $v1, $v1, $a0`: $a0 is the item pool,
// $v1 the new item's offset in it.
extern "C" void btx_draw_item_pool(uint8_t* rdram, recomp_context* ctx) {
    if (extended_frame) {
        ctx->r4 = (gpr)(int32_t)kNewItemPool;
    }
}
