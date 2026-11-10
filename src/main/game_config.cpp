// Game-specific options on the General tab: which pak each controller holds,
// local multiplayer and Mouse Stick; and the screen filter and bloom on the Graphics tab.
// See include/btx_config.h.
#include "btx_config.h"

#include "recompui/config.h"
#include "rt64_screen_filter.h"

namespace {
    using Accessory = btx::config::Accessory;

    const std::string p1_accessory_id = "p1_accessory";
    const std::string other_accessory_id = "other_accessory";
    const std::string local_multiplayer_id = "local_multiplayer";

    const std::vector<recomp::config::ConfigOptionEnumOption> accessory_options = {
        {Accessory::ControllerPak, "ControllerPak", "Controller Pak"},
        {Accessory::RumblePak, "RumblePak", "Rumble Pak"},
        {Accessory::None, "None", "None"},
    };

    // Looked up by id on each read: the Config& create_general_tab() returns
    // lives in a container that moves when later tabs are created, so a
    // pointer kept from it dangles (btgarecomp round 96).
    bool options_added = false;
}

void btx::config::add_general_options(recomp::config::Config& general) {
    options_added = true;

    general.add_enum_option(
        p1_accessory_id,
        "Player 1 Accessory",
        "The pak plugged into player 1's controller. As on the N64, a controller holds one pak at a time: "
        "the <b>Controller Pak</b> stores saves (in the saves folder as a standard 32 KB .mpk file), "
        "the <b>Rumble Pak</b> vibrates your controller."
        "<br/><br/>"
        "Changing this is like swapping paks: the game notices the next time it checks.",
        accessory_options,
        Accessory::ControllerPak
    );

    general.add_enum_option(
        other_accessory_id,
        "Players 2-4 Accessory",
        "The pak plugged into the other players' controllers in local multiplayer. "
        "Each player's Controller Pak is a separate .mpk file.",
        accessory_options,
        Accessory::RumblePak
    );

    general.add_bool_option(
        local_multiplayer_id,
        "Local Multiplayer",
        "Lets up to 4 players use their own controllers."
        "<br/><br/>"
        "<b>Restart the game after changing this.</b> Then open the Controls tab and use "
        "<b>Assign players</b> to choose each player's controller or keyboard.",
        false
    );

    general.add_enum_option(
        "mouse_stick",
        "Mouse Stick",
        "Moves player 1's analog stick with the mouse. <b>Turn</b> uses left-right motion only, to turn "
        "while the keys or a controller drive; <b>Full</b> also drives with up-down motion. "
        "<b>Turret</b> turns the turret with left-right motion instead, and turns on the GameShark code "
        "that lets the turret turn on its own. The mouse is held in the window while you play; open the "
        "menu to free it."
        "<br/><br/>"
        "Mouse buttons, the side buttons and the scroll wheel can be bound on the Controls tab.",
        { {0u, "Off", "Off"}, {1u, "Turn", "Turn"}, {2u, "Full", "Full"}, {3u, "Turret", "Turret"} },
        0u
    );
    general.add_number_option(
        "mouse_stick_sensitivity",
        "Mouse Stick Sensitivity",
        "How far the stick moves for the mouse's motion.",
        5, 300, 1, 0, true, 100
    );
    general.add_option_disable_dependency("mouse_stick_sensitivity", "mouse_stick", 0u);

    // Also called when the saved settings load.
    static int mouse_mode = 0;
    static double mouse_sensitivity = 100.0;
    general.add_option_change_callback("mouse_stick",
        [](recomp::config::ConfigValueVariant value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            mouse_mode = (int)std::get<uint32_t>(value);
            btx::set_mouse_stick(mouse_mode, mouse_sensitivity);
        });
    general.add_option_change_callback("mouse_stick_sensitivity",
        [](recomp::config::ConfigValueVariant value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            mouse_sensitivity = std::get<double>(value);
            btx::set_mouse_stick(mouse_mode, mouse_sensitivity);
        });
    btx::init_mouse_stick();
}

void btx::config::add_graphics_options(recomp::config::Config& graphics) {
    const std::vector<recomp::config::ConfigOptionEnumOption> screen_filter_options = {
        {RT64::ScreenFilter::None, "None", "None"},
        {RT64::ScreenFilter::CRT, "CRT", "CRT"},
        {RT64::ScreenFilter::Scanlines, "Scanlines", "Scanlines"},
        {RT64::ScreenFilter::LCD, "LCD", "LCD"},
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> bloom_options = {
        {RT64::Bloom::Off, "Off", "Off"},
        {RT64::Bloom::Low, "Low", "Low"},
        {RT64::Bloom::High, "High", "High"},
    };

    graphics.add_enum_option(
        "screen_filter",
        "Screen Filter",
        "Makes the picture look like it does on a screen of the time: "
        "<b>CRT</b> a TV, <b>Scanlines</b> just the TV's lines, <b>LCD</b> a handheld's pixel grid.",
        screen_filter_options,
        RT64::ScreenFilter::None
    );
    graphics.add_enum_option(
        "bloom",
        "Bloom",
        "A glow around bright, strongly coloured parts of the picture, like fire, explosions and energy shots.",
        bloom_options,
        RT64::Bloom::Off
    );

    // Also called when the saved settings load.
    graphics.add_option_change_callback("screen_filter",
        [](recomp::config::ConfigValueVariant value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            RT64::setScreenFilter(static_cast<RT64::ScreenFilter>(std::get<uint32_t>(value)));
        });
    graphics.add_option_change_callback("bloom",
        [](recomp::config::ConfigValueVariant value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            RT64::setBloom(static_cast<RT64::Bloom>(std::get<uint32_t>(value)));
        });
}

btx::config::Accessory btx::config::get_accessory(int port) {
    if (!options_added) {
        return Accessory::None;
    }
    const std::string& id = port == 0 ? p1_accessory_id : other_accessory_id;
    return static_cast<Accessory>(std::get<uint32_t>(recompui::config::get_general_config().get_option_value(id)));
}

bool btx::config::get_local_multiplayer() {
    if (!options_added) {
        return false;
    }
    return std::get<bool>(recompui::config::get_general_config().get_option_value(local_multiplayer_id));
}
