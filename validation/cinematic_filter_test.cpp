// ROM-free cinematic presentation checks. Compile with SDL2 and optionally
// SMA3_BASELINE_VIDEO_SCALER_HEADER pointing at 0.10.3's video_scaler.h (with
// its image_filters.h beside it) for exact comparisons of existing IDs 0–5.
#include "video_scaler.h"
#if defined(SMA3_BASELINE_VIDEO_SCALER_HEADER)
#define sma3 sma3_cinematic_baseline
#include SMA3_BASELINE_VIDEO_SCALER_HEADER
#undef sma3
#endif
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
std::uint64_t checks=0,pixel_checks=0;
unsigned cinematic_cases=0,unchanged_filter_presentations=0,presentations=0;
void require(bool ok,const char* message) {
    ++checks;
    if(!ok) {
        std::fprintf(stderr,"FAIL %s: %s\n",message,SDL_GetError());
        std::exit(1);
    }
}
std::vector<std::uint8_t> pattern(int width,int phase) {
    std::vector<std::uint8_t> result(std::size_t(width)*160*3);
    for(int y=0;y<160;++y) for(int x=0;x<width;++x) {
        const auto at=(std::size_t(y)*width+x)*3;
        result[at]=std::uint8_t(((x+y+phase*5)%17)*12+20);
        result[at+1]=std::uint8_t(((x/3+y/5+phase)%5)*40+30);
        result[at+2]=std::uint8_t(((x/11+y/7+phase*2)%4)*50+40);
    }
    return result;
}
struct Harness {
    SDL_Renderer* renderer;
    SDL_Texture* source;
    SDL_Texture* target;
    SDL_Point extent;
    int width,quality;
    SDL_Rect destination;
    sma3::VideoScaler& scaler;

    void begin(const std::vector<std::uint8_t>& input) {
        require(SDL_SetRenderTarget(renderer,target)==0,"choose output");
        require(SDL_UpdateTexture(source,nullptr,input.data(),width*3)==0,"upload raw image");
        require(SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE)==0 &&
                SDL_SetRenderDrawColor(renderer,7,11,20,255)==0 &&
                SDL_RenderClear(renderer)==0,"clear output");
        require(SDL_SetRenderDrawColor(renderer,101,53,199,181)==0 &&
                SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_ADD)==0,"set overlay state");
    }
    std::vector<std::uint8_t> finish() {
        require(SDL_GetRenderTarget(renderer)==target,"output target restored");
        Uint8 r=0,g=0,b=0,a=0;SDL_BlendMode blend=SDL_BLENDMODE_NONE;
        require(SDL_GetRenderDrawColor(renderer,&r,&g,&b,&a)==0 &&
                r==101 && g==53 && b==199 && a==181,"drawing color restored");
        require(SDL_GetRenderDrawBlendMode(renderer,&blend)==0 && blend==SDL_BLENDMODE_ADD,
                "drawing blend restored");
        std::vector<std::uint8_t> output(std::size_t(extent.x)*extent.y*4);
        require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_RGBA32,
                    output.data(),extent.x*4)==0,"read output");
        ++presentations;
        return output;
    }
    std::vector<std::uint8_t> draw(const std::vector<std::uint8_t>& input,sma3::ImageFilter filter) {
        begin(input);
        require(scaler.draw(renderer,source,width,160,destination,quality,filter,input.data()),"draw filter");
        return finish();
    }
#if defined(SMA3_BASELINE_VIDEO_SCALER_HEADER)
    std::vector<std::uint8_t> historical(const std::vector<std::uint8_t>& input,int filter,
                                        sma3_cinematic_baseline::VideoScaler& old) {
        begin(input);
        require(old.draw(renderer,source,width,160,destination,quality,
            static_cast<sma3_cinematic_baseline::ImageFilter>(filter),input.data()),"draw previous filter");
        return finish();
    }
#endif
};
void verify_bars(const std::vector<std::uint8_t>& balanced,
                 const std::vector<std::uint8_t>& cinematic,SDL_Point extent,const SDL_Rect& d) {
    const int thickness=d.h/10;
    for(int y=0;y<extent.y;++y) for(int x=0;x<extent.x;++x) {
        const auto at=(std::size_t(y)*extent.x+x)*4;
        const bool bar=x>=d.x && x<d.x+d.w && y>=d.y && y<d.y+d.h &&
            (y<d.y+thickness || y>=d.y+d.h-thickness);
        ++pixel_checks;
        if(bar) require(cinematic[at]==0 && cinematic[at+1]==0 && cinematic[at+2]==0 &&
                        cinematic[at+3]==255,"opaque symmetric bars only");
        else require(std::equal(balanced.begin()+at,balanced.begin()+at+4,cinematic.begin()+at),
                     "visible center and external margins unchanged");
    }
}
void helper_checks(SDL_Renderer* renderer) {
    require(!sma3::VideoScaler::draw_cinematic_bars(nullptr,{0,0,100,100}),"null renderer rejected");
    for(const auto bad:{SDL_Rect{0,0,0,10},SDL_Rect{0,0,10,0},SDL_Rect{0,0,-1,10},
                       SDL_Rect{INT_MAX,0,1,10},SDL_Rect{0,INT_MAX,10,1}})
        require(!sma3::VideoScaler::draw_cinematic_bars(renderer,bad),"invalid bar rectangle rejected");
    auto* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,80,100);
    require(target && SDL_SetRenderTarget(renderer,target)==0,"helper target");
    require(SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE)==0 &&
        SDL_SetRenderDrawColor(renderer,60,100,180,255)==0 && SDL_RenderClear(renderer)==0,"helper clear");
    const SDL_Rect clip{20,13,30,74};
    require(SDL_RenderSetClipRect(renderer,&clip)==0,"set caller clip");
    require(SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_ADD)==0 &&
        SDL_SetRenderDrawColor(renderer,101,53,199,181)==0,"helper overlay state");
    require(sma3::VideoScaler::draw_cinematic_bars(renderer,{10,10,60,80}),"clipped bars");
    SDL_Rect after{};SDL_RenderGetClipRect(renderer,&after);
    require(SDL_RenderIsClipEnabled(renderer) && after.x==clip.x && after.y==clip.y &&
        after.w==clip.w && after.h==clip.h,"caller clip preserved");
    std::vector<std::uint8_t> output(80*100*4);
    require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_RGBA32,output.data(),80*4)==0,"helper read");
    for(int y=0;y<100;++y) for(int x=0;x<80;++x) {
        const bool masked=x>=20 && x<50 && ((y>=13 && y<18)||(y>=82 && y<87));
        const auto at=(y*80+x)*4;
        require(output[at]==(masked?0:60) && output[at+1]==(masked?0:100) &&
            output[at+2]==(masked?0:180) && output[at+3]==255,"bar clipping and opacity");
    }
    require(SDL_RenderSetClipRect(renderer,nullptr)==0 && SDL_SetRenderTarget(renderer,nullptr)==0,
            "release helper target");
    SDL_DestroyTexture(target);
}
}
int main() {
    require(SDL_Init(0)==0,"SDL initialization");
    auto* surface=SDL_CreateRGBSurfaceWithFormat(0,8,8,32,SDL_PIXELFORMAT_RGBA32);
    require(surface!=nullptr,"surface");
    auto* renderer=SDL_CreateSoftwareRenderer(surface);
    require(renderer!=nullptr,"renderer");
    sma3::VideoScaler scaler;
#if defined(SMA3_BASELINE_VIDEO_SCALER_HEADER)
    sma3_cinematic_baseline::VideoScaler old;
#endif
    helper_checks(renderer);
    for(int width:{240,347,356}) {
        auto* source=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STREAMING,width,160);
        require(source!=nullptr,"source texture");
        const auto first=pattern(width,0),second=pattern(width,1),first_copy=first,second_copy=second;
        for(const auto extent:{SDL_Point{961,541},SDL_Point{541,961},SDL_Point{321,217},
                               SDL_Point{3,2},SDL_Point{21,9},SDL_Point{23,10}}) {
            auto* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,extent.x,extent.y);
            require(target!=nullptr,"resized target");
            for(int quality:{0,720,1080}) for(bool stretch:{false,true}) {
                const auto d=sma3::video_destination(extent.x,extent.y,width,160,stretch);
                Harness h{renderer,source,target,extent,width,quality,d,scaler};
                const auto balanced=h.draw(first,sma3::ImageFilter::Balanced);
                const auto cinematic=h.draw(first,sma3::ImageFilter::Cinematic);
                verify_bars(balanced,cinematic,extent,d);
                h.draw(second,sma3::ImageFilter::Cinematic);
                require(h.draw(first,sma3::ImageFilter::Cinematic)==cinematic,"stationary bars and no old frame pixels");
                require(h.draw(first,sma3::ImageFilter::Balanced)==balanced,"disabling bars restores exact full image");
                require(first==first_copy && second==second_copy,"source buffers untouched");
                ++cinematic_cases;
#if defined(SMA3_BASELINE_VIDEO_SCALER_HEADER)
                for(int id=0;id<6;++id) {
                    const auto candidate=h.draw(first,static_cast<sma3::ImageFilter>(id));
                    require(h.historical(first,id,old)==candidate,"existing filter byte-identical to 0.10.3");
                    ++unchanged_filter_presentations;
                }
#endif
            }
            require(SDL_SetRenderTarget(renderer,nullptr)==0,"release output target");
            SDL_DestroyTexture(target);
        }
        SDL_DestroyTexture(source);
    }
    scaler.reset();
#if defined(SMA3_BASELINE_VIDEO_SCALER_HEADER)
    old.reset();
#endif
    SDL_DestroyRenderer(renderer);SDL_FreeSurface(surface);SDL_Quit();
    std::printf("{\"result\":\"PASS\",\"cinematic_cases\":%u,\"old_filter_presentations_exact\":%u,"
        "\"total_presentations\":%u,\"pixel_checks\":%llu,\"checks\":%llu,"
        "\"bar_fraction_each\":0.1,\"renderer\":\"SDL software\",\"android_device_tested\":false}\n",
        cinematic_cases,unchanged_filter_presentations,presentations,
        static_cast<unsigned long long>(pixel_checks),static_cast<unsigned long long>(checks));
}
