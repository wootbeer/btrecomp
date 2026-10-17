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
// The frame step is also snapped to whole VIs, as in the sequel's port
// (btgarecomp src/game/frame_dt_fix.cpp). On hardware the game reaches that
// point at the same phase of every video frame, so the value is steady. Here
// osGetTime() is the host clock and the game thread wakes at slightly
// different moments, so it wobbles a little from frame to frame, and objects
// move unevenly while frames are shown on time. At the original framerate
// that's hard to see; at the display rate the in-between frames spread each
// uneven step out, and driving stutters on and off.
#include <cmath>

#include "recomp.h"

namespace {
    constexpr gpr kOsClockRate = (gpr)(int32_t)0x80145DA0; // u64
    constexpr float kViUnits = 0.5f;
}

// After the game's osInitialize call (func_800779CC, 0x800779E4).
extern "C" void btx_init_clock_rate(uint8_t* rdram) {
    uint64_t rate = (uint64_t(uint32_t(MEM_W(0, kOsClockRate))) << 32) | uint32_t(MEM_W(4, kOsClockRate));
    rate = rate * 3 / 4;
    MEM_W(0, kOsClockRate) = int32_t(rate >> 32);
    MEM_W(4, kOsClockRate) = int32_t(rate);
}

// Before `swc1 $f0, 0x57B0($at)` (0x8007B0AC).
extern "C" void btx_frame_dt(uint8_t*, recomp_context* ctx) {
    const float raw = ctx->f0.fl;
    const float snapped = std::round(raw / kViUnits) * kViUnits;
    if (snapped >= kViUnits) { // the first frame's tiny or zero value stays as measured
        ctx->f0.fl = snapped;
    }
}
