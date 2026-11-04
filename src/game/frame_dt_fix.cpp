// Game speed and frame step.
//
// func_8007AD2C, the per-frame timer, measures the time since the previous
// frame with osGetTime() and stores it as a float at 0x801257B0; motion
// multiplies by it. It divides by osClockRate (0x80145DA0), which the game's
// own osInitialize sets to 3/4 of its initial 62.5 MHz, the CPU count rate,
// so one VI is 0.5. The runtime replaces osInitialize and skips that step;
// with the value left at 62.5 MHz a VI came out as 0.375 and the whole game
// ran at 3/4 speed. btx_init_clock_rate does the step.
//
// The frame step is the VIs the frames took. On hardware the game reaches
// the measuring point at the same phase of every video frame, so the value
// is exactly that. Here osGetTime() is the host clock and the game thread
// wakes at slightly different moments, so the measurement wobbles (about
// +/-25% of a 2-VI frame): rounded to whole VIs it sometimes came out a VI
// long or short on frames that were really all 2 VIs (owner's pacing log),
// and motion, turning most of all, jumped on those frames at the display
// rate. So the step is counted instead: the VIs of the frames swapped since
// the last measurement (btx_frame_swapped, from the game's VI swap routine,
// src/game/pacing_log.cpp), each swap counted once. With no swap since (the
// first frame, a frame that wasn't shown) the measurement is kept, rounded
// to whole VIs.
#include <atomic>
#include <cmath>
#include <cstdint>

#include "recomp.h"

namespace {
    constexpr gpr kOsClockRate = (gpr)(int32_t)0x80145DA0; // u64
    constexpr float kViUnits = 0.5f;
    // A frame step longer than this is a pause or a load, left as measured.
    constexpr uint64_t kMaxCountedVis = 16;

    std::atomic<uint64_t> swapped_vis{ 0 }; // VI thread: VIs of all frames swapped
    uint64_t counted_vis = 0;               // game thread: swapped_vis at the last step
}

// After the game's osInitialize call (func_800779CC, 0x800779E4).
extern "C" void btx_init_clock_rate(uint8_t* rdram) {
    uint64_t rate = (uint64_t(uint32_t(MEM_W(0, kOsClockRate))) << 32) | uint32_t(MEM_W(4, kOsClockRate));
    rate = rate * 3 / 4;
    MEM_W(0, kOsClockRate) = int32_t(rate >> 32);
    MEM_W(4, kOsClockRate) = int32_t(rate);
}

// The game's VI swap routine has shown a frame that was up for `vis` VIs.
void btx_frame_swapped(uint32_t vis) {
    swapped_vis.fetch_add(vis, std::memory_order_relaxed);
}

// Before `swc1 $f0, 0x57B0($at)` (0x8007B0AC).
extern "C" void btx_frame_dt(uint8_t*, recomp_context* ctx) {
    const uint64_t now = swapped_vis.load(std::memory_order_relaxed);
    const uint64_t vis = now - counted_vis;
    counted_vis = now;
    if (vis >= 1 && vis <= kMaxCountedVis) {
        ctx->f0.fl = (float)vis * kViUnits;
        return;
    }
    const float raw = ctx->f0.fl;
    const float snapped = std::round(raw / kViUnits) * kViUnits;
    if (snapped >= kViUnits) { // the first frame's tiny or zero value stays as measured
        ctx->f0.fl = snapped;
    }
}
