#ifndef __BTX_CHEATS_H__
#define __BTX_CHEATS_H__

#include <cstdint>
#include <string>
#include <vector>

// The Cheats and Hacks tabs (src/main/cheats_config.cpp) and what each cheat
// does in the game (src/game/cheats.cpp), as in the sequel's port.
namespace btx::cheats {
    enum class Section {
        Stock,     // the game's own codes
        GameShark,
    };

    struct Choice {
        std::string key;  // saved in the tab's .json
        std::string name; // shown in the menu
    };

    struct Cheat {
        std::string id;
        std::string name;
        std::string description;
        Section section;
        // Empty: an on/off toggle. Otherwise the choices of a picker, the
        // first one being "off".
        std::vector<Choice> choices;
    };

    // Every cheat, in menu order.
    const std::vector<Cheat>& list();

    // The value of cheat `index` in list(): 0/1 for a toggle, the choice
    // otherwise. Applied on the game's next frame.
    void set_value(size_t index, uint32_t value);

    // Adds the Cheats and Hacks tabs. Call between the other create_*_tab()
    // calls and recompui::config::finalize().
    void add_cheats_tabs();
}

#endif
