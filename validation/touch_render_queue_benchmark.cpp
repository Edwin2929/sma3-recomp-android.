// Linux diagnostic for the real touch renderer, without a game/ROM.
// SDL software window + batching=1. Direct surface reads prove drawing is
// deferred until flush; enqueue CPU and raster CPU are measured separately.
// Usage: binary [frames_per_case=500] [optional_BMP_directory]
// Override HOST_SOURCE to select a prepared engine snapshot for comparisons.
#include <SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <time.h>
#include <vector>

struct DrawCounts { unsigned geometry=0, rect=0, blend=0; };
static DrawCounts calls;
static int counted_geometry(SDL_Renderer* r,SDL_Texture* t,const SDL_Vertex* v,int n,const int* indices,int count){
    ++calls.geometry;return SDL_RenderGeometry(r,t,v,n,indices,count);
}
static int counted_rect(SDL_Renderer* r,const SDL_FRect* rect){++calls.rect;return SDL_RenderFillRectF(r,rect);}
static int counted_blend(SDL_Renderer* r,SDL_BlendMode mode){++calls.blend;return SDL_SetRenderDrawBlendMode(r,mode);}
#define SDL_RenderGeometry counted_geometry
#define SDL_RenderFillRectF counted_rect
#define SDL_SetRenderDrawBlendMode counted_blend
#ifndef HOST_SOURCE
#define HOST_SOURCE "../../gbarecomp/src/runtime/host_window.cpp"
#endif
#include HOST_SOURCE
#undef SDL_RenderGeometry
#undef SDL_RenderFillRectF
#undef SDL_SetRenderDrawBlendMode

using namespace gbarecomp;
using Clock=std::chrono::steady_clock;
static double thread_us(){timespec ts{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&ts);return ts.tv_sec*1e6+ts.tv_nsec/1e3;}
struct Sample {double wall,cpu,flush_cpu;};
struct Fixture {
    SDL_Surface* surface;
    SDL_Window* window;
    Backend b;
    Fixture(int w,int h,float mm){
        window=SDL_CreateWindow("SMA3 overlay queue benchmark",0,0,w,h,0);
        b.renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
        surface=SDL_GetWindowSurface(window);
        if(!surface||!b.renderer){std::fprintf(stderr,"SDL: %s\n",SDL_GetError());std::exit(1);}
        b.last_drawable_w=w;b.last_drawable_h=h;b.px_per_mm=mm;
        b.safe_insets={10,12,14,16};b.base_w=240;b.base_h=160;
        b.last_layout=compute_adaptive_presentation_layout(w,h,240,160);
        b.touch_controls=true;b.pad_visible=true;
    }
    ~Fixture(){SDL_DestroyRenderer(b.renderer);SDL_DestroyWindow(window);}
    void clear(unsigned frame){
        SDL_SetRenderDrawBlendMode(b.renderer,SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(b.renderer,64+(frame%51),72+(frame%71),88+(frame%31),255);
        SDL_RenderClear(b.renderer);SDL_RenderFlush(b.renderer);
    }
    Sample draw(unsigned frame,bool changing){
        b.touches[0]={};
        if(changing&&frame%2){b.touches[0].active=true;b.touches[0].pad=true;b.touches[0].buttons=1;}
        clear(frame);
        calls={};
        const auto start=Clock::now();
        const double cpu_start=thread_us();
        render_host_layers(&b);
        const double cpu_end=thread_us();
        const auto end=Clock::now();
        SDL_RenderFlush(b.renderer);
        const double flush_end=thread_us();
        return {std::chrono::duration<double,std::micro>(end-start).count(),cpu_end-cpu_start,flush_end-cpu_end};
    }
};

int main(int argc,char**argv){
    const int frames=argc>1?std::max(50,std::atoi(argv[1])):500;
    const std::string out=argc>2&&std::string(argv[2])!="-"?argv[2]:"";
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);
    SDL_SetHint(SDL_HINT_RENDER_BATCHING,"1");
    if(SDL_Init(SDL_INIT_VIDEO)!=0){std::fprintf(stderr,"SDL init: %s\n",SDL_GetError());return 1;}
    {
        Fixture check(641,401,3);
        sma3::set_touch_design({},false);sma3::set_touch_appearance(1,75);
        check.clear(0);
        const auto layout=compute_pad_layout(&check.b);
        auto* pixel=reinterpret_cast<Uint32*>(static_cast<Uint8*>(check.surface->pixels)+
            static_cast<int>(layout.a.y)*check.surface->pitch)+static_cast<int>(layout.a.x);
        const Uint32 before=*pixel;
        render_host_layers(&check.b);
        if(*pixel!=before){std::fprintf(stderr,"FAIL: renderer rasterized before explicit flush\n");return 1;}
        SDL_RenderFlush(check.b.renderer);
        if(*pixel==before){std::fprintf(stderr,"FAIL: queued control did not render after flush\n");return 1;}
        std::fprintf(stderr,"PASS SDL window software batching defers actual surface writes until flush\n");
    }
    SDL_version version;SDL_GetVersion(&version);
    std::printf("SDL %u.%u.%u software; frames=%d warmup=30; mode=real queued draw; raster flush timed separately\n",version.major,version.minor,version.patch,frames);
    std::puts("width,height,mm,style,changing,median_us,p95_us,mean_us,cpu_median_us,cpu_p95_us,flush_cpu_median_us,geometry_calls,rect_calls,blend_calls");
    for(auto shape:std::array<std::array<float,3>,4>{{{960,540,3},{2400,1080,16},{1080,2400,16},{3120,1440,20}}}){
        Fixture f(static_cast<int>(shape[0]),static_cast<int>(shape[1]),shape[2]);
        for(int style:{0,1})for(bool changing:{false,true}){
            sma3::set_touch_design({},false);sma3::set_touch_appearance(style,75);
            for(unsigned i=0;i<30;++i)f.draw(i,changing);
            std::vector<double> times,cpu,flush;times.reserve(frames);cpu.reserve(frames);flush.reserve(frames);
            for(int i=0;i<frames;++i){auto s=f.draw(i,changing);times.push_back(s.wall);cpu.push_back(s.cpu);flush.push_back(s.flush_cpu);}
            auto counts=calls;
            const double mean=std::accumulate(times.begin(),times.end(),0.)/times.size();
            std::sort(times.begin(),times.end());
            std::sort(cpu.begin(),cpu.end());std::sort(flush.begin(),flush.end());
            std::printf("%d,%d,%.1f,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u,%u,%u\n",f.surface->w,f.surface->h,shape[2],style,changing,times[times.size()/2],times[times.size()*95/100],mean,cpu[cpu.size()/2],cpu[cpu.size()*95/100],flush[flush.size()/2],counts.geometry,counts.rect,counts.blend);
            if(!out.empty()){
                std::filesystem::create_directories(out);
                const auto path=std::filesystem::path(out)/(std::to_string(f.surface->w)+"x"+std::to_string(f.surface->h)+"-"+std::to_string(style)+"-"+std::to_string(changing)+".bmp");
                if(SDL_SaveBMP(f.surface,path.string().c_str())!=0){std::fprintf(stderr,"Save: %s\n",SDL_GetError());return 1;}
            }
        }
    }
    SDL_Quit();
}
