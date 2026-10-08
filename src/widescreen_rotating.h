#pragma once
#include "widescreen_strips.h"
namespace sma3::wide {
// 0809E324 receives complete component coordinates, then clamps/masks OAM XY.
// Preserve the original X; predict the guest's actual attributes independently.
template<class Reader>
bool observe_rotating_position(Reader read,const uint32_t* r,StagingPositions& out) {
    const unsigned a=read(0x030021a4,4);
    if(a<0x03005a00 || a>=0x03006200 || (a&7))return false;
    const unsigned slot=(a-0x03005a00)/8;
    const uint32_t x=r[0]-16,y=r[1]-16;
    const unsigned stored_x=x+32<=288?x:256,stored_y=y+32<=224?y:192;
    out[slot]={int32_t(x),true,uint16_t((read(a,2)&0xff00)|(stored_y&255)),
        uint16_t((read(a+2,2)&0xfe00)|(stored_x&511)),uint16_t(read(a+4,2)),slot};
    return true;
}
// 0809E774 assigns one affine matrix to each group of three components.
// Only transform observations that still exactly match the pre-affine staging.
template<class Reader>
bool observe_rotating_affine(Reader read,unsigned sprite,StagingPositions& out) {
    if(sprite<0x03000000 || sprite>0x03007fc8)return false;
    const unsigned first=read(sprite+0x34,2)/4;
    const unsigned count=read(sprite+0x32,2)==0x101?3:6;
    const unsigned matrix=read(0x030069d2,2);
    if(first+count>256 || matrix+(count/3)>32)return false;
    for(unsigned i=0;i<count;++i) {
        auto& p=out[first+i];const unsigned a=0x03005a00+(first+i)*8;
        if(!p.valid || p.a0!=read(a,2) || p.a1!=read(a+2,2) || p.a2!=read(a+4,2)) {
            p={};continue;
        }
        p.a0|=0x100;
        p.a1=uint16_t((p.a1&511)|((matrix+i/3)<<9)|0x8000);
    }
    return true;
}
// 08063B52 and 08063D32 expand one component's affine box and shifts its origin left 16.
template<class Reader>
bool observe_scaled_component(Reader read,const uint32_t* r,StagingPositions& out,uint32_t pc=0x08063b52) {
    if(pc!=0x08063b52 && pc!=0x08063d32)return false;
    const unsigned sprite=r[pc==0x08063b52?7:8],a=r[5];
    if(sprite<0x03000000 || sprite>0x03007fc8 || a<0x03005a00 ||
       a>=0x03006200 || (a&7))return false;
    const unsigned allocation=read(sprite+0x34,2);
    if((allocation&3) || allocation>=1024 || a!=0x03005a00+allocation*2)return false;
    const unsigned a1=read(a+2,2),slot=allocation/4;
    const int anchor=int16_t(read(sprite+0x20,2));
    const int offset=int(((a1-(unsigned(anchor)&511)+256)&511))-256;
    if(offset< -64 || offset>64)return false;
    out[slot]={anchor+offset-16,true,uint16_t(pc==0x08063b52?(r[1]|(r[0]&255)|0x300):(r[1]|r[0])),
        uint16_t(0x8000|((read(0x030069d2,2)&31)<<9)|((a1-16)&511)),
        uint16_t(read(a+4,2)),slot};
    return true;
}

// 0809D49E writes the owner's third component, including real IWRAM mirrors
// used by the guest when its allocation field is FFFF. Normalize only this
// observed write address, never the global memory reader.
template<class Reader>
bool observe_offset_affine(Reader read,const uint32_t* r,StagingPositions& out) {
    const unsigned sprite=r[10];
    if(sprite<0x03000000 || sprite>0x03007f88 || r[7]!=sprite+0x76)return false;
    const unsigned expected=0x03005a10+(read(sprite+0x34,2)>>2)*8;
    if(r[5]!=expected)return false;
    const unsigned a=0x03000000|(expected&0x7fff);
    if(a<0x03005a00 || a>=0x03006200 || (a&7))return false;
    const unsigned slot=(a-0x03005a00)/8;
    const int x=int16_t(read(sprite+0x20,2))-8;
    out[slot]={x,true,uint16_t(((r[4]-r[0])&255)|(r[9]&0xfe00)|0x100),
        uint16_t((read(a+2,2)&0xc000)|(read(0x030069d2,2)<<9)|(unsigned(x)&511)),
        uint16_t(read(a+4,2)),slot};
    return true;
}

}
