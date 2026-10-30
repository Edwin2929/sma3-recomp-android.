// Include the production callback, with its unrelated bus-reading functions
// discarded by --gc-sections. Playback and already-latched context are fixtures;
// all projection, suppression, caching and draw calls are production code.
#include "../src/ghost_visual.cpp"

#include <cstdlib>
#include <iostream>

namespace {
bool playback_enabled=false;
sma3::GhostVisualSample playback_sample;
unsigned checks=0;
void require(bool condition,const char* message) {
    ++checks;if(!condition){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}
}
class Spy final:public gbarecomp::HostOverlay {
public:
    struct Call {float x,y,w,h;gbarecomp::OverlayColor color;gbarecomp::OverlaySpace space;};
    std::array<Call,512> calls{};
    std::size_t count=0;bool overflow=false;
    int width=960,height=640,logical_width=240;
    gbarecomp::PresentationLayout rectangle{0,0,960,640,4};
    int drawable_width()const override{return width;}
    int drawable_height()const override{return height;}
    gbarecomp::PresentationLayout game_rect()const override{return rectangle;}
    int view_width()const override{return logical_width;}
    int view_height()const override{return 160;}
    float drawable_px_per_mm()const override{return 4;}
    Insets safe_insets()const override{return {};}
    std::uint32_t host_ms()const override{return 0;}
    void to_drawable(float x,float y,gbarecomp::OverlaySpace,float* dx,float* dy)const override{*dx=x;*dy=y;}
    void line(float,float,float,float,float,gbarecomp::OverlayColor,gbarecomp::OverlaySpace)override{overflow=true;}
    void polyline(const gbarecomp::OverlayPoint*,std::size_t,float,gbarecomp::OverlayColor,gbarecomp::OverlaySpace)override{overflow=true;}
    void fill_rect(float x,float y,float w,float h,float rounding,gbarecomp::OverlayColor c,gbarecomp::OverlaySpace s)override{
        if(rounding!=0||count==calls.size()){overflow=true;return;}calls[count++]={x,y,w,h,c,s};
    }
    void stroke_rect(float,float,float,float,float,float,gbarecomp::OverlayColor,gbarecomp::OverlaySpace)override{overflow=true;}
    void fill_circle(float,float,float,gbarecomp::OverlayColor,gbarecomp::OverlaySpace)override{overflow=true;}
    void stroke_circle(float,float,float,float,gbarecomp::OverlayColor,gbarecomp::OverlaySpace)override{overflow=true;}
    void arc(float,float,float,float,float,float,gbarecomp::OverlayColor,gbarecomp::OverlaySpace)override{overflow=true;}
    bool text_supported()const override{return false;}
    void text(float,float,float,gbarecomp::OverlayColor,const char*,gbarecomp::OverlayAlign,gbarecomp::OverlaySpace)override{overflow=true;}
    float text_width(const char*,float,gbarecomp::OverlaySpace)const override{return 0;}
    void draw(){count=0;overflow=false;sma3::ghost_draw_overlay(this);require(!overflow,"bounded rectangle primitives only");}
};
void context(bool visible=true,bool pillarbox=false,unsigned room=2) {
    static int source;static std::uint64_t frame=0;
    const sma3::ghost_visual_detail::Scene scene{visible,std::uint16_t(room),100,200};
    sma3::contexts.publish(scene,pillarbox,&source,++frame,1);
    sma3::contexts.publish(scene,pillarbox,&source,++frame,1);
}
}

namespace sma3 {
bool time_trial_ghost_playback(GhostVisualSample& output) noexcept {
    output={};if(!playback_enabled)return false;output=playback_sample;return true;
}
}

int main() {
    Spy spy;context();playback_sample.room=2;playback_sample.visible=true;
    playback_sample.world_x=112;playback_sample.world_y=224;
    auto& im=playback_sample.image;im.width=8;im.height=8;im.palette[1]=0x03e0;
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x)sma3::ghost_visual_detail::set_pixel(im,x,y,1);
    spy.draw();require(spy.count==0,"disabled playback produces zero draw calls");
    playback_enabled=true;spy.draw();require(spy.count==1,"solid recorded sprite renders once");
    require(spy.calls[0].x==48&&spy.calls[0].y==96&&spy.calls[0].w==32&&spy.calls[0].h==32,"callback uses completed camera");
    require(spy.calls[0].space==gbarecomp::OverlaySpace::Drawable&&spy.calls[0].color.a==112&&
        spy.calls[0].color.g==255,"recorded palette and translucent alpha are retained");
    spy.draw();require(spy.count==1&&spy.calls[0].x==48,"second 120-Hz present keeps identical pose");
    sma3::menu_open.store(true);spy.draw();require(spy.count==0,"Android settings suppress ghost");sma3::menu_open.store(false);
    sma3::touch_editing.store(true);spy.draw();require(spy.count==0,"touch editor suppresses ghost");sma3::touch_editing.store(false);
    playback_sample.room=3;spy.draw();require(spy.count==0,"different room suppresses ghost");playback_sample.room=2;
    playback_sample.visible=false;spy.draw();require(spy.count==0,"hidden route pose suppresses ghost");playback_sample.visible=true;
    context(false);spy.draw();require(spy.count==0,"paused/unsupported completed context suppresses ghost");context();
    spy.width=2400;spy.height=1080;spy.rectangle={0,0,2400,1080,0};spy.draw();
    require(spy.count==1&&spy.calls[0].x==120&&spy.calls[0].y==162&&spy.calls[0].w==80&&spy.calls[0].h==54,"Stretch draws with separate scales");
    spy.width=1424;spy.height=640;spy.logical_width=356;spy.rectangle={0,0,1424,640,4};
    playback_sample.world_x=80;spy.draw();require(spy.count==1&&spy.calls[0].x==152,"supported panorama margin draws at world position");
    context(true,true);spy.draw();require(spy.count==0,"unsupported black margin remains clean");context();
    playback_sample.world_x=112;playback_sample.world_y=212;sma3::image_filter.store(sma3::ImageFilter::Cinematic);spy.draw();
    require(spy.count==1&&spy.calls[0].y==64&&spy.calls[0].h==16,"cinematic top boundary clipped");
    playback_sample.world_y=352;spy.draw();require(spy.count==0,"cinematic bottom bar remains clean");
    sma3::image_filter.store(sma3::ImageFilter::Balanced);playback_sample.world_y=224;
    im.width=64;im.height=64;
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x)sma3::ghost_visual_detail::set_pixel(im,x,y,1+((x+y)&1));
    spy.draw();require(spy.count==0,"over-budget image never renders a partial ghost");
    spy.draw();require(spy.count==0,"rejected cached image cannot reuse previous sprite");
    im={};im.width=64;im.height=8;im.palette[1]=0x001f;im.palette[2]=0x7c00;
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<64;++x)sma3::ghost_visual_detail::set_pixel(im,x,y,1+((x+y)&1));
    spy.draw();require(spy.count==512,"hard maximum image fits exactly 512 calls");
    im.width=0;spy.draw();require(spy.count==0,"invalid bitmap dimensions draw nothing");
    playback_enabled=false;spy.draw();require(spy.count==0,"toggle-off clears visible output immediately");
    std::cout<<"PASS production ghost overlay checks="<<checks<<" off_calls=0 maximum_calls=512\n";
}
