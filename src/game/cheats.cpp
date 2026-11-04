// What the Cheats and Hacks tabs do in the game (include/btx_cheats.h), as in
// the sequel's port (btgarecomp src/game/cheats.cpp).
//
// Stock codes are the game's own: the Input Code screen (func_800DCA7C
// matches the text against the table at 0x80135698) sets one or more flags,
// and the toggle sets the same flags. While a toggle is on its flags are kept
// set; turning it off clears them once (unless another toggle that's on uses
// the same flag), so typing the code in the game still works with the toggle
// off.
//
// GameShark codes are the community's codes for this ROM. The ones that write
// to the game's memory are applied once a frame here, as the cheat device did
// (codes 80, 81 and D0-D3). The rest change the game's code (one
// instruction). The recompiled code doesn't read its instructions from
// memory, so writing them does nothing; instead each one is a hook in
// battletanx.us.rev0.toml that asks btx_cheat() which version of the
// instruction to run.
#include <atomic>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

#include "btx_cheats.h"
#include "recomp.h"

namespace {
    using btx::cheats::Cheat;
    using btx::cheats::Choice;
    using btx::cheats::Section;

    // Read by the hooks in battletanx.us.rev0.toml through btx_cheat(); the
    // numbers are used there.
    enum Hook {
        NoHook = -1,
        AllGreen = 0,             // func_80082AA8 0x80082AB0
        AllInvisible = 1,         // func_80082AA8 0x80082AB4
        LightTanks = 2,           // func_80086DC0 0x80086DDC
        BonusControl = 3,         // func_80097038 0x800971EC
        AlliesIndestructible = 4, // func_8009FA70 0x8009FB28 (delay slot)
        ThroughWalls = 5,         // func_80107F14 0x80108188
        BuildingsNoDamage = 6,    // func_80092734 0x80092990
        ShootThroughWalls = 7,    // func_8008E480 0x8008E528 (delay slot)
        EverybodyInvisible = 8,   // func_8008B48C 0x8008B504 (a branch)
        NoQueenPickup = 9,        // func_80088CF0 0x8008A2F0
        QueenLordResult = 10,     // func_800A66C8 0x800A6744: 1 win, 2 lose
        SlowTanks = 11,           // func_8009A354 0x8009A568
        BuildingsFallFast = 12,   // func_80091A60 0x80091BBC
        WallsOff = 13,            // func_8010A680 0x8010A690
        NukeYellow = 14,          // func_800F3234 0x800F3330 (delay slot)
        NukePink = 15,            // func_8008054C 0x8008055C
        LaserInvisible = 16,      // func_8008463C 0x800846EC
        HookCount
    };

    struct Entry {
        Cheat cheat;
        // Stock: the game's flags (bytes).
        std::vector<uint32_t> flags = {};
        // Memory codes, one list per choice after "off" (one list for a toggle).
        std::vector<std::vector<const char*>> codes = {};
        Hook hook = NoHook;
    };

    Entry stock(const char* id, const char* name, const char* code, const char* text, std::vector<uint32_t> flags) {
        std::string description = std::string(text) + (text[0] ? "<br/><br/>" : "") + "Code: <b>" + code + "</b>";
        return { { id, name, description, Section::Stock, {} }, flags };
    }

    std::string code_text(const std::vector<const char*>& codes) {
        std::string ret = codes.size() > 2 ? "GameShark code:<br/><b>" : "GameShark code: <b>";
        for (size_t i = 0; i < codes.size(); i++) {
            ret += (i ? (i % 2 ? " " : "<br/>") : "") + std::string(codes[i]);
        }
        return ret + "</b>";
    }

    Entry gs(const char* id, const char* name, const char* text, std::vector<const char*> codes) {
        std::string description = std::string(text) + (text[0] ? "<br/><br/>" : "") + code_text(codes);
        return { { id, name, description, Section::GameShark, {} }, {}, { codes } };
    }

    // A code that changes the game's code, done with a hook.
    Entry gs_hook(const char* id, const char* name, const char* text, const char* code, Hook hook) {
        std::string description = std::string(text) + (text[0] ? "<br/><br/>" : "") + "GameShark code: <b>" + code + "</b>";
        return { { id, name, description, Section::GameShark, {} }, {}, {}, hook };
    }

    std::vector<Entry> make_entries() {
        std::vector<Entry> e;

        // Stock codes, in the order of the game's table. Flags from func_800DCA7C.
        e.push_back(stock("unlimited_ammo", "Unlimited Ammo", "LTSFBLLTS", "", { 0x80135760 }));
        e.push_back(stock("all_weapons", "All Weapons", "PLVRZM", "", { 0x80135765 }));
        e.push_back(stock("invincible", "Invincible", "MSTSRVV", "", { 0x80135763 }));
        e.push_back(stock("all_levels", "All Levels", "DUMMYHAHA",
            "Typed in the game, this code also turns on Unlimited Ammo and Invincible.", { 0x80135767, 0x80135761 }));
        e.push_back(stock("invisible", "Invisible", "CRSTLCLR", "", { 0x8013576A }));
        e.push_back(stock("hurl_mode", "Spinning View (Hurl Mode)", "HVRL", "", { 0x8013576B }));
        e.push_back(stock("storm_ravens", "Storm Ravens Gang", "WMNRSMRTR",
            "The all-women Storm Ravens gang, with the laser and cloak power-ups.", { 0x80135773, 0x8012579A }));
        e.push_back(stock("run_story", "Run Story", "CDPLT", "", { 0x80135770 }));
        e.push_back(stock("trippy", "Psychedelic View (Trippy)", "CNCTHRTM", "", { 0x80135771 }));
        e.push_back(stock("unlimited_lives", "Unlimited Lives", "LVFRVR", "", { 0x80135772 }));
        e.push_back(stock("campaign_gangs", "All Gangs in Campaign", "LTSLTSGNGS", "", { 0x8012579A }));
        e.push_back(stock("frogs", "Frogs", "FRGZ", "", { 0x80135775 }));
        e.push_back(stock("toads", "Toads", "TDZ", "Replaces the Queen Lords with toads.", { 0x80135776 }));

        // GameShark. Left out: the ones that set the stock codes' flags (the
        // toggles above), and the sequel's codes that circulate in lists for
        // this game (0x80117F49, 0x80125AB0-0x80125ABC), which land on
        // unrelated data here.
        e.push_back(gs("gs_p1_radar", "P1 Always Has Radar", "", { "802D8195", "0080" }));
        e.push_back(gs("gs_p2_radar", "P2 Always Has Radar", "", { "802D9ABD", "0080" }));
        e.push_back(gs("gs_p1_cloak", "P1 Always Cloaked", "", { "802D818D", "0001" }));
        e.push_back(gs("gs_p2_cloak", "P2 Always Cloaked", "", { "802D9AB5", "0001" }));
        e.push_back(gs("gs_p1_goliath_radar", "P1 Goliath On Tracks Always Has Radar", "", { "802D869D", "0080" }));
        e.push_back(gs("gs_p2_goliath_radar", "P2 Goliath On Tracks Always Has Radar", "", { "802D9FC5", "0080" }));
        e.push_back(gs("gs_p1_goliath_cloak", "P1 Goliath On Tracks Always Cloaked", "", { "802D8695", "0001" }));
        e.push_back(gs("gs_p2_goliath_cloak", "P2 Goliath On Tracks Always Cloaked", "", { "802D9FBD", "0001" }));
        e.push_back(gs("gs_p1_one_hit", "P1 One Hit And You Bite It", "", { "802D819F", "0001" }));
        e.push_back(gs("gs_p2_one_hit", "P2 One Hit And You Bite It", "", { "802D9AC7", "0001" }));
        e.push_back(gs("gs_p1_ammo", "P1 Infinite Ammo", "", { "802D8603", "00FF" }));
        e.push_back(gs("gs_p2_ammo", "P2 Infinite Ammo", "", { "802D9F2B", "00FF" }));
        e.push_back(gs("gs_p1_guided", "Unlimited Guided Missiles", "", { "802D8617", "0030" }));
        e.push_back(gs("gs_p1_laser", "Unlimited Laser", "", { "802D860B", "002F" }));
        e.push_back(gs("gs_p1_gun_buddies", "Unlimited Gun Buddies", "", { "802D861B", "0002" }));
        e.push_back(gs("gs_p1_swarmers", "Unlimited Swarmer Missiles", "", { "802D8609", "0036" }));
        e.push_back(gs("gs_p1_grenades", "Unlimited Grenades", "", { "802D860F", "0062" }));
        e.push_back(gs("gs_p1_mines", "Unlimited Mines", "", { "802D8619", "0040" }));
        e.push_back(gs("gs_p1_health", "Unlimited Health Pickups", "", { "802D8621", "0005" }));
        e.push_back(gs("gs_p1_nuke", "Unlimited Nuke", "", { "802D860D", "0002" }));
        e.push_back(gs("gs_p1_cloak_pickups", "Unlimited Cloak", "", { "802D861F", "0003" }));
        e.push_back(gs("gs_p1_special", "P1 Infinite Special Weapons (Multiplayer)",
            "Every special weapon in Battlelord and the other multiplayer modes: new weapons, nuclear lasers, smoke "
            "screen and more. You may have to self destruct to get them.",
            { "802D8609", "00FF", "802D860B", "00FF", "802D860D", "00FF", "802D860F", "00FF", "802D8611", "00FF",
              "802D8615", "00FF", "802D8617", "00FF", "802D8619", "00FF", "802D861B", "00FF", "802D861D", "00FF",
              "802D861F", "00FF", "802D8621", "00FF" }));
        e.push_back(gs("gs_p2_special", "P2 Infinite Special Weapons (Multiplayer)",
            "Every special weapon in Battlelord and the other multiplayer modes.",
            { "802D9F31", "00FF", "802D9F33", "00FF", "802D9F35", "00FF", "802D9F37", "00FF", "802D9F39", "00FF",
              "802D9F3D", "00FF", "802D9F3F", "00FF", "802D9F41", "00FF", "802D9F43", "00FF", "802D9F45", "00FF",
              "802D9F47", "00FF", "802D9F49", "00FF" }));
        e.push_back(gs("gs_p1_turret", "P1 Tank Turret Always On", "Turn the turret with C-Left and C-Right.",
            { "802DE7FB", "0011", "802DE7FF", "0002" }));
        e.push_back(gs("gs_p2_turret", "P2 Tank Turret Always On", "", { "802DEB43", "0011", "802DEB47", "0002" }));
        e.push_back(gs("gs_p3_turret", "P3 Tank Turret Always On", "", { "802DEE8B", "0011", "802DEE8F", "0002" }));
        e.push_back(gs("gs_p4_turret", "P4 Tank Turret Always On", "", { "802DF1D3", "0011", "802DF1D7", "0002" }));
        e.push_back(gs("gs_black_tanks", "Black Tanks (Multiplayer)", "", { "802DE7DF", "000A" }));
        e.push_back(gs_hook("gs_all_green", "Who's Who: All Tanks Are Green", "", "80082AB0 0001", AllGreen));
        e.push_back(gs_hook("gs_all_invisible", "All Tanks Are Invisible", "", "80082AB4 0001", AllInvisible));
        e.push_back(gs_hook("gs_everybody_invisible", "Everybody Is Totally Invisible", "", "8008B504 0001", EverybodyInvisible));
        e.push_back(gs_hook("gs_light_tanks", "Light Colored Tanks", "", "80086DDE 0001", LightTanks));
        e.push_back(gs_hook("gs_slow_tanks", "Slow Tanks", "", "8009A56A 0001", SlowTanks));
        e.push_back(gs_hook("gs_through_walls", "Go Through Walls", "Doesn't work in the Arena.", "80108188 0001", ThroughWalls));
        e.push_back(gs_hook("gs_shoot_through_walls", "Shoot Through Walls", "", "8008E529 0001", ShootThroughWalls));
        e.push_back(gs_hook("gs_walls_off", "Walls Off", "Also means power-ups can't be picked up.", "8010A691 0001", WallsOff));
        e.push_back(gs_hook("gs_buildings_no_damage", "Buildings Don't Show Damage", "Until they're destroyed.", "80092990 0001", BuildingsNoDamage));
        e.push_back(gs_hook("gs_buildings_fall_fast", "Buildings Fall Fast", "", "80091BBC 0001", BuildingsFallFast));
        e.push_back(gs_hook("gs_laser_invisible", "Laser Fire Invisible", "", "800846ED 0001", LaserInvisible));
        e.push_back(gs_hook("gs_nuke_yellow", "Nuke Blast Leaves Air Yellow", "", "800F3333 0001", NukeYellow));
        e.push_back(gs_hook("gs_nuke_pink", "Nuke Blast Pink", "", "8008055F 0001", NukePink));
        e.push_back(gs_hook("gs_no_queen_pickup", "Can't Pick Up Queen Lords", "", "8008A2F0 0001", NoQueenPickup));
        {
            Entry queen = gs_hook("gs_queen_lord_levels", "Queen Lord Levels", "", "", QueenLordResult);
            queen.cheat.description = "GameShark code: <b>800A6745 0001</b> (auto win) or <b>800A6746 0001</b> (auto lose)";
            queen.cheat.choices = { { "Normal", "Normal" }, { "AutoWin", "Auto Win" }, { "AutoLose", "Auto Lose" } };
            e.push_back(queen);
        }
        e.push_back(gs_hook("gs_allies_indestructible", "Regenerated Allied Tanks Are Indestructible", "In the campaign.",
            "8009FB2A 0001", AlliesIndestructible));
        e.push_back(gs_hook("gs_bonus_control", "Full Control Of Tank In Bonus Levels",
            "And of the extra side player in Battlelord.", "800971EE 0001", BonusControl));
        return e;
    }

    const std::vector<Entry>& entries() {
        static const std::vector<Entry> list = make_entries();
        return list;
    }

    // Set by the menu, read by the game thread.
    std::vector<std::atomic<uint32_t>>& wanted() {
        static std::vector<std::atomic<uint32_t>> values(entries().size());
        return values;
    }

    // Hook number -> entry.
    std::vector<int>& hook_entries() {
        static std::vector<int> map = [] {
            std::vector<int> ret(HookCount, -1);
            for (size_t i = 0; i < entries().size(); i++) {
                if (entries()[i].hook != NoHook) {
                    ret[entries()[i].hook] = (int)i;
                }
            }
            return ret;
        }();
        return map;
    }

    struct Code {
        uint8_t type;
        uint32_t addr;
        uint16_t value;
    };

    std::vector<Code> parse(const std::vector<const char*>& text) {
        std::vector<Code> ret;
        for (size_t i = 0; i + 1 < text.size(); i += 2) {
            uint32_t word = (uint32_t)std::strtoul(text[i], nullptr, 16);
            ret.push_back({ (uint8_t)(word >> 24), 0x80000000u | (word & 0xFFFFFF), (uint16_t)std::strtoul(text[i + 1], nullptr, 16) });
        }
        return ret;
    }

    uint8_t read8(uint8_t* rdram, uint32_t addr) {
        return (uint8_t)MEM_B(0, (gpr)(int32_t)addr);
    }

    void write8(uint8_t* rdram, uint32_t addr, uint8_t value) {
        if (addr < 0x80800000u) {
            MEM_B(0, (gpr)(int32_t)addr) = (int8_t)value;
        }
    }

    uint16_t read16(uint8_t* rdram, uint32_t addr) {
        return (uint16_t)((read8(rdram, addr) << 8) | read8(rdram, addr + 1));
    }

    void run_codes(uint8_t* rdram, const std::vector<Code>& codes) {
        for (size_t i = 0; i < codes.size(); i++) {
            const Code& c = codes[i];
            switch (c.type) {
                case 0x80:
                    write8(rdram, c.addr, (uint8_t)c.value);
                    break;
                // 16-bit writes go in as two bytes, which also covers odd addresses.
                case 0x81:
                    write8(rdram, c.addr, (uint8_t)(c.value >> 8));
                    write8(rdram, c.addr + 1, (uint8_t)c.value);
                    break;
                // Conditionals: the next line only runs if the test passes.
                case 0xD0: if (read8(rdram, c.addr) != (c.value & 0xFF)) i++; break;
                case 0xD1: if (read16(rdram, c.addr) != c.value) i++; break;
                case 0xD2: if (read8(rdram, c.addr) == (c.value & 0xFF)) i++; break;
                case 0xD3: if (read16(rdram, c.addr) == c.value) i++; break;
            }
        }
    }
}

const std::vector<Cheat>& btx::cheats::list() {
    static const std::vector<Cheat> list = [] {
        std::vector<Cheat> ret;
        for (const Entry& e : entries()) {
            ret.push_back(e.cheat);
        }
        return ret;
    }();
    return list;
}

void btx::cheats::set_value(size_t index, uint32_t value) {
    if (index < wanted().size()) {
        wanted()[index].store(value);
    }
}

// Each pass of the controller thread's loop (func_800A8C88, 0x800A8CE8), on
// the menus and in game.
extern "C" void btx_cheats_frame(uint8_t* rdram) {
    static const std::vector<std::vector<std::vector<Code>>> codes = [] {
        std::vector<std::vector<std::vector<Code>>> ret(entries().size());
        for (size_t i = 0; i < entries().size(); i++) {
            for (const auto& list : entries()[i].codes) {
                ret[i].push_back(parse(list));
            }
        }
        return ret;
    }();
    // Stock flags the toggles set on the last pass.
    static std::unordered_map<uint32_t, bool> flags_held;

    std::unordered_map<uint32_t, bool> flags_now;
    for (size_t i = 0; i < entries().size(); i++) {
        const Entry& e = entries()[i];
        uint32_t value = wanted()[i].load();
        if (value == 0) {
            continue;
        }
        for (uint32_t flag : e.flags) {
            flags_now[flag] = true;
        }
        if (value - 1 < codes[i].size()) {
            run_codes(rdram, codes[i][value - 1]);
        }
    }
    for (const auto& [flag, on] : flags_now) {
        write8(rdram, flag, 1);
    }
    for (const auto& [flag, on] : flags_held) {
        if (!flags_now.count(flag)) {
            write8(rdram, flag, 0);
        }
    }
    flags_held.swap(flags_now);
}

// Everybody Is Totally Invisible (func_8008B48C): the bne at 0x8008B504 is
// made not to branch by giving $s1 $v1's value for the comparison; $s1 is
// put back after the delay slot (0x8008B50C, also a branch target, hence the
// flag). The game thread is the only one that runs these.
namespace {
    bool branch_off = false;
    gpr branch_saved_s1 = 0;
}

extern "C" void btx_cheat_branch_off(recomp_context* ctx) {
    branch_off = true;
    branch_saved_s1 = ctx->r17;
    ctx->r17 = ctx->r3;
}

extern "C" void btx_cheat_branch_restore(recomp_context* ctx) {
    if (branch_off) {
        ctx->r17 = branch_saved_s1;
        branch_off = false;
    }
}

// The hooks' question: is this cheat on (or which choice)?
extern "C" int btx_cheat(int hook) {
    if (hook < 0 || hook >= HookCount || hook_entries()[hook] < 0) {
        return 0;
    }
    return (int)wanted()[hook_entries()[hook]].load();
}
