#include <cstdlib>
#if defined(SMA3_PANORAMA) || defined(SMA3_WIDESCREEN_DIAGNOSTICS)
#include "widescreen.h"
#endif
#ifdef SMA3_WIDESCREEN_DIAGNOSTICS
#include "../validation/widescreen_probe.h"
#endif
#include "image_options.h"
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
    // Desktop qualification can exercise the production callbacks directly;
    // Android takes its exclusive image mode only from the saved Java setting.
    if (const char* mode = std::getenv("SMA3_IMAGE_MODE")) {
        char* end=nullptr;
        const long value=std::strtol(mode,&end,10);
        sma3::image_mode.store(end!=mode && end && *end=='\0' && value>=0 && value<=2
            ? sma3::validated_image_mode(static_cast<int>(value)) : sma3::ImageMode::Original);
    }
#endif
    gbarecomp::RunOptions options;
#ifdef SMA3_PANORAMA
    options.resize_driven_view = true;
    options.max_resize_view_width = 356;
    options.max_resize_view_height = 160;
    options.resize_view_sizing = gbarecomp::RunOptions::ViewSizing::Aspect;
    options.orientation = gbarecomp::RunOptions::Orientation::Any;
    options.freely_resizable_window = true;
    options.extended_view_frame = [](const gbarecomp::ExtendedViewFrameInfo* frame) {
        sma3::wide::frame(frame, sma3::image_mode.load() == sma3::ImageMode::Panorama);
    };
    options.presentation_request = [](gbarecomp::RunOptions::PresentationRequest* request) {
        if (!request) return;
        // Scene coverage belongs to the rasterized frame. Keep its geometry
        // stable and let the compositor black out unauthored margins; live
        // VBlank OAM may already describe the NEXT frame at presentation.
        request->native_view = sma3::image_mode.load() != sma3::ImageMode::Panorama;
        request->anchor_top = false;
    };
#endif
#ifdef SMA3_WIDESCREEN_DIAGNOSTICS
    if (std::getenv("SMA3_WIDE_PROBE_DIR")) {
        options.max_view_width = 356;
        options.extended_view_frame = [](const gbarecomp::ExtendedViewFrameInfo* frame) {
            sma3_probe_frame(frame);
            const char* authored = std::getenv("SMA3_AUTHORED_WIDE");
            if (authored && authored[0] == '1') sma3::wide::frame(frame);
        };
    }
#endif
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
    SDL_setenv("GBARECOMP_BIOS_SKIP_INTRO", "1", 1);
    std::vector<std::string> arguments = {
        root + "/sma3_runner", "--config", root + "/variants/sma3-usa/game.toml",
        "--rom", root + "/roms/sma3-usa.gba",
        "--bios", root + "/bios/gba_bios.bin", "--window", "--resize-view"
    };
    std::vector<char*> pointers;
    for (auto& argument : arguments) pointers.push_back(argument.data());
    return gbarecomp::run_game(static_cast<int>(pointers.size()), pointers.data(), options);
#else
    return gbarecomp::run_game(argc, argv, options);
#endif
}
