#pragma once
#include "widescreen_strips.h"
namespace sma3::wide {
// At 08084D92 the secondary component rewrites the generic emitter's XY.
// Preserve world-minus-camera X before the guest masks it to nine OAM bits.
template<class Reader>
bool observe_secondary_component(Reader read,const uint32_t* r,StagingPositions& out) {
    const unsigned address=r[4];
    if(address<0x03005a00 || address>=0x03006200 || (address&7) ||
       r[5]<0x03000000 || r[5]>0x03007f9c || r[6]!=r[5]+0x62)return false;
    const unsigned slot=(address-0x03005a00)/8;
    const int x=int16_t(uint16_t(read(r[6],2)-read(0x030069d4,2)));
    out[slot]={x,true,uint16_t(r[3]),
        uint16_t((read(address+2,2)&0xfe00)|(unsigned(x)&511)),
        uint16_t(read(address+4,2)),slot};
    return true;
}
template<class Reader>
bool observe_tongue_tip(Reader read,uint32_t pc,const uint32_t* r,StagingPositions& out) {
    if((pc!=0x08042380 && pc!=0x0804244c) || r[4]>=256) return false;
    // The horizontal branch writes its step at entry before emitting the tip.
    // Neither branch changes these signed anchor or tip offset values.
    const int x=int32_t(r[8])+int16_t(read(0x03006a02,2));
    const unsigned slot=r[4];
    out[slot]={x,true,uint16_t(r[9]&255),
        uint16_t((unsigned(x)&511)|read(0x03006a04,2)),
        uint16_t(r[7]|(pc==0x08042380?0x5040:0x5042)),slot};
    return true;
}
template<class Reader>
bool observe_toadies(Reader read,uint32_t pc,const uint32_t* r,StagingPositions& out) {
    const unsigned address=pc==0x0804f44a?r[7]:r[6];
    if(address<0x03005a00 || address>=0x03006200 || (address&7))return false;
    const unsigned slot=(address-0x03005a00)/8;
    if(pc==0x0804f44a) {
        if(r[5]!=0x030069f4 || r[6]<0x08000000 || r[6]>0x083ffffb)return false;
        const auto attr=read(r[5]+14,2);
        const int x=int16_t(read(r[5]+10,2));
        out[slot]={x,true,uint16_t(read(r[5]+12,1)),
            uint16_t(((read(r[6]+4,1)<<13)&0xc000)|((attr>>2)&0x3000)|(unsigned(x)&511)),
            uint16_t(((attr<<3)&0x7000)|((((attr>>2)^0xc00)+0x400)&0xc00)|(r[4]&1023)),slot};
        return true;
    }
    if(pc!=0x0804f51c && pc!=0x0804f5ac)return false;
    auto& base=out[slot];
    if(!base.valid || base.a0!=read(address,2) || base.a1!=read(address+2,2) || base.a2!=read(address+4,2)) {
        base={};return false;
    }
    const auto child=r[12];
    if(child<0x03000000 || child>0x03007fdc){base={};return false;}
    if(pc==0x0804f51c) {
        if((base.a0&255)==160)return false;
        base.x+=int32_t(read(child,4))>>8;
        base.a0=uint16_t((base.a0&0xff00)|((base.a0+(int32_t(read(child+4,4))>>8))&255));
        base.a1=uint16_t((base.a1&0xfe00)|(unsigned(base.x)&511));
        return true;
    }
    if(slot>253)return false;
    const auto frame=read(child+32,2);
    if(frame>=4)return false;
    const auto graphic=read(0x081720cc+frame,1);
    const unsigned first_a1=base.a1;
    for(unsigned i=1;i<=2;++i) {
        const auto a=address+i*8;
        const int x=base.x+(i==2?8:0);
        unsigned a1=(read(a+2,2)&0xfe00)|(unsigned(x)&511);
        if(i==2)a1=(a1&~0x1000)|((~out[slot+1].a1)&0x1000);
        out[slot+i]={x,true,uint16_t((read(a,2)&0xff00)|((base.a0-8)&255)),
            uint16_t(a1),uint16_t((read(a+4,2)&0xfc00)|graphic),slot+i};
    }
    base.a1=uint16_t(first_a1^(read(child+28,2)<<11));
    return true;
}
}
