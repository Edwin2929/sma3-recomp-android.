#include "../src/widescreen_rotating.h"
#include "../src/widescreen_ui.h"
#include <map>
#include <stdexcept>
#include <iostream>
int main() {
    std::map<uint32_t,uint32_t> m;
    auto read=[&](uint32_t a,unsigned){return m[a];};
    auto check=[](bool x){if(!x)throw std::runtime_error("rotating position regression");};
    sma3::wide::StagingPositions out{};uint32_t r[16]{};
    const unsigned a=0x03005a00,s=0x03004000;
    m[0x030021a4]=a;m[a]=0x4000;m[a+2]=0x4000;m[a+4]=0x998;
    r[0]=288;r[1]=65;
    check(sma3::wide::observe_rotating_position(read,r,out));
    check(out[0].x==272 && out[0].a1==0x4100 && out[0].a0==0x4031);
    r[0]=uint32_t(-224);check(sma3::wide::observe_rotating_position(read,r,out));
    check(out[0].x==-240 && out[0].a1==0x4100);
    r[0]=uint32_t(-16);r[1]=uint32_t(-16);
    check(sma3::wide::observe_rotating_position(read,r,out));
    check(out[0].x==-32 && out[0].a1==0x41e0 && out[0].a0==0x40e0);
    r[0]=272;r[1]=208;check(sma3::wide::observe_rotating_position(read,r,out));
    check(out[0].x==256 && out[0].a1==0x4100 && out[0].a0==0x40c0);
    m[0x030021a4]=a+1;check(!sma3::wide::observe_rotating_position(read,r,out));
    m[s+0x34]=0;m[s+0x32]=0x102;m[0x030069d2]=4;
    for(unsigned i=0;i<6;++i) {
        m[0x030021a4]=a+i*8;r[0]=64+i*8;r[1]=48;
        check(sma3::wide::observe_rotating_position(read,r,out));
        m[a+i*8]=out[i].a0;m[a+i*8+2]=out[i].a1;m[a+i*8+4]=out[i].a2;
    }
    ++m[a+4]; // Stale first component must not be promoted.
    check(sma3::wide::observe_rotating_affine(read,s,out));
    check(!out[0].valid && out[1].valid && out[1].x==56);
    check(out[1].a1==(0x8000|4<<9|56) && out[1].a0==0x120);
    check(out[3].a1==(0x8000|5<<9|72));
    m[s+0x34]=255*4;check(!sma3::wide::observe_rotating_affine(read,s,out));
    m[s+0x34]=0;m[0x030069d2]=31;check(!sma3::wide::observe_rotating_affine(read,s,out));
    r[7]=s;r[5]=a;r[0]=0x168;r[1]=0;m[s+0x34]=0;m[s+0x20]=48;
    m[a+2]=0x4030;m[a+4]=0x99c;m[0x030069d2]=1;
    check(sma3::wide::observe_scaled_component(read,r,out));
    check(out[0].x==32 && out[0].a0==0x368 && out[0].a1==0x8220 && out[0].a2==0x99c);
    r[8]=s;r[0]=0x300;r[1]=0x68;
    check(sma3::wide::observe_scaled_component(read,r,out,0x08063d32));
    check(out[0].x==32 && out[0].a0==0x368 && out[0].a1==0x8220);
    check(!sma3::wide::observe_scaled_component(read,r,out,0));
    m[s+0x20]=uint16_t(-16);m[a+2]=0x41f0;
    check(sma3::wide::observe_scaled_component(read,r,out) && out[0].x==-32);
    m[a+2]=200;check(!sma3::wide::observe_scaled_component(read,r,out));
    m[s+0x34]=4;check(!sma3::wide::observe_scaled_component(read,r,out));
    m[s+0x34]=0;m[a]=160;m[a+2]=0;r[0]=152;r[1]=0;r[7]=s;
    check(sma3::wide::observe_scaled_component(read,r,out));
    check(!out[0].valid && out[0].screen_space && out[0].a0==0x398 && out[0].a1==0x83f0);
    auto candidate=out[0];candidate.valid=true;candidate.screen_space=false;
    sma3::wide::apply_screen_observation(candidate,out[0]);
    check(!candidate.valid && candidate.screen_space);
    candidate=out[0];candidate.valid=true;candidate.screen_space=false;++candidate.a2;
    sma3::wide::apply_screen_observation(candidate,out[0]);check(candidate.valid);
    r[10]=s;r[7]=s+0x76;m[s+0x34]=0;r[5]=a+16;
    r[4]=160;r[0]=6;r[9]=0;m[s+0x20]=uint16_t(-16);
    m[a+18]=0x8000;m[a+20]=0x194;m[0x030069d2]=2;
    check(sma3::wide::observe_offset_affine(read,r,out));
    check(out[2].x==-24 && out[2].a0==0x19a && out[2].a1==0x85e8 && out[2].a2==0x194);
    m[s+0x34]=0xffff;r[5]=0x03025a08;m[a+10]=0;m[a+12]=0;
    check(sma3::wide::observe_offset_affine(read,r,out) && out[1].x==-24);
    ++r[5];check(!sma3::wide::observe_offset_affine(read,r,out));--r[5];
    ++r[7];check(!sma3::wide::observe_offset_affine(read,r,out));
    std::cout<<"PASS: unclamped signed X, guest clamp boundaries, affine groups, stale attributes and capacity\n";
}
