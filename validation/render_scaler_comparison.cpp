// Pair every output byte against the historical scaler and count render passes.
// SDL software timing is a host component benchmark, not Android GPU performance.
#include <SDL.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

struct Calls { unsigned copies=0, targets=0, creates=0; } calls;
static int counted_copy(SDL_Renderer* r,SDL_Texture* t,const SDL_Rect* s,const SDL_Rect* d) {
    ++calls.copies; return SDL_RenderCopy(r,t,s,d);
}
static int counted_target(SDL_Renderer* r,SDL_Texture* t) {
    ++calls.targets; return SDL_SetRenderTarget(r,t);
}
static SDL_Texture* counted_create(SDL_Renderer* r,Uint32 f,int a,int w,int h) {
    ++calls.creates; return SDL_CreateTexture(r,f,a,w,h);
}
#define SDL_RenderCopy counted_copy
#define SDL_SetRenderTarget counted_target
#define SDL_CreateTexture counted_create
#define sma3 sma3_before
#include SMA3_BASELINE_SCALER_HEADER
#undef sma3
#include "video_scaler.h"
#undef SDL_RenderCopy
#undef SDL_SetRenderTarget
#undef SDL_CreateTexture

static unsigned long long comparisons=0;
static void require(bool value,const char* name) {
    if(!value) { std::fprintf(stderr,"FAIL %s %s\n",name,SDL_GetError());std::exit(1); }
}
static double thread_ms() {
    timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);
    return t.tv_sec*1000.0+t.tv_nsec/1000000.0;
}
struct Harness {
    SDL_Surface* surface;
    SDL_Renderer* renderer;
    SDL_Texture* source;
    sma3_before::VideoScaler before;
    sma3::VideoScaler after;
    int sw,w,h;
    SDL_Rect destination;
    std::vector<std::uint8_t> pixels,a,b;
    Harness(int width,int dw,int dh):sw(width),w(dw+14),h(dh+18),destination{7,9,dw,dh},
        pixels(std::size_t(sw)*160*3),a(std::size_t(w)*h*4),b(a.size()) {
        surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGBA32);
        require(surface!=nullptr,"surface"); renderer=SDL_CreateSoftwareRenderer(surface);
        require(renderer!=nullptr,"renderer");
        source=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STREAMING,sw,160);
        require(source!=nullptr,"source");require(SDL_SetTextureBlendMode(source,SDL_BLENDMODE_NONE)==0,"opaque");
    }
    ~Harness(){before.reset();after.reset();SDL_DestroyTexture(source);SDL_DestroyRenderer(renderer);SDL_FreeSurface(surface);}
    void pattern(int phase) {
        for(int y=0;y<160;++y) for(int x=0;x<sw;++x) {
            const auto at=(std::size_t(y)*sw+x)*3;
            pixels[at]=std::uint8_t((x/3+y/7+phase)*31);
            pixels[at+1]=std::uint8_t((x/11+y/2+phase)*47);
            pixels[at+2]=std::uint8_t((x+y/8+phase*9)*23);
        }
        require(SDL_UpdateTexture(source,nullptr,pixels.data(),sw*3)==0,"upload");
    }
    Calls render(bool candidate,int filter,int quality,bool read) {
        require(SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE)==0 &&
            SDL_SetRenderDrawColor(renderer,7,11,20,255)==0 && SDL_RenderClear(renderer)==0,"clear");
        calls={};
        if(candidate) require(after.draw(renderer,source,sw,160,destination,quality,
            static_cast<sma3::ImageFilter>(filter),pixels.data()),"candidate draw");
        else require(before.draw(renderer,source,sw,160,destination,quality,
            static_cast<sma3_before::ImageFilter>(filter),pixels.data()),"baseline draw");
        require(SDL_GetRenderTarget(renderer)==nullptr,"target restored");
        require(SDL_RenderFlush(renderer)==0,"flush");
        const Calls counted=calls;
        if(read) require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_RGBA32,
            candidate?b.data():a.data(),w*4)==0,"readback");
        return counted;
    }
};

int main(int argc,char** argv) {
    const bool benchmark_only=argc>1 && std::string(argv[1])=="--benchmark-only";
    const bool validate_only=argc>1 && std::string(argv[1])=="--validate-only";
    require(SDL_Init(0)==0,"init");
    unsigned checks=0,bypassed=0;
    if(!benchmark_only) {
    for(int sw:{240,356}) for(int factor:{1,2,3,6,9}) {
        Harness h(sw,sw*factor,160*factor);
        for(int quality:{0,720,1080}) for(int filter=0;filter<=6;++filter) for(int phase:{0,1,0}) {
            h.pattern(phase); const Calls old=h.render(false,filter,quality,true);
            const Calls fresh=h.render(true,filter,quality,true);
            require(h.a==h.b,"all output bytes identical");
            const int cpu_factor=filter==3?2:filter==4?3:1;
            const bool eligible=quality==0 && filter!=1 && filter!=2 && factor%cpu_factor==0;
            if(eligible) {
                require(old.copies==2 && fresh.copies==1 && old.targets==2 && fresh.targets==0,"one pass exact integer scaling");
                ++bypassed;
            } else require(old.copies==fresh.copies,"noneligible render path preserved");
            comparisons+=h.a.size(); ++checks;
        }
    }
    for(const auto dimensions:{std::array<int,3>{240,1620,1080},std::array<int,3>{356,2400,1078},
                              std::array<int,3>{240,1080,720},std::array<int,3>{356,1,1}}) {
        Harness h(dimensions[0],dimensions[1],dimensions[2]);h.pattern(0);
        for(int quality:{0,720,1080}) for(int filter=0;filter<=6;++filter) {
            h.render(false,filter,quality,true);h.render(true,filter,quality,true);
            require(h.a==h.b,"fractional output unchanged");comparisons+=h.a.size();++checks;
        }
    }
    std::printf("{\"result\":\"PASS\",\"paired_presentations\":%u,\"eligible_presentations\":%u,\"compared_output_bytes\":%llu}\n",checks,bypassed,comparisons);
    }
    if(!validate_only) for(const auto dimensions:{std::array<int,3>{240,2160,1440},std::array<int,3>{240,1440,960},std::array<int,3>{240,1620,1080}}) {
        Harness h(dimensions[0],dimensions[1],dimensions[2]);h.pattern(0);
        for(int filter:{0,3,4}) {
            std::array<double,7> before{},after{};
            const auto measure=[&](bool candidate) {
                const double start=thread_ms();
                for(int i=0;i<12;++i) h.render(candidate,filter,0,false);
                return (thread_ms()-start)/12;
            };
            h.render(false,filter,0,false);h.render(true,filter,0,false);
            for(int trial=0;trial<7;++trial) {
                if(trial%2) {after[trial]=measure(true);before[trial]=measure(false);}
                else {before[trial]=measure(false);after[trial]=measure(true);}
            }
            std::sort(before.begin(),before.end());std::sort(after.begin(),after.end());
            std::printf("{\"kind\":\"SDL_software\",\"width\":%d,\"height\":%d,\"filter\":%d,\"before_ms\":%.6f,\"after_ms\":%.6f,\"ratio\":%.4f}\n",dimensions[1],dimensions[2],filter,before[3],after[3],after[3]/before[3]);
        }
    }
    SDL_Quit();
}
