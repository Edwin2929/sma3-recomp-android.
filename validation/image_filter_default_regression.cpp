// Print deterministic displayed-pixel fingerprints for the default scaler.
// Compile this same source against both the 0.10.2 video_scaler.h and the
// candidate include directory, then compare stdout byte for byte. To also
// compare every rendered pixel byte in one process, define
// SMA3_BASELINE_VIDEO_SCALER_HEADER to the path of that historical header.
// No ROM is required. SDL readback is a host check, not an Android GPU test.
#include "video_scaler.h"
#if defined(SMA3_BASELINE_VIDEO_SCALER_HEADER)
#define sma3 sma3_baseline
#include SMA3_BASELINE_VIDEO_SCALER_HEADER
#undef sma3
#endif
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

static void require(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s: %s\n", what, SDL_GetError());
        std::exit(1);
    }
}
static uint64_t fingerprint(const std::vector<uint8_t>& bytes) {
    uint64_t value = 14695981039346656037ull;
    for (auto byte : bytes) value = (value ^ byte) * 1099511628211ull;
    return value;
}
int main() {
    require(SDL_Init(0) == 0, "SDL init");
    auto* surface = SDL_CreateRGBSurfaceWithFormat(0, 8, 8, 32, SDL_PIXELFORMAT_RGBA32);
    require(surface != nullptr, "surface");
    auto* renderer = SDL_CreateSoftwareRenderer(surface);
    require(renderer != nullptr, "software renderer");
    sma3::VideoScaler scaler;
#if defined(SMA3_BASELINE_VIDEO_SCALER_HEADER)
    sma3_baseline::VideoScaler baseline_scaler;
#endif
    unsigned count = 0;
    for (int sw : {240, 347, 356, 240}) {
        auto* source = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24,
                                        SDL_TEXTUREACCESS_STREAMING, sw, 160);
        require(source != nullptr, "source texture");
        std::vector<uint8_t> pixels(size_t(sw) * 160 * 3);
        for (const auto dimensions : {SDL_Point{2400,1080}, SDL_Point{1080,2400},
                 SDL_Point{1921,1081}, SDL_Point{1081,1921},
                 SDL_Point{321,217}, SDL_Point{3,2}}) {
            auto* target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                SDL_TEXTUREACCESS_TARGET, dimensions.x, dimensions.y);
            require(target != nullptr, "resized target");
            require(SDL_SetRenderTarget(renderer, target) == 0, "target selection");
            std::vector<uint8_t> result(size_t(dimensions.x) * dimensions.y * 4);
            for (int quality : {0,720,1080,0}) for (bool stretch : {false,true}) {
                const auto destination = sma3::video_destination(
                    dimensions.x, dimensions.y, sw, 160, stretch);
                uint64_t first = 0;
                for (int phase : {0,1,0}) {
                    for (int y=0; y<160; ++y) for (int x=0; x<sw; ++x) {
                        const size_t at = (size_t(y)*sw+x)*3;
                        const bool edge = ((x+y+phase*5)/7)%2;
                        pixels[at] = uint8_t(edge ? 241 : 17);
                        pixels[at+1] = uint8_t(((x/3+y/5+phase)%3)*73+16);
                        pixels[at+2] = uint8_t(((x/11-y/9+phase*2)&3)*61+32);
                    }
                    require(SDL_UpdateTexture(source,nullptr,pixels.data(),sw*3)==0, "upload");
                    require(SDL_SetRenderDrawColor(renderer,0,0,0,255)==0 &&
                            SDL_RenderClear(renderer)==0, "clear");
                    require(scaler.draw(renderer,source,sw,160,destination,quality), "draw");
                    require(SDL_GetRenderTarget(renderer)==target, "target restoration");
                    require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_RGBA32,
                            result.data(),dimensions.x*4)==0, "readback");
#if defined(SMA3_BASELINE_VIDEO_SCALER_HEADER)
                    require(SDL_SetRenderDrawColor(renderer,0,0,0,255)==0 &&
                            SDL_RenderClear(renderer)==0,"clear reference output");
                    require(baseline_scaler.draw(renderer,source,sw,160,destination,quality),
                            "historical scaler draw");
                    std::vector<uint8_t> reference(result.size());
                    require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_RGBA32,
                            reference.data(),dimensions.x*4)==0,"historical readback");
                    require(reference==result,"every default output pixel byte matches 0.10.2");
#endif
                    const auto value=fingerprint(result);
                    if (phase==0 && first) require(value==first,"no stale pixels after alternating frame");
                    if (phase==0) first=value;
                    std::printf("%u,%d,%d,%d,%d,%d,%d,%016llx\n", count++, sw,
                        dimensions.x,dimensions.y,quality,int(stretch),phase,
                        static_cast<unsigned long long>(value));
                }
            }
            require(SDL_SetRenderTarget(renderer,nullptr)==0,"release target");
            SDL_DestroyTexture(target);
        }
        SDL_DestroyTexture(source);
    }
    scaler.reset();
#if defined(SMA3_BASELINE_VIDEO_SCALER_HEADER)
    baseline_scaler.reset();
#endif
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
    SDL_Quit();
    std::fprintf(stderr,"PASS default fingerprints=%u\n",count);
}
