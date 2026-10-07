#pragma once
#include "widescreen_strips.h"

namespace sma3::wide {
// Screen-space objects stay clipped to the native view. Classification must
// come from a known producer AND match the completed object, never its slot alone.
inline void apply_screen_observation(ObjectPosition& p,const ObjectPosition& ui) {
    if(ui.screen_space && ui.source==p.source && ui.a0==p.a0 &&
       ui.a1==p.a1 && ui.a2==p.a2) p=ui;
}
template<class Reader>
bool observe_screen_objects(Reader read,uint32_t pc,const uint32_t* r,StagingPositions& out) {
    auto set=[&](unsigned slot,unsigned a0,unsigned a1,unsigned a2) {
        out[slot]={0,false,uint16_t(a0),uint16_t(a1),uint16_t(a2),slot,true};
    };
    if(pc==0x080e9124) {
        const unsigned message=r[0];
        if(message<0x03000000 || message>0x03007fa0)return false;
        const auto code=read(message+14,2)>>8;
        if(code==15 || code==0x51 || code==255 || read(message+10,2)>1 || !(read(0x03006b01,1)&16))return false;
        const unsigned x=160-(read(0x030069e8,2)&7),y=95-(read(0x030069f0,2)&7);
        set(0,0x4000|y,x,0xf3f4);
        return true;
    }
    if(pc==0x0802d0cc) {
        // Five fixed HUD components copied from one of two ROM templates.
        const unsigned first=r[3],table=r[2];
        if(first>=5 || r[1]!=0x03005ae0+first*8) return false;
        const uint32_t a=read(0x081693ec,4),b=read(0x081693f0,4);
        if((table!=a+first*6 && table!=b+first*6) ||
           table<0x08000000 || uint64_t(table)+(5-first)*6>0x08400000) return false;
        for(unsigned i=first;i<5;++i) {
            const auto t=table+(i-first)*6;
            set(28+i,read(t,2),read(t+2,2),read(t+4,2));
        }
        return true;
    }
    if(pc==0x080dfdc2) {
        // Four components of the direction indicator. Its producer clamps
        // the anchor to screen X=2..222, Y=10..143 before this loop.
        const unsigned first=r[5],table=r[4];
        if(first>=4 || r[3]!=0x03005b08+first*8 ||
           table<0x08190e74 || uint64_t(table)+(4-first)*4>0x08190eb4 ||
           r[6]<2 || r[6]>222 || r[7]<10 || r[7]>143) return false;
        for(unsigned i=first;i<4;++i) {
            const auto t=table+(i-first)*4,graphic=read(t+3,1);
            set(33+i,(read(t,1)+r[7])&255,
                ((read(t+1,1)+r[6])&511)|(read(t+2,1)<<8),
                (graphic&15)|((graphic&240)<<1)|r[12]);
        }
        return true;
    }
    if(pc==0x080e98a0) {
        // Message frame templates. Keep every component in screen space;
        // require a terminator before publishing any observation.
        const unsigned table=r[4],address=r[5],sp=r[13];
        if(address<0x03005a00 || address>=0x03005ae0 || (address&7) ||
           sp<0x03000000 || sp>0x03007ff8 || table<0x08000000) return false;
        const unsigned first=(address-0x03005a00)/8;
        unsigned count=0;
        for(;count<28-first;++count) {
            if(uint64_t(table)+(count+1)*8>0x08400000) return false;
            if(read(table+count*8+6,2)==65535) break;
        }
        if(count==28-first) return false;
        const auto y=read(sp,4),x=read(sp+4,4);
        for(unsigned i=0;i<count;++i) {
            const auto t=table+i*8,a0=read(t,2),a1=read(t+2,2);
            set(first+i,(a0&0xff00)|((a0+y)&255),
                (a1&0xfe00)|((a1+x)&511),read(t+4,2));
        }
        return true;
    }
    return false;
}
}
