// Widescreen support. For now this only holds the current Expand factor,
// which the frontend updates every frame (update_gfx in src/main/main.cpp);
// the cull-cone widening and HUD anchoring hooks will read it.
#include <atomic>

#include "btx_config.h"

namespace {
    std::atomic<float> widescreen_scale{1.0f};
}

void btx::set_widescreen_scale(float scale) {
    widescreen_scale.store(scale, std::memory_order_relaxed);
}

float btx::get_widescreen_scale() {
    return widescreen_scale.load(std::memory_order_relaxed);
}
