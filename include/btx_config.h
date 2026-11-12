#ifndef __BTX_CONFIG_H__
#define __BTX_CONFIG_H__

#include <cstdint>

#include "librecomp/config.hpp"

// Game-specific options, shown on the General and Graphics tabs
// (src/main/game_config.cpp).
namespace btx::config {
    // What's plugged into a controller's accessory slot. Like on the N64, one
    // controller holds one pak at a time.
    enum class Accessory : uint32_t {
        ControllerPak, // saves -- src/game/controller_pak_hle.cpp
        RumblePak,
        None,
    };

    // Adds this game's options to the General tab. Call between
    // recompui::config::create_general_tab() and recompui::config::finalize().
    void add_general_options(recomp::config::Config& general);

    // Screen Filter and Bloom at the end of the Graphics tab, passed straight
    // to RT64 (lib-patches/rt64/0008, 0009), then Fog and Draw Distance
    // (src/game/view_distance.cpp). Call between
    // recompui::config::create_graphics_tab() and recompui::config::finalize().
    void add_graphics_options(recomp::config::Config& graphics);

    Accessory get_accessory(int port);
    bool get_local_multiplayer();
}

// General tab: Mouse Stick (src/main/mouse_stick.cpp).
namespace btx {
    // Mode 0 Off, 1 Turn, 2 Full, 3 Turret; sensitivity in percent.
    void set_mouse_stick(int mode, double percent);
    // Turret mode: the turret hack is on and the mouse turns the turret.
    bool mouse_turret_active();
    // Lets the mouse be captured while Mouse Stick is on. Call once at startup.
    void init_mouse_stick();
    // The input callback: recompinput's, plus the mouse for player 1.
    bool get_n64_input_with_mouse(int player_index, uint16_t* buttons_out, float* x_out, float* y_out);
}

// Graphics tab options read by the game's hooks (src/game/view_distance.cpp).
namespace btx {
    void set_fog_enabled(bool enabled);
    // 0 Original, 1 Far, 2 Very Far, 3 Map Edge.
    void set_draw_distance(int level);
}

// Widescreen (src/game/widescreen.cpp): how much wider than 4:3 RT64 is
// drawing full-width 3D views -- the window aspect / (4/3) in Expand mode,
// 1 otherwise. Updated by the frontend every frame.
namespace btx {
    void set_widescreen_scale(float scale);
    float get_widescreen_scale();
}

#endif
