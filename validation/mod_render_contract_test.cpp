// ROM-free composition test of production FrameBlend, color matrix and scaler.
// The SDL software renderer is deterministic; this does not qualify Android GPU
// drivers, device performance or the Android document picker.
#include "frame_interpolation.h"
#include "mod_color_profile.h"
#include "video_scaler.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
std::uint64_t checks=0, presentations=0, pixel_checks=0;
void require(bool ok,const char* message) {
    ++checks;
    if (!ok) { std::fprintf(stderr,"FAIL %s: %s\n",message,SDL_GetError()); std::exit(1); }
}
std::vector<std::uint8_t> pattern(int width,int phase) {
    std::vector<std::uint8_t> data(std::size_t(width)*160*3);
    for (int y=0;y<160;++y) for(int x=0;x<width;++x) {
        const auto p=(std::size_t(y)*width+x)*3;
        data[p]=std::uint8_t(30+(x%23)*8+phase*3);
        data[p+1]=std::uint8_t(20+(y%19)*10+phase*2);
        data[p+2]=std::uint8_t(20+((x/3+y/4)%17)*11+phase);
    }
    return data;
}
std::vector<std::uint8_t> reference(const std::vector<std::uint8_t>& source) {
    auto result=source;
    // The installed sample's documented diagonal matrix. This reference does
    // not call ColorProfile::map, preserving an independent numeric oracle.
    for(std::size_t i=0;i<result.size();i+=3) {
        result[i]=static_cast<std::uint8_t>(std::min(255,(int(source[i])*108+50)/100));
        result[i+2]=static_cast<std::uint8_t>((int(source[i+2])*88+50)/100);
    }
    return result;
}
struct Surface {
    SDL_Surface* surface=nullptr;
    SDL_Renderer* renderer=nullptr;
    SDL_Texture* texture=nullptr;
    sma3::VideoScaler scaler;
    int width,height,source_width;
    SDL_Rect destination;
    Surface(int w,int h,int sw,bool stretch):width(w),height(h),source_width(sw),
        destination(sma3::video_destination(w,h,sw,160,stretch)) {
        surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGBA32);
        require(surface!=nullptr,"create surface");
        renderer=SDL_CreateSoftwareRenderer(surface);
        require(renderer!=nullptr,"create software renderer");
        texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB24,
                                  SDL_TEXTUREACCESS_STREAMING,sw,160);
        require(texture!=nullptr,"create source texture");
        require(SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_NONE)==0,"source opaque");
    }
    ~Surface() {
        scaler.reset(); SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer); SDL_FreeSurface(surface);
    }
    std::vector<std::uint8_t> render(const std::uint8_t* rgb,int filter,int quality) {
        require(SDL_UpdateTexture(texture,nullptr,rgb,source_width*3)==0,"upload present");
        require(SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE)==0 &&
                SDL_SetRenderDrawColor(renderer,7,11,20,255)==0 &&
                SDL_RenderClear(renderer)==0,"clear");
        require(SDL_SetRenderDrawColor(renderer,49,127,211,255)==0,"overlay color");
        require(scaler.draw(renderer,texture,source_width,160,destination,quality,
                    static_cast<sma3::ImageFilter>(filter),rgb),"composed scaler");
        Uint8 r=0,g=0,b=0,a=0; SDL_BlendMode blend=SDL_BLENDMODE_BLEND;
        require(SDL_GetRenderDrawColor(renderer,&r,&g,&b,&a)==0 &&
                r==49 && g==127 && b==211 && a==255,"draw color preserved");
        require(SDL_GetRenderDrawBlendMode(renderer,&blend)==0 && blend==SDL_BLENDMODE_NONE,
                "draw blend preserved");
        // A subsequent touch/control marker on a top band stays visible and
        // has the exact overlay color, unaffected by the game's color profile.
        const SDL_Rect overlay{destination.x,destination.y,5,3};
        require(SDL_RenderFillRect(renderer,&overlay)==0,"overlay after filters");
        std::vector<std::uint8_t> output(std::size_t(width)*height*4);
        require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_RGBA32,
                    output.data(),width*4)==0,"read presentation");
        const auto at=(std::size_t(destination.y)*width+destination.x)*4;
        require(output[at]==49 && output[at+1]==127 && output[at+2]==211,
                "controls remain visible and ungraded");
        ++presentations;
        return output;
    }
    void verify_bars(const std::vector<std::uint8_t>& balanced,
                     const std::vector<std::uint8_t>& cinematic) {
        const auto& d=destination;
        for(int y=0;y<height;++y) for(int x=0;x<width;++x) {
            const auto at=(std::size_t(y)*width+x)*4;
            const bool bar=x>=d.x && x<d.x+d.w && y>=d.y && y<d.y+d.h &&
                (y<d.y+d.h/10 || y>=d.y+d.h-d.h/10);
            const bool overlay=x>=d.x && x<d.x+5 && y>=d.y && y<d.y+3;
            if(bar && !overlay)
                require(cinematic[at]==0 && cinematic[at+1]==0 && cinematic[at+2]==0 &&
                        cinematic[at+3]==255,"profile cannot tint cinematic bands");
            else require(std::equal(balanced.begin()+at,balanced.begin()+at+4,
                                    cinematic.begin()+at),"same center, margins and controls");
            ++pixel_checks;
        }
    }
};
}

int main() {
    require(SDL_Init(SDL_INIT_VIDEO)==0,"initialize SDL");
    auto warm=std::make_shared<sma3::mods::ColorProfile>();
    warm->matrix={108,0,0,0,100,0,0,0,88};
    for(int source_width:{240,356}) {
        auto previous=pattern(source_width,0),current=pattern(source_width,1);
        const auto unchanged=current;
        sma3::FrameBlend blend;
        require(!blend.prepare(previous.data(),previous.size(),100),"first frame has no midpoint");
        require(blend.prepare(current.data(),current.size(),101),"adjacent frame produces midpoint");
        std::vector<std::uint8_t> middle(blend.middle(),blend.middle()+current.size());
        for(std::size_t i=0;i<middle.size();++i)
            require(middle[i]==(unsigned(previous[i])+current[i]+1)/2,"true RGB midpoint");
        for(const auto& input:{current,middle}) {
            std::vector<std::uint8_t> scratch;
            sma3::mods::publish(warm);
            const auto* transformed=sma3::mods::apply(input.data(),source_width,160,scratch);
            const auto expected=reference(input);
            require(std::equal(expected.begin(),expected.end(),transformed),"profile maps exact current/midpoint RGB");
            for(const auto extent:{SDL_Point{643,401},SDL_Point{321,679}})
              for(bool stretch:{false,true}) {
                Surface surface(extent.x,extent.y,source_width,stretch);
                for(int quality:{0,720}) {
                    std::vector<std::uint8_t> balanced,cinematic;
                    for(int filter=0;filter<=6;++filter) {
                        sma3::mods::publish(nullptr);
                        const auto baseline=surface.render(sma3::mods::apply(input.data(),source_width,160,scratch),filter,quality);
                        sma3::mods::publish(warm);
                        const auto modified=surface.render(sma3::mods::apply(input.data(),source_width,160,scratch),filter,quality);
                        const auto oracle=surface.render(expected.data(),filter,quality);
                        require(modified==oracle,"matrix precedes every filter");
                        require(modified!=baseline,"enabled profile changes visible game pixels");
                        if(filter==0) balanced=modified;
                        if(filter==6) cinematic=modified;
                        sma3::mods::publish(nullptr);
                        require(surface.render(sma3::mods::apply(input.data(),source_width,160,scratch),filter,quality)==baseline,
                                "disable restores exact presentation without stale texture");
                    }
                    surface.verify_bars(balanced,cinematic);
                }
            }
        }
        require(current==unchanged,"guest framebuffer was not modified");
    }
    sma3::mods::publish(nullptr);
    SDL_Quit();
    std::printf("PASS mod composition presentations=%llu bar_pixel_checks=%llu assertions=%llu\n",
        static_cast<unsigned long long>(presentations),static_cast<unsigned long long>(pixel_checks),
        static_cast<unsigned long long>(checks));
}
