// Exercises the actual Android presentation path with SDL software. Build with
// GBARECOMP_HAVE_SDL2, SMA3_FRAME_INTERPOLATION, SMA3_MOD_PROFILES and
// SMA3_PRESENTATION_DIAGNOSTICS; link host_platform, touch_input and color_lut.
// No ROM, Android APIs, or performance claims are involved.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

static int source_width, injected_failure;
static unsigned native_uploads, filtered_uploads, failures, checks;
static void (*during_clear)();
static std::vector<unsigned char> displayed;
static void require(bool ok,const char* text) {
    ++checks;
    if(!ok){std::fprintf(stderr,"FAIL %s: %s\n",text,SDL_GetError());std::exit(1);}
}
static int checked_upload(SDL_Texture* texture,const SDL_Rect* rect,const void* data,int pitch) {
    int width=0,height=0;
    require(SDL_QueryTexture(texture,nullptr,nullptr,&width,&height)==0,"query upload size");
    const bool native=width==source_width&&height==160;
    if(native)++native_uploads;else ++filtered_uploads;
    if(!native&&injected_failure==1){injected_failure=0;++failures;return SDL_SetError("injected filtered upload failure");}
    return SDL_UpdateTexture(texture,rect,data,pitch);
}
static SDL_Texture* checked_create(SDL_Renderer* renderer,Uint32 format,int access,int width,int height) {
    if(access==SDL_TEXTUREACCESS_TARGET&&injected_failure==2){
        injected_failure=0;++failures;SDL_SetError("injected target allocation failure");return nullptr;
    }
    return SDL_CreateTexture(renderer,format,access,width,height);
}
static int checked_target(SDL_Renderer* renderer,SDL_Texture* texture) {
    if(texture&&injected_failure==3){injected_failure=0;++failures;return SDL_SetError("injected target binding failure");}
    return SDL_SetRenderTarget(renderer,texture);
}
static int checked_clear(SDL_Renderer* renderer) {
    if(during_clear){auto callback=during_clear;during_clear=nullptr;callback();}
    return SDL_RenderClear(renderer);
}
static void checked_present(SDL_Renderer* renderer) {
    int width=0,height=0;
    require(SDL_GetRendererOutputSize(renderer,&width,&height)==0,"read display size");
    displayed.resize(std::size_t(width)*height*4);
    require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_RGBA32,displayed.data(),width*4)==0,
            "read pixels before swap");
    SDL_RenderPresent(renderer);
}
#define private public
#include "../../gbarecomp/src/runtime/host_window.h"
#undef private
#define SDL_UpdateTexture checked_upload
#define SDL_CreateTexture checked_create
#define SDL_SetRenderTarget checked_target
#define SDL_RenderClear checked_clear
#define SDL_RenderPresent checked_present
#include "../../gbarecomp/src/runtime/host_window.cpp"
#undef SDL_UpdateTexture
#undef SDL_CreateTexture
#undef SDL_SetRenderTarget
#undef SDL_RenderClear
#undef SDL_RenderPresent

namespace {
using namespace gbarecomp;
struct Fixture {
    HostWindow window;
    Backend* backend;
    std::vector<unsigned char> pixels;
    explicit Fixture(int width) {
        source_width=width;backend=new Backend;
        backend->window=SDL_CreateWindow("presentation upload test",0,0,961,541,0);
        backend->renderer=SDL_CreateRenderer(backend->window,-1,SDL_RENDERER_SOFTWARE);
        backend->texture=SDL_CreateTexture(backend->renderer,SDL_PIXELFORMAT_RGB24,
            SDL_TEXTUREACCESS_STREAMING,width,160);
        require(backend->window&&backend->renderer&&backend->texture,"SDL fixture");
        backend->base_w=width;backend->base_h=160;
        backend->last_drawable_w=961;backend->last_drawable_h=541;
        backend->last_layout=compute_adaptive_presentation_layout(961,541,width,160);
        window.open_=true;window.impl_=backend;
        pixels.resize(std::size_t(width)*160*3);
    }
    ~Fixture(){window.close();}
    void pattern(unsigned phase) {
        for(std::size_t i=0;i<pixels.size();++i)
            pixels[i]=static_cast<unsigned char>((i/9*31+i%13*17+phase*83)%256);
    }
    std::vector<unsigned char> draw(sma3::ImageFilter filter,int fault=0) {
        sma3::image_filter.store(filter);sma3::video_quality.store(0);
        sma3::image_mode.store(sma3::ImageMode::Original);
        if(fault==2)backend->video_scaler.reset();
        injected_failure=fault;native_uploads=filtered_uploads=failures=0;
        const auto before=pixels;
        require(window.present_paced(pixels.data(),nullptr,false),"present completed");
        require(pixels==before,"presentation cannot alter the guest framebuffer");
        require(failures==unsigned(fault!=0),"requested failure was exercised exactly once");
        require(SDL_GetRenderTarget(backend->renderer)==nullptr,"screen target restored");
        return displayed;
    }
};
}

int main() {
    SDL_SetHint(SDL_HINT_VIDEODRIVER,"dummy");SDL_SetHint(SDL_HINT_RENDER_BATCHING,"1");
    require(SDL_Init(SDL_INIT_VIDEO)==0,"SDL init");
    for(int width:{240,356}) {
        Fixture f(width);unsigned phase=0;
        for(int id:{0,1,2,3,4,5,6,4,1,3,2,0}) {
            f.pattern(phase++);f.draw(static_cast<sma3::ImageFilter>(id));
            const bool filtered=id==3||id==4;
            require(native_uploads==unsigned(!filtered),"one native upload only when used");
            require(filtered_uploads==unsigned(filtered),"one filtered upload only when used");
        }
        for(auto filter:{sma3::ImageFilter::Scale2x,sma3::ImageFilter::Scale3x}) {
            for(int fault:{1,2,3}) {
                // Establish the expected fresh raw fallback, then poison the
                // native texture with another frame before the filter fails.
                f.pattern(phase++);
                const auto fresh=f.pixels;
                const auto expected=f.draw(sma3::ImageFilter::Nearest);
                f.pattern(phase++);f.draw(sma3::ImageFilter::Nearest);
                f.pixels=fresh;
                const auto actual=f.draw(filter,fault);
                require(actual==expected,"filter failure displays this frame, never a stale upload");
                require(native_uploads==1,"failed filter uploads the native fallback once");
                f.pattern(phase++);f.draw(filter);
                require(native_uploads==0&&filtered_uploads==1,"filtered path recovers after failure");
            }
            f.pattern(phase++);
            const auto expected=f.draw(filter);
            during_clear=[] {sma3::image_filter.store(sma3::ImageFilter::Nearest);};
            const auto actual=f.draw(filter);
            require(actual==expected,"mid-presentation setting change cannot sample a stale native texture");
            require(native_uploads==0&&filtered_uploads==1,"one filter snapshot covers the whole frame");
        }
        f.pattern(phase++);
        const auto expected=f.draw(sma3::ImageFilter::Scale2x);
        SDL_DestroyTexture(f.backend->texture);f.backend->texture=nullptr;f.backend->video_scaler.reset();
        const auto actual=f.draw(sma3::ImageFilter::Scale2x);
        require(actual==expected,"streaming and target recreation preserve displayed pixels");
        require(native_uploads==0&&filtered_uploads==1,"recreated filtered output skips the unused native upload");
    }
    SDL_Quit();
    std::printf("PASS presentation upload: %u checks; native/wide, filter switches, three injected failures, settings race, texture recreation\n",checks);
}
