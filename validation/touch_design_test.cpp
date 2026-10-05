#include "touch_layout.h"
#include <cassert>
#include <limits>
#include <cstdio>
struct Circle {float x,y,r;};struct Rect {float x,y,w,h;};
struct Pad {Circle dpad,a,b;float dpad_thick;Rect l,r,select,start;};
int main(){
 Pad initial{{150,450,70},{800,400,45},{700,470,45},40,{60,40,130,45},{800,40,130,45},{430,500,80,30},{540,500,80,30}};
 sma3::TouchDesign empty{};assert(sma3::set_touch_design(empty,false));
 Pad p=initial;sma3::apply_touch_design(p,20,10,980,590);assert(p.a.x==initial.a.x);
 auto defaults=sma3::get_touch_design(true);assert(sma3::valid_design(defaults));
 auto d=defaults;d[4]=.1f;d[5]=.2f;d[6]=.1f;d[7]=.16f;
 assert(sma3::set_touch_design(d,true));p=initial;sma3::apply_touch_design(p,20,10,980,590);
 assert(std::abs(p.a.x-116)<.01f&&std::abs(p.a.y-126)<.01f);
 assert(std::abs(p.a.r-46.4f)<.01f); // same circle used by renderer and hit testing
 assert(sma3::get_touch_design(true)==defaults); // resetting retains computed factory layout
 auto bad=d;bad[0]=std::numeric_limits<float>::quiet_NaN();assert(!sma3::set_touch_design(bad,true));assert(sma3::get_touch_design(false)==d);
 bad=d;bad[2]=0;assert(!sma3::set_touch_design(bad,true));bad=d;bad[3]=1.1;assert(!sma3::set_touch_design(bad,true));
 d[0]=0;d[1]=1;d[2]=.5;d[3]=.5;d[12]=0;d[13]=1;d[14]=.9;d[15]=.5;
 assert(sma3::set_touch_design(d,true));p=initial;sma3::apply_touch_design(p,20,10,980,590);
 assert(p.dpad.x-p.dpad.r>=20&&p.dpad.y+p.dpad.r<=590);assert(p.l.x>=20&&p.l.y+p.l.h<=590);
 // Rotation/resolution change keeps all geometry in the new safe area.
 p=initial;sma3::apply_touch_design(p,0,30,600,1000);assert(p.dpad.x-p.dpad.r>=0&&p.dpad.y+p.dpad.r<=1000);
 assert(sma3::set_touch_design(empty,false));p=initial;sma3::apply_touch_design(p,20,10,980,590);assert(p.a.x==800&&p.l.w==130);
 puts("PASS: factory geometry, independent movement/size, safe-area clamps, orientation, invalid data rejection and reset");
}
