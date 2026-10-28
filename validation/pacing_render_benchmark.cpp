// Host SDL software renderer, Android presentation branches; not Android EGL.
// Build with __ANDROID__, GBARECOMP_HAVE_SDL2, SMA3_FRAME_INTERPOLATION and
// SMA3_MOD_PROFILES. Override SOURCE/HEADER to compare archived engine sources.
// BENCH_VERSION=4 executes the original 0.10.4 presentation order; 5/6 use
// serial PresentationPacing, 7 uses cooperative callbacks. Guest work is sleep,
// not ROM execution. No throughput number from this harness is Android FPS.
#ifndef BENCH_VERSION
#define BENCH_VERSION 7
#endif
#ifndef SMA3_HOST_WINDOW_HEADER
#define SMA3_HOST_WINDOW_HEADER "../../gbarecomp/src/runtime/host_window.h"
#endif
#ifndef SMA3_HOST_WINDOW_SOURCE
#define SMA3_HOST_WINDOW_SOURCE "../../gbarecomp/src/runtime/host_window.cpp"
#endif
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <thread>

static std::uint64_t ns_now() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
static std::uint64_t cpu_ns() {
    timespec t{}; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return std::uint64_t(t.tv_sec) * 1000000000ULL + t.tv_nsec;
}
static std::uint64_t flush_ns, present_ns, upload_ns;
static unsigned flush_calls, present_calls, upload_calls;
static int bench_flush(SDL_Renderer* renderer) {
    auto t=cpu_ns(); ++flush_calls; int result=SDL_RenderFlush(renderer);
    flush_ns+=cpu_ns()-t; return result;
}
static void bench_present(SDL_Renderer* renderer) {
    auto t=cpu_ns(); ++present_calls; SDL_RenderPresent(renderer);
    present_ns+=cpu_ns()-t;
}
static int bench_upload(SDL_Texture* texture,const SDL_Rect* rect,const void* pixels,int pitch) {
    auto t=cpu_ns(); ++upload_calls; int result=SDL_UpdateTexture(texture,rect,pixels,pitch);
    upload_ns+=cpu_ns()-t; return result;
}
#define private public
#include SMA3_HOST_WINDOW_HEADER
#undef private
#define SDL_RenderFlush bench_flush
#define SDL_RenderPresent bench_present
#define SDL_UpdateTexture bench_upload
#include SMA3_HOST_WINDOW_SOURCE
#undef SDL_RenderFlush
#undef SDL_RenderPresent
#undef SDL_UpdateTexture
#include "host_platform.h"
#include "presentation_pacing.h"

int main(int argc, char** argv) {
    const int width=argc>1?std::atoi(argv[1]):2400;
    const int height=argc>2?std::atoi(argv[2]):1080;
    const bool controls=argc>3?std::atoi(argv[3]):true;
    const int filter=argc>4?std::atoi(argv[4]):1;
    const int target=argc>5?std::atoi(argv[5]):0;
    const int guest_us=argc>6?std::atoi(argv[6]):0;
    const int poll_us=argc>7?std::max(1,std::atoi(argv[7])):1000;
    SDL_SetHint(SDL_HINT_VIDEODRIVER,"dummy");
    SDL_SetHint(SDL_HINT_RENDER_BATCHING,"1");
    if(SDL_Init(SDL_INIT_VIDEO)!=0){std::fprintf(stderr,"%s\n",SDL_GetError());return 1;}
    using namespace gbarecomp;
    auto* b=new Backend;
    b->window=SDL_CreateWindow("pacing-benchmark",0,0,width,height,SDL_WINDOW_SHOWN);
    b->renderer=SDL_CreateRenderer(b->window,-1,SDL_RENDERER_SOFTWARE);
    b->texture=SDL_CreateTexture(b->renderer,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STREAMING,240,160);
    if(!b->window||!b->renderer||!b->texture){std::fprintf(stderr,"%s\n",SDL_GetError());return 2;}
    sma3::image_filter.store(sma3::validated_image_filter(filter));
    b->base_w=240;b->base_h=160;b->px_per_mm=16;b->logical_px_per_mm=16;
    b->last_drawable_w=width;b->last_drawable_h=height;
    b->last_layout=compute_adaptive_presentation_layout(width,height,240,160);
    b->touch_controls=controls;b->pad_visible=controls;
    HostWindow window;window.open_=true;window.impl_=b;
    FramePacer pacer; pacer.set_uncapped(target==0);
    sma3::FrameBlend legacy_blend;
    sma3::PresentationPacing scheduling;
    std::uint64_t guest_frame=0;
    std::vector<std::uint8_t> pixels(240*160*3);
    for(std::size_t i=0;i<pixels.size();++i) pixels[i]=std::uint8_t(i*17);
    auto draw=[&]{
        ++guest_frame;
        auto guest_remaining=std::chrono::nanoseconds(std::chrono::microseconds(guest_us));
        while(guest_remaining.count()>0){
            const auto began=std::chrono::steady_clock::now();
            std::this_thread::sleep_for(std::min(guest_remaining,
                std::chrono::nanoseconds(std::chrono::microseconds(poll_us))));
            guest_remaining-=std::chrono::steady_clock::now()-began;
#if BENCH_VERSION >= 7
            scheduling.service(&pacer,target==120,true,0,
                [&](const std::uint8_t* data,FramePacer* clock,bool optional){
                    return window.present_paced(data,clock,optional);
                });
#endif
        }
#if BENCH_VERSION == 4
        if(target==120){
            const bool middle=legacy_blend.prepare(pixels.data(),pixels.size(),guest_frame);
            if(pacer.wait_for_midpoint())window.present(middle?legacy_blend.middle():pixels.data());
            pacer.wait_for_next_frame();
        }
        window.present(pixels.data());
        if(target==60)pacer.wait_for_next_frame();
#else
#if BENCH_VERSION >= 7
        scheduling.service(&pacer,target==120,true,0,
            [&](const std::uint8_t* data,FramePacer* clock,bool optional){
                return window.present_paced(data,clock,optional);
            },true);
#endif
        if(target==0)window.present_paced(pixels.data(),&pacer,false);
        else scheduling.present(&pacer,pixels.data(),pixels.size(),guest_frame,target==120,true,
            std::chrono::steady_clock::now(),0,[&](const std::uint8_t* data,FramePacer* clock,bool middle){
                return window.present_paced(data,clock,middle);
            });
#endif
    };
    for(int i=0;i<30;++i) draw();
    flush_ns=present_ns=upload_ns=0;flush_calls=present_calls=upload_calls=0;
    std::vector<double> times;
    constexpr unsigned frames=120;
    auto wall_start=ns_now(),cpu_start=cpu_ns();
    for(unsigned i=0;i<frames;++i){auto t=cpu_ns();draw();times.push_back((cpu_ns()-t)/1e6);}
    auto cpu_total=cpu_ns()-cpu_start,wall_total=ns_now()-wall_start;
    std::sort(times.begin(),times.end());
    std::printf("{\"version\":%d,\"width\":%d,\"height\":%d,\"controls\":%s,\"filter\":%d,\"target\":%d,\"synthetic_guest_sleep_us\":%d,\"synthetic_poll_us\":%d,\"frames\":%u,\"guest_rate\":%.6f,\"present_rate\":%.6f,\"cpu_mean_ms\":%.6f,\"cpu_p50_ms\":%.6f,\"cpu_p95_ms\":%.6f,\"wall_total_ms\":%.6f,\"flush_mean_ms\":%.6f,\"present_mean_ms\":%.6f,\"upload_mean_ms\":%.6f,\"flush_calls\":%u,\"present_calls\":%u,\"upload_calls\":%u}\n",BENCH_VERSION,width,height,controls?"true":"false",filter,target,guest_us,poll_us,frames,frames*1e9/wall_total,present_calls*1e9/wall_total,cpu_total/1e6/frames,times[frames/2],times[frames*95/100],wall_total/1e6,flush_ns/1e6/frames,present_ns/1e6/frames,upload_ns/1e6/frames,flush_calls,present_calls,upload_calls);
    window.close();SDL_Quit();
    return 0;
}
