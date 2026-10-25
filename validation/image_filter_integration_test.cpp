// Independent presentation invariants for all seven Android image filters.
// No ROM or Android device is needed. Use SDL2's software renderer, optionally
// with ASan/UBSan. Pixel algorithms have a separate specification-oracle test.
#include "video_scaler.h"
#include "frame_interpolation.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks=0, presentations=0;
void require(bool ok, const char* message) {
    ++checks;
    if (!ok) {
        std::fprintf(stderr,"FAIL %s: %s\n",message,SDL_GetError());
        std::exit(1);
    }
}
std::vector<uint8_t> pattern(int width,int phase) {
    std::vector<uint8_t> result(size_t(width)*160*3);
    const int left=(width-240)/2;
    for(int y=0;y<160;++y) for(int x=0;x<width;++x) {
        const size_t at=(size_t(y)*width+x)*3;
        // Model panorama's safe fallback bands without modifying geometry.
        if(width>240 && (x<left || x>=left+240)) continue;
        const bool shape=((x+y)/9)%2;
        result[at]=uint8_t(shape?210+phase*8:35+phase*8);
        result[at+1]=uint8_t(((x/6+y/5)%3)*70+phase*8);
        result[at+2]=uint8_t(((x/13+y/7)%2)*140+30+phase*8);
    }
    return result;
}
struct Draw {
    SDL_Renderer* renderer;
    SDL_Texture* source;
    SDL_Texture* target;
    sma3::VideoScaler& scaler;
    SDL_Point size;
    int width;
    int quality;
    SDL_Rect destination;
    std::vector<uint8_t> run(const uint8_t* rgb,sma3::ImageFilter filter) {
        require(SDL_UpdateTexture(source,nullptr,rgb,width*3)==0,"current raw upload");
        require(SDL_SetRenderDrawColor(renderer,7,11,20,255)==0 &&
                SDL_RenderClear(renderer)==0,"clear margin");
        require(SDL_SetRenderDrawColor(renderer,101,53,199,181)==0 &&
                SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_ADD)==0,"set host draw state");
        require(scaler.draw(renderer,source,width,160,destination,quality,filter,rgb),"selected filter draw");
        require(SDL_GetRenderTarget(renderer)==target,"restore original target");
        Uint8 r,g,b,a;SDL_BlendMode blend;
        require(SDL_GetRenderDrawColor(renderer,&r,&g,&b,&a)==0 &&
                r==101 && g==53 && b==199 && a==181,"preserve host overlay color");
        require(SDL_GetRenderDrawBlendMode(renderer,&blend)==0 && blend==SDL_BLENDMODE_ADD,
                "preserve host overlay blend mode");
        std::vector<uint8_t> output(size_t(size.x)*size.y*4);
        require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_RGBA32,
            output.data(),size.x*4)==0,"read displayed output");
        // Filters must never touch letterboxing outside the chosen destination.
        auto background=[&](int x,int y) {
            const size_t at=(size_t(y)*size.x+x)*4;
            require(output[at]==7 && output[at+1]==11 && output[at+2]==20,
                    "preserve external letterbox");
        };
        if(destination.x>0) background(destination.x-1,destination.y+destination.h/2);
        if(destination.y>0) background(destination.x+destination.w/2,destination.y-1);
        // The interior of guarded panoramic margins remains black for every
        // filter; the one-pixel contour boundary can be smoothed deliberately.
        if(width>240 && destination.w>=width && destination.h>=160) {
            for(int x:{destination.x+destination.w/30,
                       destination.x+destination.w-destination.w/30-1}) {
                const size_t at=(size_t(destination.y+destination.h/2)*size.x+x)*4;
                require(output[at]==0 && output[at+1]==0 && output[at+2]==0,
                        "guarded panorama margin remains black");
            }
        }
        ++presentations;
        return output;
    }
};
} // namespace
int main() {
    require(SDL_Init(0)==0,"SDL init");
    auto* surface=SDL_CreateRGBSurfaceWithFormat(0,16,16,32,SDL_PIXELFORMAT_RGBA32);
    require(surface!=nullptr,"surface");
    auto* renderer=SDL_CreateSoftwareRenderer(surface);
    require(renderer!=nullptr,"software renderer");
    sma3::VideoScaler scaler;
    for(int width:{240,347,356,240}) {
        auto* source=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB24,
            SDL_TEXTUREACCESS_STREAMING,width,160);
        require(source!=nullptr,"RGB24 source");
        auto first=pattern(width,0),second=pattern(width,1);
        const auto first_before=first,second_before=second;
        sma3::FrameBlend blend;
        require(!blend.prepare(first.data(),first.size(),40),"no initial blend history");
        require(blend.prepare(second.data(),second.size(),41),"small change creates midpoint");
        std::vector<uint8_t> midpoint(blend.middle(),blend.middle()+first.size());
        const auto midpoint_before=midpoint;
        for(const auto size:{SDL_Point{961,541},SDL_Point{541,961},
                             SDL_Point{120,80},SDL_Point{3,2}}) {
            auto* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,
                SDL_TEXTUREACCESS_TARGET,size.x,size.y);
            require(target!=nullptr,"resized output");
            require(SDL_SetRenderTarget(renderer,target)==0,"set output");
            for(int quality:{0,720,1080}) for(bool stretch:{false,true}) {
                const auto destination=sma3::video_destination(size.x,size.y,width,160,stretch);
                Draw draw{renderer,source,target,scaler,size,width,quality,destination};
                const auto original=draw.run(first.data(),sma3::ImageFilter::Balanced);
                for(int id=0;id<7;++id) {
                    const auto filter=static_cast<sma3::ImageFilter>(id);
                    const auto a=draw.run(first.data(),filter);
                    draw.run(midpoint.data(),filter);
                    const auto again=draw.run(first.data(),filter);
                    require(a==again,"same frame stays exact after interpolated presentation");
                    const auto restored=draw.run(first.data(),sma3::ImageFilter::Balanced);
                    require(original==restored,"switching filter restores exact default output");
                }
                require(draw.run(first.data(),static_cast<sma3::ImageFilter>(-17))==original &&
                        draw.run(first.data(),static_cast<sma3::ImageFilter>(999))==original,
                        "invalid filter IDs fall back to balanced");
                require(first==first_before && second==second_before && midpoint==midpoint_before,
                        "filters cannot modify game or interpolation source bytes");
            }
            require(SDL_SetRenderTarget(renderer,nullptr)==0,"release output");
            SDL_DestroyTexture(target);
        }
        SDL_DestroyTexture(source);
    }
    scaler.reset();
    require(!scaler.filtered_texture && !scaler.integer_texture && !scaler.output_texture &&
            !scaler.filtered_pixels && !scaler.filtered_capacity,"all filter cache released");
    SDL_DestroyRenderer(renderer);SDL_FreeSurface(surface);SDL_Quit();
    std::printf("{\"result\":\"PASS\",\"presentations\":%u,\"checks\":%u,"
                "\"renderer\":\"SDL software\",\"android_device_tested\":false}\n",
                presentations,checks);
}
