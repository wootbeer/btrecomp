#ifndef __BTX_COMMAND_LINE_H__
#define __BTX_COMMAND_LINE_H__

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Command-line options and where settings and saves live
// (src/main/command_line.cpp). `--help` lists everything.
namespace btx::cli {
    struct Options {
        bool help = false;
        bool list_options = false;
        bool play = false;          // go straight into the game (the runtime's --game btx)
        bool portable = false;      // switch to portable mode
        bool no_portable = false;   // switch back to the user folder
        std::optional<std::filesystem::path> rom;
        std::optional<std::filesystem::path> data_dir;
        // Settings as given ("name", "value"), checked once the options exist.
        std::vector<std::pair<std::string, std::string>> settings;
        // Passed on to the runtime (its own --game and --game-mode).
        std::vector<std::string> runtime_args;
        std::vector<std::string> errors;
    };

    Options parse(int argc, char** argv);

    // The folder settings, saves and the ROM copy go in. Call after the
    // program id is set. Portable is the default for new installs: see the
    // comment above the definition.
    std::filesystem::path choose_data_folder(const Options& options);

    // Selects the ROM given with --rom. Call after the game is registered.
    void select_rom(Options& options);

    // Applies and saves the settings given. Call after
    // recompui::config::finalize().
    void apply_settings(Options& options);

    // --help and --list-options text; the latter needs the options to exist.
    std::string help_text();
    std::string options_text();

    // Prints to the console the game was started from; on Windows, where the
    // Release build has no console of its own, shows a message box when there
    // isn't one.
    void show_text(const std::string& title, const std::string& text);
}

#endif
