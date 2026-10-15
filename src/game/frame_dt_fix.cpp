// Frame step snapped to whole VIs, as in the sequel's port (btgarecomp
// src/game/frame_dt_fix.cpp, its STATUS.md rounds 88-89).
//
// func_8007AD2C, the per-frame timer, measures the time since the previous
// frame with osGetTime() and stores it as a float at 0x801257B0; motion
// multiplies by it. Like the sequel, it scales by the CPU count rate where
// the clock rate is meant, so one VI is 0.375, not 0.5 (measured: 0.75,
// 1.125 and 1.5 for 2-, 3- and 4-VI frames, give or take the wobble). On
// hardware the game reaches that point at the same phase of every video
// frame, so the value is steady. Here osGetTime() is the host clock and the
// game thread wakes at slightly different moments, so it wobbles a little
// from frame to frame, and objects move unevenly while frames are shown on
// time. At the original framerate that's hard to see; at the display rate
// the in-between frames spread each uneven step out, and driving stutters
// on and off. The value is rounded to whole VIs.
#include <cmath>

#include "recomp.h"

namespace {
    constexpr float kViUnits = 0.375f;
}

// Before `swc1 $f0, 0x57B0($at)` (0x8007B0AC).
extern "C" void btx_frame_dt(uint8_t*, recomp_context* ctx) {
    const float raw = ctx->f0.fl;
    const float snapped = std::round(raw / kViUnits) * kViUnits;
    if (snapped >= kViUnits) { // the first frame's tiny or zero value stays as measured
        ctx->f0.fl = snapped;
    }
}
