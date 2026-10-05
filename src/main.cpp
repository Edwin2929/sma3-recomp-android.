#include <cstdlib>
#include "runtime.h"
#include "runtime_arm.h"
#include "localization.h"
int sma3_ram_dispatch(uint32_t, int);

#ifdef __ANDROID__
#include <SDL.h>
#include <unistd.h>
#include <string>
#include <vector>
#endif

int main(int argc, char** argv) {
    g_runtime_ram_dispatch_hook = sma3_ram_dispatch;
    g_runtime_bus_read_override = sma3::localized_read;
#ifndef __ANDROID__
    if (const char* lang = std::getenv("SMA3_LANGUAGE")) sma3::language.store(std::atoi(lang));
#endif
    gbarecomp::RunOptions options;
    options.builtin_game_name = "SMA3 USA experimental recompilation";
    options.builtin_rom_sha1 = "7352d2bd064d9ebaec579e264228aa21c7345b80";
#ifdef __ANDROID__
    options.show_fps_by_default = true;
    const char* storage = SDL_AndroidGetInternalStoragePath();
    if (!storage) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No Android internal storage path");
        return 1;
    }
    const std::string root(storage);
    if (chdir(root.c_str()) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot open app storage directory");
        return 1;
    }
    SDL_setenv("GBARECOMP_STRICT_STATIC", "1", 1);
    SDL_setenv("GBARECOMP_FORCE_INTERP", "0", 1);
    SDL_setenv("GBARECOMP_BIOS_HLE", "0", 1);
    SDL_setenv("GBARECOMP_BIOS_SKIP_INTRO", "0", 1);
    std::vector<std::string> arguments = {
        root + "/sma3_runner", "--config", root + "/variants/sma3-usa/game.toml",
        "--rom", root + "/roms/sma3-usa.gba",
        "--bios", root + "/bios/gba_bios.bin", "--window"
    };
    std::vector<char*> pointers;
    for (auto& argument : arguments) pointers.push_back(argument.data());
    return gbarecomp::run_game(static_cast<int>(pointers.size()), pointers.data(), options);
#else
    return gbarecomp::run_game(argc, argv, options);
#endif
}
