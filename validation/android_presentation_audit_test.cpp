// Bounded presentation audit used while investigating Android 0.10 flicker.
// Exercises the real scaler and blend helper using SDL software rendering.
// This does not execute an Android GPU or establish physical-device behavior.
// Build with C++20, -I src, SDL2 headers and SDL2; optional ASan/UBSan.
#include "frame_interpolation.h"
#include "video_scaler.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>

static unsigned checks=0;
static void require(bool value,const char* what) {
    ++checks;
    if(!value) { std::fprintf(stderr,"FAIL: %s (%s)\n",what,SDL_GetError()); std::exit(1); }
}
static uint64_t hash(const std::vector<Uint8>& bytes) {
    uint64_t value=1469598103934665603ull;
    for(auto byte:bytes) value=(value^byte)*1099511628211ull;
    return value;
}
int main() {
    sma3::FrameBlend blend;
    std::vector<Uint8> a(347*160*3,10),b(a.size(),30),wide(356*160*3,30);
    require(!blend.prepare(a.data(),a.size(),100),"first frame has no history");
    require(blend.prepare(b.data(),b.size(),101),"same-width consecutive frame can blend");
    require(std::all_of(blend.middle(),blend.middle()+b.size(),[](Uint8 v){return v==20;}),"all 347-wide midpoint bytes exact");
    require(!blend.prepare(wide.data(),wide.size(),102),"347 to 356 rejects prior image");
    require(blend.prepare(wide.data(),wide.size(),103),"new width blends only after two same-size frames");
    require(!blend.prepare(a.data(),a.size(),104),"356 to 347 rejects prior image");
    require(!blend.prepare(b.data(),b.size(),200),"frame discontinuity rejects history");
    blend.reset();
    require(!blend.prepare(a.data(),a.size(),201),"explicit reset rejects same-size history");
    const unsigned blend_checks=checks;
    require(SDL_Init(0)==0,"SDL init");
    auto* screen=SDL_CreateRGBSurfaceWithFormat(0,3120,1440,32,SDL_PIXELFORMAT_RGBA32);
    require(screen!=nullptr,"output surface");
    auto* renderer=SDL_CreateSoftwareRenderer(screen);
    require(renderer!=nullptr,"software renderer");
    sma3::VideoScaler scaler;
    std::vector<Uint8> readback(3120*1440*4);
    unsigned frames=0;
    for(int width:{347,356,240,347}) {
        auto* source=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STREAMING,width,160);
        require(source!=nullptr,"RGB24 streaming texture");
        std::vector<Uint8> rgb(width*160*3);
        const auto destination=sma3::video_destination(3120,1440,width,160,false);
        for(int quality:{0,720,1080}) {
            uint64_t first=0;
            for(int repeat=0;repeat<3;++repeat) {
                const bool alternate=repeat==1;
                for(int y=0;y<160;++y) for(int x=0;x<width;++x) {
                    const bool left=x<width/2;
                    const bool top=y<80;
                    const size_t at=(size_t(y)*width+x)*3;
                    rgb[at]=Uint8(left^alternate?240:20);
                    rgb[at+1]=Uint8(top^alternate?220:40);
                    rgb[at+2]=Uint8(alternate?180:60);
                }
                require(SDL_UpdateTexture(source,nullptr,rgb.data(),width*3)==0,"packed odd/even RGB24 upload");
                require(SDL_SetRenderDrawColor(renderer,0,0,0,255)==0&&SDL_RenderClear(renderer)==0,"clear output");
                require(scaler.draw(renderer,source,width,160,destination,quality),"actual Android scaler draw");
                require(SDL_GetRenderTarget(renderer)==nullptr,"backbuffer restored");
                require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_RGBA32,readback.data(),3120*4)==0,"read output");
                const auto h=hash(readback);
                if(repeat==0) first=h;
                if(repeat==1) require(h!=first,"updated texture affects output");
                if(repeat==2) require(h==first,"repeated frame exact after different texture contents");
                const size_t sample=(size_t(destination.y+destination.h/4)*3120+destination.x+destination.w/4)*4;
                require(readback[sample]==(alternate?20:240)&&readback[sample+1]==(alternate?40:220)&&readback[sample+2]==(alternate?180:60),"interior sample contains current frame only");
                ++frames;
            }
        }
        SDL_DestroyTexture(source);
    }
    scaler.reset();
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(screen);
    SDL_Quit();
    std::printf("{\"result\":\"PASS\",\"blend_boundary_checks\":%u,\"scaler_presentations\":%u,\"assertions\":%u,\"source_width_sequence\":[347,356,240,347],\"source_height\":160,\"drawable\":[3120,1440],\"qualities\":[0,720,1080],\"renderer\":\"SDL software\",\"android_device_tested\":false}\n",blend_checks,frames,checks);
}
