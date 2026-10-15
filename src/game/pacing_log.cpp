// Frame pacing statistics, for stutter reports (BTX_PACING_LOG=1).
//
// The game shows a new frame only when its VI routine (func_80079530,
// patched in patches/recompui_patches.c) finds both that enough VIs have
// passed since the last swap (the threshold, 2 for 30 fps) and that the next
// frame is finished. Called from there once per VI. Every 300 VIs (about 5
// seconds) a line goes to pacing_log.txt in the working directory:
//   - how many VIs each shown frame took (1, 2, 3, 4+);
//   - "late": VIs where a swap was due but the frame wasn't finished, i.e.
//     the game (or the renderer it waits on) ran over its frame time.
// Frames of uneven length are what interpolation can't smooth: RT64 learns
// a frame's length only afterwards.
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "recomp.h"

namespace {
    bool enabled() {
        static const bool on = [] {
            const char* value = std::getenv("BTX_PACING_LOG");
            return value != nullptr && value[0] != '\0' && value[0] != '0';
        }();
        return on;
    }

    struct Stats {
        uint32_t vis = 0;
        uint32_t frames[5] = {}; // 1, 2, 3, 4+ VIs (index 0 unused)
        uint32_t late = 0;
        uint32_t threshold = 0;
    };
    Stats stats;
    uint32_t windows = 0;
}

extern "C" void btx_pacing_vi(uint8_t*, recomp_context* ctx) {
    if (!enabled()) {
        return;
    }
    const uint32_t pending = (uint32_t)ctx->r4;
    const uint32_t threshold = (uint32_t)ctx->r5;
    const bool frame_ready = ctx->r6 != 0;
    stats.vis++;
    stats.threshold = threshold;
    if (pending >= threshold) {
        if (frame_ready) {
            // This VI swaps: the frame was shown for `pending` VIs.
            stats.frames[pending >= 4 ? 4 : (pending < 1 ? 1 : pending)]++;
        }
        else {
            stats.late++;
        }
    }
    if (stats.vis >= 300) {
        if (FILE* file = std::fopen("pacing_log.txt", windows == 0 ? "w" : "a")) {
            std::fprintf(file, "[%u] VIs per frame: 1:%u 2:%u 3:%u 4+:%u  late VIs: %u  threshold: %u\n",
                windows, stats.frames[1], stats.frames[2], stats.frames[3], stats.frames[4], stats.late, stats.threshold);
            std::fclose(file);
        }
        windows++;
        stats = Stats{};
    }
}
