// Command-line options (round 32) and where settings and saves live.
//
// Any option on the General, Graphics, Sound, Cheats and Hacks tabs can be set
// by its id, as `--id value` or `--id=value` (dashes or underscores, any
// case), plus friendlier names for the common ones (`--aspect`, `--volume`,
// ...). Settings given this way are applied and saved as if changed in the
// menu. `--rom` selects a ROM the way the launcher's button does, and `--play`
// skips the launcher (the runtime's own `--game btx`). `--help` lists it all,
// `--list-options` every option with its values.
#include "btx_command_line.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "SDL.h"
#else
#include "SDL2/SDL.h"
#endif

#include "recompui/config.h"
#include "util/file.h"
#include "librecomp/game.hpp"

namespace {
    const std::u8string game_id = u8"btx.n64.us.1.0";

    // Tabs searched for an option id, in order.
    const char* const option_tabs[] = { "general", "graphics", "sound", "cheats", "hacks" };

    // Friendlier names for common options.
    const std::map<std::string, std::string> aliases = {
        { "aspect", "ar_option" },
        { "aspect_ratio", "ar_option" },
        { "resolution", "res_option" },
        { "window", "wm_option" },
        { "window_mode", "wm_option" },
        { "msaa", "msaa_option" },
        { "antialiasing", "msaa_option" },
        { "framerate", "rr_option" },
        { "framerate_value", "rr_manual_value" },
        { "hud", "hr_option" },
        { "hud_ratio", "hr_option" },
        { "api", "api_option" },
        { "renderer", "api_option" },
        { "high_precision_framebuffer", "hpfb_option" },
        { "downsampling", "ds_option" },
        { "volume", "main_volume" },
        { "filter", "screen_filter" },
        { "deadzone", "joystick_deadzone" },
        { "rumble", "rumble_strength" },
        { "multiplayer", "local_multiplayer" },
    };

    std::string lower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return s;
    }

    // Option names: lowercase, dashes as underscores.
    std::string option_key(const std::string& name) {
        std::string s = lower(name);
        std::replace(s.begin(), s.end(), '-', '_');
        return s;
    }

    // Values: lowercase, no spaces, dashes or underscores ("Map Edge",
    // "map-edge" and "MapEdge" all match).
    std::string value_key(const std::string& value) {
        std::string s;
        for (unsigned char c : value) {
            if (c != ' ' && c != '-' && c != '_') {
                s += (char)std::tolower(c);
            }
        }
        return s;
    }

    std::optional<bool> parse_bool(const std::string& value) {
        std::string v = value_key(value);
        if (v == "on" || v == "true" || v == "yes" || v == "1") return true;
        if (v == "off" || v == "false" || v == "no" || v == "0") return false;
        return std::nullopt;
    }

    recomp::config::Config* find_tab(const std::string& id) {
        try {
            return &recompui::config::get_config(id);
        }
        catch (const std::exception&) {
            return nullptr;
        }
    }

    std::filesystem::path exe_dir() {
        std::error_code ec;
#if defined(_WIN32)
        wchar_t buf[MAX_PATH];
        DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            return std::filesystem::path(std::wstring(buf, len)).parent_path();
        }
        return {};
#elif defined(__linux__)
        return std::filesystem::read_symlink("/proc/self/exe", ec).parent_path();
#else
        return std::filesystem::current_path(ec);
#endif
    }

    bool folder_has_files(const std::filesystem::path& path) {
        std::error_code ec;
        return std::filesystem::is_directory(path, ec) && !std::filesystem::is_empty(path, ec);
    }

    bool write_portable_txt(const std::filesystem::path& dir) {
        std::ofstream out(dir / "portable.txt");
        if (!out) {
            return false;
        }
        out << "This file makes BattleTanx Recompiled keep its settings, saves and ROM copy in this folder.\n"
               "Delete it (or start the game with --no-portable) to use your user folder instead.\n";
        return out.good();
    }

    std::string describe_values(const recomp::config::ConfigOption& option) {
        std::ostringstream out;
        switch (option.type) {
            case recomp::config::ConfigOptionType::Enum: {
                const auto& e = std::get<recomp::config::ConfigOptionEnum>(option.variant);
                for (size_t i = 0; i < e.options.size(); i++) {
                    out << (i ? ", " : "") << e.options[i].key;
                }
                break;
            }
            case recomp::config::ConfigOptionType::Number: {
                const auto& n = std::get<recomp::config::ConfigOptionNumber>(option.variant);
                out << n.min << " to " << n.max;
                break;
            }
            case recomp::config::ConfigOptionType::Bool:
                out << "on, off";
                break;
            default:
                out << "text";
                break;
        }
        return out.str();
    }

    std::string describe_value(const recomp::config::ConfigOption& option, const recomp::config::ConfigValueVariant& value) {
        if (const uint32_t* u = std::get_if<uint32_t>(&value)) {
            if (option.type == recomp::config::ConfigOptionType::Enum) {
                const auto& e = std::get<recomp::config::ConfigOptionEnum>(option.variant);
                auto it = e.find_option_from_value(*u);
                if (it != e.options.end()) return it->key;
            }
            return std::to_string(*u);
        }
        if (const double* d = std::get_if<double>(&value)) {
            std::ostringstream out;
            out << *d;
            return out.str();
        }
        if (const bool* b = std::get_if<bool>(&value)) return *b ? "on" : "off";
        if (const std::string* s = std::get_if<std::string>(&value)) return *s;
        return "";
    }

    // Converts a value to the option's type; empty on a bad value.
    std::optional<recomp::config::ConfigValueVariant> convert_value(const recomp::config::ConfigOption& option, const std::string& value) {
        switch (option.type) {
            case recomp::config::ConfigOptionType::Enum: {
                const auto& e = std::get<recomp::config::ConfigOptionEnum>(option.variant);
                const std::string v = value_key(value);
                for (const auto& choice : e.options) {
                    if (value_key(choice.key) == v || value_key(choice.name) == v) {
                        return recomp::config::ConfigValueVariant{ choice.value };
                    }
                }
                // On/off for two-choice options such as Fog.
                if (auto b = parse_bool(value)) {
                    for (const auto& choice : e.options) {
                        if (value_key(choice.key) == (*b ? "on" : "off")) {
                            return recomp::config::ConfigValueVariant{ choice.value };
                        }
                    }
                }
                return std::nullopt;
            }
            case recomp::config::ConfigOptionType::Number: {
                const auto& n = std::get<recomp::config::ConfigOptionNumber>(option.variant);
                try {
                    size_t used = 0;
                    std::string trimmed = value;
                    if (!trimmed.empty() && trimmed.back() == '%') trimmed.pop_back();
                    double d = std::stod(trimmed, &used);
                    if (used != trimmed.size()) return std::nullopt;
                    if (n.max > n.min) d = std::clamp(d, n.min, n.max);
                    return recomp::config::ConfigValueVariant{ d };
                }
                catch (const std::exception&) {
                    return std::nullopt;
                }
            }
            case recomp::config::ConfigOptionType::Bool: {
                if (auto b = parse_bool(value)) return recomp::config::ConfigValueVariant{ *b };
                return std::nullopt;
            }
            case recomp::config::ConfigOptionType::String:
                return recomp::config::ConfigValueVariant{ value };
            default:
                return std::nullopt;
        }
    }

#ifdef _WIN32
    // The Release build is a GUI app: borrow the console it was started from.
    bool attach_console() {
        static int attached = -1;
        if (attached < 0) {
            attached = AttachConsole(ATTACH_PARENT_PROCESS) ? 1 : 0;
            if (attached) {
                FILE* f = nullptr;
                freopen_s(&f, "CONOUT$", "w", stdout);
                freopen_s(&f, "CONOUT$", "w", stderr);
            }
        }
        return attached == 1;
    }
#endif
}

btx::cli::Options btx::cli::parse(int argc, char** argv) {
    Options options;
    auto next_value = [&](int& i, const std::string& name) -> std::optional<std::string> {
        if (i + 1 < argc) {
            return std::string(argv[++i]);
        }
        options.errors.push_back(name + " needs a value.");
        return std::nullopt;
    };

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "-?" || arg == "/?") {
            options.help = true;
            continue;
        }
        if (arg.rfind("--", 0) != 0) {
            options.errors.push_back("Not an option: \"" + arg + "\" (options start with --).");
            continue;
        }

        std::string name = arg.substr(2);
        std::optional<std::string> inline_value;
        size_t eq = name.find('=');
        if (eq != std::string::npos) {
            inline_value = name.substr(eq + 1);
            name = name.substr(0, eq);
        }
        const std::string key = option_key(name);

        if (key == "help") options.help = true;
        else if (key == "list_options") options.list_options = true;
        else if (key == "play" || key == "start" || key == "skip_launcher") options.play = true;
        else if (key == "portable") options.portable = true;
        else if (key == "no_portable") options.no_portable = true;
        else if (key == "fullscreen") options.settings.emplace_back("wm_option", "Fullscreen");
        else if (key == "windowed") options.settings.emplace_back("wm_option", "Windowed");
        else if (key == "rom" || key == "data_dir") {
            auto value = inline_value ? inline_value : next_value(i, "--" + name);
            if (value) {
                (key == "rom" ? options.rom : options.data_dir) = std::filesystem::u8path(*value);
            }
        }
        else if (key == "game" || key == "game_mode") {
            // The runtime's own options.
            auto value = inline_value ? inline_value : next_value(i, "--" + name);
            if (value) {
                options.runtime_args.push_back(key == "game" ? "--game" : "--game-mode");
                options.runtime_args.push_back(*value);
            }
        }
        else {
            // A setting. Without "=value", take the next argument unless it's
            // another option; a bare flag means "on".
            std::string value = "on";
            if (inline_value) {
                value = *inline_value;
            }
            else if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) {
                value = argv[++i];
            }
            options.settings.emplace_back(key, value);
        }
    }

    if (options.portable && options.no_portable) {
        options.errors.push_back("--portable and --no-portable together: ignoring both.");
        options.portable = options.no_portable = false;
    }
    return options;
}

// Where settings, saves and the ROM copy go:
// - --data-dir: that folder.
// - --portable / --no-portable: writes or removes portable.txt next to the
//   game, then as below. Files already saved elsewhere aren't moved.
// - portable.txt next to the game (or in the folder it was started from, as
//   before): that folder.
// - Otherwise the user folder (%LOCALAPPDATA% on Windows, ~/.config or
//   $APP_FOLDER_PATH on Linux) if it already has files in it or
//   APP_FOLDER_PATH is set: existing installs keep working as they were.
// - Otherwise this is a new install: portable, by writing portable.txt next
//   to the game, unless that folder can't be written to (Program Files, say),
//   in which case the user folder.
std::filesystem::path btx::cli::choose_data_folder(const Options& options) {
    std::error_code ec;
    if (options.data_dir) {
        std::filesystem::create_directories(*options.data_dir, ec);
        return std::filesystem::absolute(*options.data_dir, ec);
    }

    const std::filesystem::path game_dir = exe_dir();
    if (!game_dir.empty()) {
        if (options.no_portable) {
            std::filesystem::remove(game_dir / "portable.txt", ec);
        }
        else if (options.portable && !std::filesystem::exists(game_dir / "portable.txt", ec)) {
            if (!write_portable_txt(game_dir)) {
                fprintf(stderr, "Couldn't write portable.txt in %s; using the user folder.\n", game_dir.string().c_str());
            }
        }
        if (std::filesystem::exists(game_dir / "portable.txt", ec)) {
            return game_dir;
        }
    }
    if (!options.no_portable && std::filesystem::exists("portable.txt", ec)) {
        return std::filesystem::current_path(ec);
    }

    const std::filesystem::path user_folder = recompui::file::get_app_folder_path();
    if (!options.no_portable && getenv("APP_FOLDER_PATH") == nullptr && !folder_has_files(user_folder) && !game_dir.empty() &&
        write_portable_txt(game_dir)) {
        return game_dir;
    }
    // Created now, so the settings saved this run are there next time and the
    // folder counts as an existing install.
    std::filesystem::create_directories(user_folder, ec);
    return user_folder;
}

void btx::cli::select_rom(Options& options) {
    if (!options.rom) {
        return;
    }
    const std::string path = options.rom->string();
    switch (recomp::select_rom(*options.rom, game_id)) {
        case recomp::RomValidationError::Good:
            fprintf(stderr, "ROM selected: %s\n", path.c_str());
            return;
        case recomp::RomValidationError::FailedToOpen:
            options.errors.push_back("Couldn't open the ROM \"" + path + "\".");
            break;
        case recomp::RomValidationError::NotARom:
            options.errors.push_back("\"" + path + "\" isn't an N64 ROM.");
            break;
        case recomp::RomValidationError::IncorrectRom:
            options.errors.push_back("\"" + path + "\" isn't BattleTanx.");
            break;
        case recomp::RomValidationError::IncorrectVersion:
            options.errors.push_back("\"" + path + "\" is a different version of BattleTanx; the USA version is needed.");
            break;
        default:
            options.errors.push_back("\"" + path + "\" couldn't be used as the ROM.");
            break;
    }
}

void btx::cli::apply_settings(Options& options) {
    std::set<recomp::config::Config*> changed;
    for (const auto& [given_name, value] : options.settings) {
        std::string id = given_name;
        if (auto it = aliases.find(id); it != aliases.end()) {
            id = it->second;
        }

        recomp::config::Config* tab = nullptr;
        for (const char* tab_id : option_tabs) {
            recomp::config::Config* candidate = find_tab(tab_id);
            if (candidate != nullptr && candidate->has_option(id)) {
                tab = candidate;
                break;
            }
        }
        if (tab == nullptr) {
            options.errors.push_back("Unknown option --" + given_name + " (see --help and --list-options).");
            continue;
        }

        const recomp::config::ConfigOption& option = tab->get_option(id);
        auto converted = convert_value(option, value);
        if (!converted) {
            options.errors.push_back("--" + given_name + ": \"" + value + "\" isn't one of: " + describe_values(option) + ".");
            continue;
        }
        tab->set_option_value(id, *converted);
        changed.insert(tab);
    }
    // Saved, as when changed in the menu (this also applies graphics changes).
    for (recomp::config::Config* tab : changed) {
        tab->save_config();
    }
}

std::string btx::cli::help_text() {
    return
        "BattleTanx Recompiled command-line options\n"
        "\n"
        "Starting:\n"
        "  --rom <file>          Select the BattleTanx (USA) ROM, like the launcher's button\n"
        "  --play                Skip the launcher and start the game (needs a ROM selected,\n"
        "                        now or before)\n"
        "\n"
        "Where settings and saves go:\n"
        "  --portable            Keep them next to the game (the default for new installs)\n"
        "  --no-portable         Keep them in your user folder instead\n"
        "  --data-dir <folder>   Use this folder for this run\n"
        "  (Switching doesn't move files already saved; copy them over yourself.)\n"
        "\n"
        "Settings (saved, as if changed in the menu):\n"
        "  --fullscreen, --windowed\n"
        "  --aspect original|expand        --resolution original|original2x|auto\n"
        "  --msaa none|2x|4x               --framerate original|display|manual\n"
        "  --framerate-value <fps>         --hud original|16:9|expand\n"
        "  --api auto|d3d12|vulkan         --filter none|crt|scanlines|lcd\n"
        "  --bloom off|low|high            --fog on|off\n"
        "  --draw-distance original|far|very-far|map-edge\n"
        "  --volume <0-100>                --multiplayer on|off\n"
        "  --mouse-stick off|turn|full|turret\n"
        "  Any other option on the General, Graphics, Sound, Cheats or Hacks tab by its id,\n"
        "  for example --unlimited-ammo on or --gs-all-green on.\n"
        "  Values can be given as --name value or --name=value.\n"
        "\n"
        "  --list-options        List every option, its values and its current setting\n"
        "  --help                This text\n"
        "\n"
        "Example: BattleTanxRecompiled --rom \"BattleTanx (USA).z64\" --fullscreen --play\n";
}

std::string btx::cli::options_text() {
    std::ostringstream out;
    out << "Options (set with --<id> <value>):\n";
    for (const char* tab_id : option_tabs) {
        recomp::config::Config* tab = find_tab(tab_id);
        if (tab == nullptr) continue;
        out << "\n[" << tab_id << "]\n";
        for (const auto& option : tab->get_config_schema().options) {
            if (option.hidden) continue;
            std::string id = option.id;
            std::replace(id.begin(), id.end(), '_', '-');
            out << "  --" << id << "  (" << option.name << ")  values: " << describe_values(option)
                << "  now: " << describe_value(option, tab->get_option_value(option.id)) << "\n";
        }
    }
    return out.str();
}

void btx::cli::show_text(const std::string& title, const std::string& text) {
#ifdef _WIN32
    if (!attach_console()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title.c_str(), text.c_str(), nullptr);
        return;
    }
    // Start on a fresh line after the console's prompt.
    fprintf(stdout, "\n");
#endif
    fprintf(stdout, "%s\n", text.c_str());
    fflush(stdout);
}
