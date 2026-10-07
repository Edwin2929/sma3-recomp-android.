#include "../src/widescreen_ui.h"
#include <map>
#include <stdexcept>
#include <iostream>
int main() {
    std::map<uint32_t,uint32_t> m;
    auto read=[&](uint32_t a,unsigned){return m[a];};
    auto check=[](bool x){if(!x)throw std::runtime_error("screen-space regression");};
    sma3::wide::StagingPositions out{};
    uint32_t r[16]{};
    r[1]=0x03005ae0;r[2]=0x081693cc;m[0x081693ec]=r[2];
    m[r[2]]=24;m[r[2]+2]=0x40c8;m[r[2]+4]=0x100c;
    check(sma3::wide::observe_screen_objects(read,0x0802d0cc,r,out));
    check(out[28].screen_space && !out[28].valid && out[28].a1==0x40c8);
    auto p=out[28];p.screen_space=false;p.valid=true;p.x=200;
    sma3::wide::apply_screen_observation(p,out[28]);
    check(p.screen_space && !p.valid);
    p=out[28];p.screen_space=false;p.valid=true;++p.a2;
    sma3::wide::apply_screen_observation(p,out[28]);
    check(!p.screen_space && p.valid); // Reused slot with another graphic.
    p=out[28];p.screen_space=false;p.source=29;
    sma3::wide::apply_screen_observation(p,out[28]);check(!p.screen_space);
    ++r[1];check(!sma3::wide::observe_screen_objects(read,0x0802d0cc,r,out));
    r[4]=0x08190e74;r[3]=0x03005b08;r[5]=0;r[6]=2;r[7]=88;r[12]=0x1000;
    m[r[4]]=0;m[r[4]+1]=0;m[r[4]+2]=0x10;m[r[4]+3]=0xd5;
    check(sma3::wide::observe_screen_objects(read,0x080dfdc2,r,out));
    check(out[33].screen_space && out[33].a0==88 && out[33].a1==0x1002 && out[33].a2==0x11a5);
    r[6]=223;check(!sma3::wide::observe_screen_objects(read,0x080dfdc2,r,out));
    r[4]=0x08190000;r[5]=0x03005a08;r[13]=0x03007000;
    m[r[13]]=64;m[r[13]+4]=120;
    m[r[4]]=0x80ce;m[r[4]+2]=0xc030;m[r[4]+4]=0xf290;m[r[4]+6]=0;
    m[r[4]+14]=65535;
    check(sma3::wide::observe_screen_objects(read,0x080e98a0,r,out));
    check(out[1].screen_space && out[1].a0==0x800e && out[1].a1==0xc0a8 && out[1].a2==0xf290);
    m[r[4]+14]=0;out={};
    check(!sma3::wide::observe_screen_objects(read,0x080e98a0,r,out) && !out[1].screen_space);
    r[4]=0x083ffffe;check(!sma3::wide::observe_screen_objects(read,0x080e98a0,r,out));
    r[0]=0x03002400;m[r[0]+14]=0x1200;m[r[0]+10]=1;m[0x03006b01]=16;
    m[0x030069e8]=3;m[0x030069f0]=2;
    check(sma3::wide::observe_screen_objects(read,0x080e9124,r,out));
    check(out[0].screen_space && out[0].a0==0x405d && out[0].a1==157 && out[0].a2==0xf3f4);
    m[r[0]+14]=0x5100;check(!sma3::wide::observe_screen_objects(read,0x080e9124,r,out));
    std::cout<<"PASS: HUD templates, bounded indicator, message wrapping, sentinel and pointer limits, slot reuse rejection\n";
}
