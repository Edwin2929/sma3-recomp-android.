#pragma once
#include "widescreen_strips.h"
namespace sma3::wide {
template<class Reader>
bool observe_car_body_copy(Reader read,StagingPositions& out) {
    const unsigned first=read(0x030069fc,2)>>2;
    if(first>253)return false;
    const auto p=out[first];const unsigned a=0x03005a00+first*8;
    out[first+2]={};out[first]={};
    if(!p.valid || p.a0!=read(a,2) || p.a1!=read(a+2,2) || p.a2!=read(a+4,2))return false;
    out[first+2]=p;out[first+2].source=first+2;
    return true;
}
// Two known emitters for the car's extending wheel supports. Full coordinates
// come from the player's world-coordinate records, before nine-bit masking.
template<class Reader>
bool observe_car_supports(Reader read,uint32_t pc,const uint32_t* r,StagingPositions& out) {
    const bool base=pc==0x08042608;
    if(!base && pc!=0x0804273e)return false;
    const unsigned a=r[3],b=base?r[5]:a+8,c=r[base?6:5],sp=r[13];
    if(a<0x03005a00 || a>=0x03006200 || (a&7) ||
       b<0x03005a00 || b>=0x03006200 || (b&7) ||
       c<0x03000000 || c>0x03007ffc || sp<0x03000000 || sp>0x03007fe0 || (sp&3))return false;
    const uint32_t origin=base?uint32_t(int16_t(read(0x030069d4,2)))+8:read(sp+0x18,4);
    if(r[12]>2 || (r[12]&1))return false;
    const unsigned steps=2-r[12]/2;
    const int remaining=int16_t(read(0x03006a0a,2));
    const unsigned layers=base || r[12]?1:unsigned(remaining>0?(remaining+7)/8:1);
    if(layers>32)return false;
    // Internal guest loop edges may stay inside one recompiled function.
    // Predict the remaining iterations, not just the first pair.
    for(unsigned layer=0;layer<layers;++layer) for(unsigned step=0;step<steps;++step) {
        const unsigned ca=c+step*4;
        const unsigned aa=a+(base?step*8:layer*32+step*16);
        const unsigned bb=base?b+step*8:aa+8;
        if(ca>0x03007ffc || aa>=0x03006200 || bb>=0x03006200)return false;
        const uint32_t x=uint32_t(int16_t(read(ca,2)))-origin;
        const uint32_t xs[2]={x,x-read(sp+8,4)};
        const unsigned addresses[2]={aa,bb};
        const unsigned y=(read(ca+2,1)+r[9]-layer*8)&255;
        for(unsigned i=0;i<2;++i) {
            const unsigned address=addresses[i],slot=(address-0x03005a00)/8;
            unsigned a1=(read(address+2,2)&0xee00)|(xs[i]&511);
            unsigned flip;
            if(base)flip=i?((1^read(sp+4,4))&read(sp+12,1)):(read(sp+16,4)>>4);
            else flip=i?(read(sp+20,4)>>4):(r[8]>>4);
            a1|=(flip&1)<<12;
            if(base)a1=(a1&0x3fff)|0x4000;
            const unsigned tile=base?(0x5800|r[i?10:8]):0x4844;
            out[slot]={int32_t(xs[i]),true,uint16_t((read(address,2)&0xff00)|y),
                uint16_t(a1),uint16_t(tile),slot};
        }
    }
    return true;
}
}
