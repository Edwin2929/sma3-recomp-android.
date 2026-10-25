// Compile against the actual patched host_window.cpp, with section garbage
// collection so unrelated host runtime entry points are not linked.
#ifndef SMA3_HOST_WINDOW_SOURCE
#define SMA3_HOST_WINDOW_SOURCE "../../gbarecomp/src/runtime/host_window.cpp"
#endif
#include SMA3_HOST_WINDOW_SOURCE
#include "touch_appearance.h"
#include <atomic>
#include <cassert>
#include <climits>
#include <filesystem>
#include <iostream>
#include <thread>

namespace {
using namespace gbarecomp;
using sma3::touch::Box;
unsigned checks = 0;
void require(bool condition, const char* description) {
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
}

std::array<float, 31> geometry(const PadLayout& p) {
    return {p.dpad.x,p.dpad.y,p.dpad.r,p.dpad_thick,p.a.x,p.a.y,p.a.r,
        p.b.x,p.b.y,p.b.r,p.l.x,p.l.y,p.l.w,p.l.h,p.r.x,p.r.y,p.r.w,p.r.h,
        p.select.x,p.select.y,p.select.w,p.select.h,p.start.x,p.start.y,p.start.w,
        p.start.h,p.toggle.x,p.toggle.y,p.toggle.w,p.toggle.h,0};
}

struct Fixture {
    SDL_Surface* surface = nullptr;
    Backend backend;
    Fixture(int width, int height, float mm = 3.f) {
        surface=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_ARGB8888);
        require(surface != nullptr,"surface created");
        backend.renderer=SDL_CreateSoftwareRenderer(surface);
        require(backend.renderer != nullptr,"software renderer created");
        backend.last_drawable_w=width;backend.last_drawable_h=height;
        backend.last_layout=compute_adaptive_presentation_layout(width,height,240,160);
        backend.safe_insets={10,12,14,16};backend.px_per_mm=mm;
        backend.base_w=240;backend.base_h=160;
        backend.touch_controls=true;backend.pad_visible=true;
    }
    ~Fixture(){SDL_DestroyRenderer(backend.renderer);SDL_FreeSurface(surface);}
    std::vector<std::uint32_t> draw() {
        SDL_RenderSetLogicalSize(backend.renderer,0,0);
        SDL_RenderSetViewport(backend.renderer,nullptr);
        SDL_SetRenderDrawBlendMode(backend.renderer,SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(backend.renderer,64,72,88,255);
        SDL_RenderClear(backend.renderer);
        SDL_SetRenderDrawBlendMode(backend.renderer,SDL_BLENDMODE_ADD);
        SDL_SetRenderDrawColor(backend.renderer,9,19,29,39);
        render_host_layers(&backend);
        SDL_BlendMode blend{};Uint8 r=0,g=0,b=0,a=0;
        SDL_GetRenderDrawBlendMode(backend.renderer,&blend);
        SDL_GetRenderDrawColor(backend.renderer,&r,&g,&b,&a);
        require(blend==SDL_BLENDMODE_ADD,"overlay restores caller blend mode");
        require(r==9&&g==19&&b==29&&a==39,"overlay restores caller color");
        std::vector<std::uint32_t> result(static_cast<std::size_t>(surface->w)*surface->h);
        require(SDL_RenderReadPixels(backend.renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,
                    result.data(),surface->w*4)==0,"read rendered control pixels");
        return result;
    }
    std::array<int,3> rgb(const std::vector<std::uint32_t>& pixels,int x,int y) {
        Uint8 r,g,b,a;SDL_GetRGBA(pixels[std::size_t(y)*surface->w+x],surface->format,&r,&g,&b,&a);
        return {r,g,b};
    }
};

void validate_state() {
    const auto initial=sma3::get_touch_appearance();
    require(initial.style==sma3::TouchStyle::Symbols&&initial.opacity_percent==75,"default appearance");
    for(int style:{INT_MIN,-1,0,1,2,INT_MAX})for(int opacity:{INT_MIN,-1,0,19,20,75,100,101,INT_MAX}){
        sma3::set_touch_appearance(style,opacity);const auto value=sma3::get_touch_appearance();
        require(value.style==(style==0?sma3::TouchStyle::Classic:sma3::TouchStyle::Symbols),"style validated");
        require(value.opacity_percent==static_cast<unsigned>(std::clamp(opacity,20,100)),"opacity clamped");
        require(sma3::touch_alpha(value)>=51,"visible minimum alpha");
    }
}

void appearance_cases(const std::filesystem::path& preview) {
    sma3::TouchDesign zero{};sma3::set_touch_design(zero,false);
    unsigned renders=0;
    for(auto size:{std::pair{960,540},std::pair{540,960},std::pair{641,401}}){
        Fixture fixture(size.first,size.second);
        const PadLayout before=compute_pad_layout(&fixture.backend);
        const auto original=geometry(before);
        std::vector<std::uint16_t> original_hits;
        for(int y=0;y<size.second;y+=11)for(int x=0;x<size.first;x+=11)
            original_hits.push_back(pad_buttons_at(before,x,y,fixture.backend.px_per_mm));
        std::vector<std::uint32_t> classic, symbols;
        std::array<int,3> previous_rgb{};
        for(int style:{0,1})for(int opacity:{20,75,100}){
            sma3::set_touch_appearance(style,opacity);
            const auto pixels=fixture.draw();++renders;
            const auto after=compute_pad_layout(&fixture.backend);
            require(geometry(after)==original,"appearance leaves layout unchanged");
            std::size_t index=0;
            for(int y=0;y<size.second;y+=11)for(int x=0;x<size.first;x+=11)
                require(pad_buttons_at(after,x,y,fixture.backend.px_per_mm)==original_hits[index++],
                        "appearance leaves hit regions unchanged");
            const int sx=static_cast<int>(after.a.x-after.a.r*.68f);
            const int sy=static_cast<int>(after.a.y+after.a.r*.2f);
            const auto sample=fixture.rgb(pixels,sx,sy);
            if(opacity==100)require(sample==std::array<int,3>{177,49,65},"100 percent backing is opaque");
            if(opacity!=20)require(sample[0]>previous_rgb[0],"increasing opacity visibly increases red backing");
            previous_rgb=sample;
            if(opacity==75){if(style==0)classic=pixels;else symbols=pixels;}
            if(size.first==960&&opacity==75&&!preview.empty()) {
                std::filesystem::create_directories(preview);
                const auto target=preview/(style==0?"classic.bmp":"symbols.bmp");
                require(SDL_SaveBMP(fixture.surface,target.string().c_str())==0,"saved control comparison");
            }
        }
        require(classic!=symbols,"classic and symbol glyphs visibly differ");
        // At every action button, both styles place visible glyphs inside it.
        const std::array<Box,6> boxes{{
            {before.a.x-before.a.r,before.a.y-before.a.r,2*before.a.r,2*before.a.r},
            {before.b.x-before.b.r,before.b.y-before.b.r,2*before.b.r,2*before.b.r},
            {before.l.x,before.l.y,before.l.w,before.l.h},{before.r.x,before.r.y,before.r.w,before.r.h},
            {before.select.x,before.select.y,before.select.w,before.select.h},
            {before.start.x,before.start.y,before.start.w,before.start.h}}};
        for(auto box:boxes)for(const auto* pixels:{&classic,&symbols}){
            unsigned ink=0;
            for(int y=static_cast<int>(box.y+box.h*.2f);y<box.y+box.h*.8f;++y)
                for(int x=static_cast<int>(box.x+box.w*.15f);x<box.x+box.w*.85f;++x){
                    const auto c=fixture.rgb(*pixels,x,y);if(c[0]>140&&c[1]>140&&c[2]>140)++ink;
                }
            require(ink>=2,"fontless action glyph is visible");
        }
        fixture.backend.pad_visible=false;
        // No controls: the baseline pixels remain unchanged and caller state is intact.
        const auto hidden=fixture.draw();++renders;
        const auto clear=SDL_MapRGBA(fixture.surface->format,64,72,88,255);
        require(std::all_of(hidden.begin(),hidden.end(),[&](auto p){return p==clear;}),"hidden pad does not draw");
    }
    std::cout<<"appearance presentations="<<renders<<'\n';
}

void custom_layout_and_pressed() {
    Fixture fixture(960,540);auto original=compute_pad_layout(&fixture.backend);
    auto custom=sma3::get_touch_design(true);
    custom[4]=.65f;custom[5]=.65f;custom[6]=.10f;custom[7]=.18f;
    require(sma3::set_touch_design(custom,true),"custom design installed");
    const auto changed=compute_pad_layout(&fixture.backend);
    require(changed.a.x!=original.a.x,"custom design applied to test");
    const auto snapshot=geometry(changed);
    for(int style:{0,1})for(int opacity:{20,75,100}){
        sma3::set_touch_appearance(style,opacity);
        const auto neutral=fixture.draw();
        auto& contact=fixture.backend.touches[0];contact.active=true;contact.pad=true;contact.buttons=1;
        const auto held=fixture.draw();
        require(neutral!=held,"pressed state remains visibly distinct");
        require(geometry(compute_pad_layout(&fixture.backend))==snapshot,"pressed/style changes preserve custom geometry");
        require((pad_buttons_at(changed,changed.a.x,changed.a.y,fixture.backend.px_per_mm)&1)!=0,
                "custom A still hits native A");
        contact={};
    }
    sma3::set_touch_design({},false);
}

void caller_render_state() {
    Fixture fixture(960,540);
    require(SDL_RenderSetLogicalSize(fixture.backend.renderer,240,160)==0,"caller logical size installed");
    const SDL_Rect viewport{4,6,201,143};
    require(SDL_RenderSetViewport(fixture.backend.renderer,&viewport)==0,"caller viewport installed");
    SDL_SetRenderDrawBlendMode(fixture.backend.renderer,SDL_BLENDMODE_MOD);
    SDL_SetRenderDrawColor(fixture.backend.renderer,25,50,75,100);
    render_host_layers(&fixture.backend);
    int logical_w=0,logical_h=0;SDL_Rect actual{};
    SDL_RenderGetLogicalSize(fixture.backend.renderer,&logical_w,&logical_h);
    SDL_RenderGetViewport(fixture.backend.renderer,&actual);
    require(logical_w==240&&logical_h==160,"overlay preserves nonzero caller logical size");
    require(actual.x==viewport.x&&actual.y==viewport.y&&actual.w==viewport.w&&actual.h==viewport.h,
            "overlay preserves caller viewport");
    SDL_BlendMode mode{};Uint8 r=0,g=0,b=0,a=0;
    SDL_GetRenderDrawBlendMode(fixture.backend.renderer,&mode);
    SDL_GetRenderDrawColor(fixture.backend.renderer,&r,&g,&b,&a);
    require(mode==SDL_BLENDMODE_MOD,"overlay preserves alternate blend mode");
    require(r==25&&g==50&&b==75&&a==100,"overlay preserves alternate color");
}

void concurrent_complete_frames() {
    Fixture fixture(321,241,1.5f);
    sma3::set_touch_appearance(0,20);const auto classic=fixture.draw();
    sma3::set_touch_appearance(1,100);const auto symbols=fixture.draw();
    std::atomic<bool> stop{false};
    std::thread writer([&]{while(!stop.load(std::memory_order_relaxed)){
        sma3::set_touch_appearance(0,20);sma3::set_touch_appearance(1,100);
    }});
    for(unsigned frame=0;frame<100;++frame){
        const auto actual=fixture.draw();
        require(actual==classic||actual==symbols,"complete pad uses one coherent style/opacity snapshot");
    }
    stop.store(true,std::memory_order_relaxed);writer.join();
    sma3::set_touch_appearance(1,75);
}
}

int main(int argc,char** argv){
    require(SDL_Init(0)==0,"SDL initialized");
    validate_state();appearance_cases(argc>1?std::filesystem::path(argv[1]):std::filesystem::path{});
    custom_layout_and_pressed();caller_render_state();concurrent_complete_frames();
    SDL_Quit();std::cout<<"PASS touch appearance assertions="<<checks<<" concurrent_frames=100\n";
}
