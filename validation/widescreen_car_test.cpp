#include "../src/widescreen_car.h"
#include <map>
#include <stdexcept>
#include <iostream>
int main() {
    std::map<uint32_t,uint32_t> m;
    auto read=[&](uint32_t a,unsigned){return m[a];};
    auto check=[](bool x){if(!x)throw std::runtime_error("car regression");};
    sma3::wide::StagingPositions out{};uint32_t r[16]{};
    const unsigned a=0x03005a00,c=0x03004c84,sp=0x03007e00;
    out[0]={-32,true,0x150,0x81e0,0x5800,0};
    m[a]=0x150;m[a+2]=0x81e0;m[a+4]=0x5800;m[0x030069fc]=0;
    check(sma3::wide::observe_car_body_copy(read,out));
    check(!out[0].valid && out[2].x==-32 && out[2].source==2 && out[2].a1==0x81e0);
    out[0]=out[2];++m[a+4];check(!sma3::wide::observe_car_body_copy(read,out) && !out[2].valid);
    m[a]=m[a+24]=0;r[3]=a;r[5]=a+24;r[6]=c;r[13]=sp;r[9]=80;r[8]=4;r[10]=6;
    m[c]=1000;m[c+2]=5;m[0x030069d4]=1024;m[sp+8]=4;m[sp+4]=1;m[sp+12]=1;m[sp+16]=16;
    check(sma3::wide::observe_car_supports(read,0x08042608,r,out));
    check(out[0].x==-32 && out[3].x==-36 && out[0].a0==85);
    check(out[0].a1==0x51e0 && out[3].a1==0x41dc && out[0].a2==0x5804 && out[3].a2==0x5806);
    r[5]=c;r[8]=16;m[sp+24]=728;m[sp+20]=0;m[a+2]=m[a+10]=0;m[a+8]=0;
    check(sma3::wide::observe_car_supports(read,0x0804273e,r,out));
    check(out[0].x==272 && out[1].x==268 && out[0].a1==0x1110 && out[1].a1==0x10c);
    check(out[0].a2==0x4844 && out[1].a2==0x4844);
    m[0x03006a0a]=9;m[c+4]=1008;m[c+6]=6;
    check(sma3::wide::observe_car_supports(read,0x0804273e,r,out));
    check(out[2].x==280 && out[4].x==272 && out[6].x==280 && out[4].a0==77);
    r[12]=3;check(!sma3::wide::observe_car_supports(read,0x0804273e,r,out));r[12]=0;
    m[0x03006a0a]=300;check(!sma3::wide::observe_car_supports(read,0x0804273e,r,out));
    m[0x03006a0a]=0;
    r[3]=0x030061f8;check(!sma3::wide::observe_car_supports(read,0x0804273e,r,out));
    r[3]=a;r[13]=1;check(!sma3::wide::observe_car_supports(read,0x0804273e,r,out));
    std::cout<<"PASS: exact body copy, stale rejection, signed supports, flips, tiles and bounds\n";
}
