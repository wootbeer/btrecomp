// Restart (round 34, from snprecomp's round 12): a button in the menu's header
// between Quit and Close, and a choice in the quit prompt (also Alt+F4 or
// closing the window), from lib-patches/RecompFrontend/0005 (snprecomp's
// 0023). It quits as Quit does, and just before the process ends (main's end,
// a quiet shutdown fault in src/main/crash_handler.cpp, or the watchdog
// below) starts the game again with the same command line and working folder.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <spawn.h>
#include <unistd.h>
extern char** environ;
#endif

#include "recompui/recompui.h"
#include "ultramodern/ultramodern.hpp"

#include "btx_restart.h"

namespace {
    std::atomic_bool requested = false;
    std::atomic_bool relaunched = false;
#if !defined(_WIN32)
    std::vector<std::string> arguments;
#endif
}

void btx::restart::install(int argc, char** argv) {
#if !defined(_WIN32)
    arguments.assign(argv, argv + argc);
#else
    (void)argc;
    (void)argv;
#endif
    recompui::set_restart_callback([]() {
        requested = true;
        // If shutting down takes too long, start again anyway. Only for
        // Restart: Quit is unchanged. Saves are written as the game goes.
        std::thread([] {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            relaunch_if_requested();
            fflush(stdout);
            fflush(stderr);
            std::_Exit(EXIT_SUCCESS);
        }).detach();
        ultramodern::quit();
    });
}

void btx::restart::relaunch_if_requested() {
    if (!requested.load() || relaunched.exchange(true)) {
        return;
    }
#if defined(_WIN32)
    wchar_t exe[MAX_PATH * 4];
    DWORD length = GetModuleFileNameW(nullptr, exe, (DWORD)std::size(exe));
    if (length == 0 || length >= std::size(exe)) {
        std::fprintf(stderr, "Restart: couldn't find the game's exe\n");
        return;
    }
    // CreateProcessW may write to the command line, so a copy.
    std::wstring command_line = GetCommandLineW();
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(exe, command_line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    else {
        std::fprintf(stderr, "Restart: couldn't start the game again (error %lu)\n", GetLastError());
    }
#else
    std::vector<char*> argv;
    for (std::string& argument : arguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);
    // The resolved path rather than /proc/self/exe, so the new process keeps
    // its name in process lists.
    char exe[4096];
    ssize_t length = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (length <= 0) {
        std::fprintf(stderr, "Restart: couldn't find the game's executable\n");
        return;
    }
    exe[length] = '\0';
    pid_t pid;
    int error = posix_spawn(&pid, exe, nullptr, nullptr, argv.data(), environ);
    if (error != 0) {
        std::fprintf(stderr, "Restart: couldn't start the game again (error %d)\n", error);
    }
#endif
}
