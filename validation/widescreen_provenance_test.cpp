#include "../src/widescreen_strips.h"
#include <map>
#include <stdexcept>
#include <iostream>
int main() {
    std::map<uint32_t,uint32_t> m;
    auto read=[&](uint32_t a,unsigned){return m[a];};
    auto require=[](bool v){if(!v)throw std::runtime_error("provenance regression");};
    sma3::wide::StagingPositions out{};
    uint32_t r[16]{};r[7]=0x03005a08;r[9]=272;r[8]=40;r[0]=2;r[5]=37;r[12]=0x5600;
    require(sma3::wide::observe_generic(read,r,out));
    require(out[1].x==272 && out[1].a0==40 && out[1].a1==0x5110 && out[1].a2==0x3c25);
    r[9]=uint32_t(-240);require(!sma3::wide::observe_generic(read,r,out) && !out[1].valid);
    r[9]=uint32_t(-64);require(sma3::wide::observe_generic(read,r,out) && out[1].x==-64);
    r[7]=0x03006200;require(!sma3::wide::observe_generic(read,r,out));
    constexpr uint32_t p=0x03006d80,t=0x081ac634;
    m[p+0x94]=4;m[p+0x32]=0;m[0x081ac028]=2;m[p+0x2c]=uint16_t(-20);m[p+0x2e]=50;m[p+0x42]=1;
    m[t]=0;m[t+1]=uint8_t(-8);m[t+2]=uint8_t(-3);
    m[t+4]=0x20;m[t+5]=4;m[t+6]=5;
    require(sma3::wide::observe_yoshi(read,out));
    require(out[8].valid && out[8].x==-28 && out[8].a0==47 && out[8].a1==484 && out[8].a2==0);
    require(out[9].valid && out[9].x==-16 && out[9].a1==0x41f0 && out[9].a2==2);
    m[p+0x42]=0;require(sma3::wide::observe_yoshi(read,out));
    require(out[8].x==-4 && out[8].a1==0x11fc);
    m[t]=8;require(sma3::wide::observe_yoshi(read,out) && out[8].valid && out[8].x==-28);
    const uint32_t o=0x03005a40;
    m[o]=out[8].a0;m[o+2]=out[8].a1;m[o+4]=out[8].a2;m[0x030069d2]=3;
    require(sma3::wide::observe_yoshi_affine(read,o,out));
    require(out[8].x==-28 && out[8].a0==0x12f && out[8].a1==0x87e4);
    m[p+0x3c]=0x164;m[0x03006e3a]=2;
    m[o]=out[8].a0;m[o+2]=out[8].a1;m[o+4]=out[8].a2;
    require(sma3::wide::observe_yoshi_affine(read,o,out));
    require(out[8].x==-44 && out[8].a0==0x31f && out[8].a1==0x87d4);
    m[o+4]=999;require(!sma3::wide::observe_yoshi_affine(read,o,out) && !out[8].valid);
    m[p+0x3c]=516;require(!sma3::wide::observe_yoshi(read,out));
    r[0]=0x08001000;r[1]=2;r[2]=4;r[3]=0x120;r[13]=0x03007000;
    m[r[13]]=uint32_t(-8);m[r[13]+4]=10;m[r[13]+8]=272;m[r[13]+12]=8;
    m[r[0]]=1;m[r[0]+1]=3;m[r[0]+2]=0x40;
    m[r[0]+3]=2;m[r[0]+4]=5;m[r[0]+5]=0;
    require(sma3::wide::observe_strip(read,0x0804211c,r,out));
    require(out[2].x==272 && out[2].a0==11 && out[2].a1==0x1110 && out[2].a2==0x123);
    require(out[3].x==280 && out[3].a0==12);
    m[r[13]]=100;m[r[13]+4]=20;m[r[13]+8]=0;m[r[13]+12]=4;m[r[0]]=uint8_t(-8);
    require(sma3::wide::observe_strip(read,0x080421a8,r,out));
    require(out[2].x==112 && out[2].a0==28 && out[3].a0==36);
    r[2]=257;require(!sma3::wide::observe_strip(read,0x080421a8,r,out));
    const uint32_t sprite=0x03007000,oa=0x03005a10;
    m[sprite+0x34]=8;m[sprite+0x94]=0;m[sprite+0x20]=uint16_t(-230);
    m[oa]=40;m[oa+2]=272;m[oa+4]=0x4994;
    require(sma3::wide::observe_sprite_affine(read,sprite,out) && out[2].x==-240);
    m[sprite+0x20]=280;
    require(sma3::wide::observe_sprite_affine(read,sprite,out) && out[2].x==272 && out[2].a0==0x128);
    m[oa]=out[2].a0;m[oa+2]=out[2].a1;m[oa+4]=out[2].a2;
    m[sprite+0x76]=0x100;m[0x081af4cc]=256;
    require(sma3::wide::observe_sprite_bob(read,sprite,out));
    require(out[2].x==272 && out[2].a0==0x130);
    m[oa]=out[2].a0;m[sprite+0x76]=0;m[0x081af2cc]=uint16_t(-256);
    require(sma3::wide::observe_sprite_bob(read,sprite,out) && out[2].a0==0x118);
    m[oa]=out[2].a0;m[sprite+0x76]=512;
    require(!sma3::wide::observe_sprite_bob(read,sprite,out) && !out[2].valid);
    m[sprite+0x20]=0;
    require(!sma3::wide::observe_sprite_affine(read,sprite,out) && !out[2].valid);
    std::cout<<"PASS: signed generic positions, clipping, slot bounds, Yoshi offsets, facing, affine matrices, exact vertical animation and both strip emitters\n";
}
