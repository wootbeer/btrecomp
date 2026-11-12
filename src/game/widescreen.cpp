// Widescreen (RT64's Expand aspect ratio).
//
// RT64 widens a 3D view by itself when its viewport and scissor span the
// whole screen width (that needs src/game/screen_edge_scissor_fix.cpp): it
// stretches the viewport to the window and narrows the projection to match,
// so more of the world shows, unstretched. The game still culls against its
// own 4:3 view cone, though, so ground, objects and effects near the new
// side edges would pop in and out (btgarecomp round 98).
//
// func_8008228C builds each player's cull record every frame (stride 0x2C
// from +0x584 of the struct in $a0) with func_80094060, passing a cull
// factor k on the stack (sp+0x14): the global at 0x8012585C (set per player
// count by func_80081AD0, from 2.2 / 1.12 / 1.12 / 2.29 at 0x80071490), or
// 1.12 (0x800714AC) for the full-width top view of the 3-player layout. k is
// about 1 / tan of half the horizontal field of view, and the cull tests
// |k * lateral| < forward, so dividing k by RT64's widening factor widens
// the cone to match.
#include <atomic>
#include <cstdint>

#include "recomp.h"

#include "btx_config.h"

// The game culls each frame against the view it is about to draw, with almost
// no room to spare (k 2.2 for one player, against about 2.24 for the screen
// edge). At a higher frame rate RT64 draws in-between frames, turning the
// camera part of the way between two game frames but using the newer frame's
// objects: while the tank turns, the side the camera is turning away from
// looked past what that frame drew, and the ground there popped in a moment
// later (round 27). So the cone gets extra room, enough for about 8 degrees of
// turning per game frame. Parked in the Arena, 1.6 times the room drew nothing
// more, so this costs little.
constexpr float kTurnMargin = 1.3f;

namespace {
    std::atomic<float> widescreen_scale{1.0f};

    // RT64 only widens views that span the full screen width: one player,
    // both halves of the 2-player split, and the top view of 3 players.
    bool is_full_width_view(int32_t players, int32_t player) {
        return players == 1 || players == 2 || (players == 3 && player == 0);
    }
}

void btx::set_widescreen_scale(float scale) {
    widescreen_scale.store(scale, std::memory_order_relaxed);
}

float btx::get_widescreen_scale() {
    return widescreen_scale.load(std::memory_order_relaxed);
}

// Hooked in func_8008228C at 0x800823D0, where both paths have stored k at
// sp+0x14. Widened by the Expand factor (1 in Original) and the turn margin. Player count in $s4, player index in $s1.
extern "C" void btx_widen_cull(uint8_t* rdram, recomp_context* ctx) {
    if (!is_full_width_view((int32_t)ctx->r20, (int32_t)ctx->r17)) {
        return;
    }
    float* k = (float*)(rdram + ((uint32_t)(ctx->r29 + 0x14) - 0x80000000u));
    *k /= btx::get_widescreen_scale() * kTurnMargin;
}
