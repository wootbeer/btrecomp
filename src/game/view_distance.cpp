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
#include <cstring>

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

// Detail levels. func_800824E4 picks an object's model by its squared distance
// from the camera, with the constants at 0x800714B8 (90,000: 300 units, for
// two-model objects) and 0x800714BC / 0x800714C0 (2,890,000 / 1,000,000: 1700
// and 1000 units, for three-model objects such as the tanks). The farthest
// models are untextured and meant to sit in the fog, so with a farther
// distance they showed plainly (round 31). The thresholds move out with the
// draw distance: scaled by the square of its ratio at each level start, and
// set back to the game's values at Original. Nothing else reads them.
namespace {
    constexpr uint32_t kLodThresholds[] = { 0x800714B8, 0x800714BC, 0x800714C0 };
    constexpr float kLodOriginal[] = { 90000.0f, 2890000.0f, 1000000.0f };

    void set_lod_scale(uint8_t* rdram, float ratio) {
        for (size_t i = 0; i < 3; i++) {
            float value = kLodOriginal[i] * ratio * ratio;
            uint32_t bits;
            memcpy(&bits, &value, sizeof(bits));
            MEM_W(0, (gpr)(int32_t)kLodThresholds[i]) = (int32_t)bits;
        }
    }
}

// func_80081AD0 before 0x80082148: $v0 is the level's draw distance, about to
// be squared and stored.
extern "C" void btx_level_draw_distance(uint8_t* rdram, recomp_context* ctx) {
    const int32_t original = (int32_t)ctx->r2;
    int32_t distance = original;
    switch (distance_level.load()) {
        case 1: distance *= 3; break; // Far
        case 2: distance *= 6; break; // Very Far
        case 3: distance = kMaxDrawDistance; break; // Map Edge
        default: break;
    }
    if (distance > kMaxDrawDistance) {
        distance = kMaxDrawDistance;
    }
    if (distance < original) {
        distance = original;
    }
    set_lod_scale(rdram, original > 0 ? (float)distance / (float)original : 1.0f);
    ctx->r2 = (gpr)distance;
}

// func_80081AD0 before 0x80082168, after it stored the distance's copy at
// 0x801C9178. func_80083394 only rebuilds the projections (func_800A5EB0,
// guPerspectiveF with the distance as the far plane) when the distance and
// that copy differ, which happens when func_800862E8 raises it. The level
// start stores both, so a farther setting moved the culling but kept the
// last far plane, and the fog with it (round 30). Clearing the copy makes
// the next frame rebuild the projections with the new distance.
extern "C" void btx_level_draw_distance_stored(uint8_t* rdram, recomp_context* ctx) {
    if (distance_level.load() > 0) {
        MEM_W(0, (gpr)(int32_t)0x801C9178) = 0;
    }
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
