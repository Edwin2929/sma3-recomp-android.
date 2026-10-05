#pragma once
#include <array>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
namespace sma3 {
// D-pad, A, B, L, R, Select, Start; safe-area normalized centre x/y, width/height.
using TouchDesign=std::array<float,28>;
inline std::mutex touch_mutex;
inline TouchDesign touch_design{},default_touch_design{};
inline bool touch_enabled=false;
inline std::atomic<bool> touch_editing{false};
inline bool valid_design(const TouchDesign& d){
 for(int i=0;i<28;i+=4){for(int j=0;j<4;j++)if(!std::isfinite(d[i+j]))return false;
 if(d[i]<0||d[i]>1||d[i+1]<0||d[i+1]>1||d[i+2]<.015f||d[i+2]>1||d[i+3]<.015f||d[i+3]>1)return false;}return true;
}
inline bool set_touch_design(const TouchDesign& d,bool on){
 if(on&&!valid_design(d))return false;std::lock_guard<std::mutex> l(touch_mutex);touch_design=d;touch_enabled=on;return true;
}
inline TouchDesign get_touch_design(bool defaults){std::lock_guard<std::mutex> l(touch_mutex);return !defaults&&touch_enabled?touch_design:default_touch_design;}
template<class Layout> void apply_touch_design(Layout& p,float left,float top,float right,float bottom){
 float w=right-left,h=bottom-top;if(w<=0||h<=0)return;
 TouchDesign base{};
 auto circle=[&](int i,const auto& c){base[i*4]=(c.x-left)/w;base[i*4+1]=(c.y-top)/h;base[i*4+2]=2*c.r/w;base[i*4+3]=2*c.r/h;};
 auto rect=[&](int i,const auto& r){base[i*4]=(r.x+r.w/2-left)/w;base[i*4+1]=(r.y+r.h/2-top)/h;base[i*4+2]=r.w/w;base[i*4+3]=r.h/h;};
 circle(0,p.dpad);circle(1,p.a);circle(2,p.b);rect(3,p.l);rect(4,p.r);rect(5,p.select);rect(6,p.start);
 TouchDesign d;{std::lock_guard<std::mutex> l(touch_mutex);default_touch_design=base;if(!touch_enabled)return;d=touch_design;}
 auto updateCircle=[&](int i,auto& c){const float* v=d.data()+4*i;c.r=std::min(v[2]*w,v[3]*h)/2;c.x=std::clamp(left+v[0]*w,left+c.r,right-c.r);c.y=std::clamp(top+v[1]*h,top+c.r,bottom-c.r);};
 float old=p.dpad.r;updateCircle(0,p.dpad);updateCircle(1,p.a);updateCircle(2,p.b);if(old>0)p.dpad_thick*=p.dpad.r/old;
 auto updateRect=[&](int i,auto& r){const float* v=d.data()+4*i;r.w=v[2]*w;r.h=v[3]*h;r.x=std::clamp(left+v[0]*w-r.w/2,left,right-r.w);r.y=std::clamp(top+v[1]*h-r.h/2,top,bottom-r.h);};
 updateRect(3,p.l);updateRect(4,p.r);updateRect(5,p.select);updateRect(6,p.start);
}
}
