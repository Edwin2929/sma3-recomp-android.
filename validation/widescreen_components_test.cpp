#include "../src/widescreen_components.h"
#include <map>
#include <stdexcept>
#include <iostream>
int main() {
    std::map<uint32_t,uint32_t> m;
    auto read=[&](uint32_t a,unsigned){return m[a];};
    auto check=[](bool x){if(!x)throw std::runtime_error("component position regression");};
    sma3::wide::StagingPositions out{};uint32_t r[16]{};
    r[4]=37;r[8]=264;r[9]=80;r[7]=0x800;m[0x03006a02]=8;m[0x03006a04]=0x1000;
    check(sma3::wide::observe_tongue_tip(read,0x08042380,r,out));
    check(out[37].x==272 && out[37].a1==0x1110 && out[37].a2==0x5840);
    r[8]=uint32_t(-40);check(sma3::wide::observe_tongue_tip(read,0x0804244c,r,out));
    check(out[37].x==-32 && out[37].a2==0x5842);
    r[4]=256;check(!sma3::wide::observe_tongue_tip(read,0x08042380,r,out));
    r[7]=0x03005a00;r[5]=0x030069f4;r[6]=0x08190000;r[4]=14;
    m[r[5]+10]=uint16_t(-8);m[r[5]+12]=16;m[r[5]+14]=0x200;m[r[6]+4]=2;
    check(sma3::wide::observe_toadies(read,0x0804f44a,r,out));
    check(out[0].x==-8 && out[0].a0==16 && out[0].a1==0x41f8 && out[0].a2==0x100e);
    const unsigned a=0x03005a00,c=0x03004000;
    r[6]=a;r[12]=c;m[a]=out[0].a0;m[a+2]=out[0].a1;m[a+4]=out[0].a2;
    m[c]=24*256;m[c+4]=uint32_t(-32*256);
    check(sma3::wide::observe_toadies(read,0x0804f51c,r,out));
    check(out[0].x==16 && out[0].a0==240 && out[0].a1==0x4010);
    m[a]=out[0].a0;m[a+2]=out[0].a1;m[a+4]=out[0].a2;
    m[a+12]=m[a+20]=0x1000;m[c+28]=2;m[0x081720cc]=0x4c;
    check(sma3::wide::observe_toadies(read,0x0804f5ac,r,out));
    check(out[0].a1==0x5010 && out[1].x==16 && out[2].x==24);
    check(out[1].a0==232 && out[2].a0==232 && out[1].a2==0x104c && out[2].a1==0x1018);
    ++m[a+4];check(!sma3::wide::observe_toadies(read,0x0804f51c,r,out) && !out[0].valid);
    check(!sma3::wide::object_intersects_vertical(0x01a8,0x8200));
    check(sma3::wide::object_intersects_vertical(0x03a8,0xc200));
    check(sma3::wide::object_intersects_vertical(255,0));
    check(!sma3::wide::object_intersects_vertical(0x200,0));
    check(!sma3::wide::object_intersects_vertical(0xc000,0));
    std::cout<<"PASS: signed tongue tips, two orientations, compound sprite translation, shared components, flips and stale attributes\n";
}
