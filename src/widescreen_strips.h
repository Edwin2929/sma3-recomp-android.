#pragma once
#include "widescreen_objects.h"
namespace sma3::wide {
using StagingPositions=std::array<ObjectPosition,256>;
// Both single-object affine helpers preserve packed X and use the owning
// sprite's signed screen anchor (offset 0x20). Bound the component offset;
// never infer the sign from the nine hardware bits alone.
template<class Reader>
bool observe_sprite_affine(Reader read,uint32_t sprite,StagingPositions& out) {
    if(!((sprite>=0x03000000 && sprite<=0x03007f60) ||
         (sprite>=0x02000000 && sprite<=0x0203ff60)))return false;
    if(read(sprite+0x94,1)==255)return false;
    const unsigned allocation=read(sprite+0x34,2);
    if((allocation&3) || allocation>=1024)return false;
    const unsigned slot=allocation/4,address=0x03005a00+slot*8;
    out[slot]={};
    const unsigned a0=read(address,2),a1=read(address+2,2),a2=read(address+4,2);
    const int anchor=int16_t(read(sprite+0x20,2));
    const int offset=int(((a1-(unsigned(anchor)&511)+256)&511))-256;
    if(offset< -64 || offset>64)return false;
    out[slot]={anchor+offset,true,uint16_t(a0|0x100),
        uint16_t((a1&511)|0x8000|((read(0x030069d2,2)&31)<<9)),uint16_t(a2),slot};
    return true;
}
// Sub080D8CB4 adjusts Y after the affine helper. Reproduce only that
// documented change, retaining exact matching of all attributes at submission.
template<class Reader>
bool observe_sprite_bob(Reader read,uint32_t sprite,StagingPositions& out) {
    if(!((sprite>=0x03000000 && sprite<=0x03007f60) ||
         (sprite>=0x02000000 && sprite<=0x0203ff60)))return false;
    if(read(sprite+0x94,1)==255)return false;
    const unsigned allocation=read(sprite+0x34,2);
    if((allocation&3) || allocation>=1024)return false;
    const unsigned slot=allocation/4,address=0x03005a00+slot*8;
    auto& p=out[slot];
    if(!p.valid || p.a0!=read(address,2) || p.a1!=read(address+2,2) || p.a2!=read(address+4,2)) {
        p={};return false;
    }
    const unsigned scale=read(sprite+0x76,2);
    // This animation clamps its scale to 0..511 (Sub080D9054).
    if(scale>=512) {p={};return false;}
    const int reciprocal=int16_t(read(0x081af2cc+scale*2,2));
    const int scaled=(reciprocal*2048+16384)>>16;
    const int delta=2*scaled-8;
    p.a0=uint16_t((p.a0&0xff00)|((int(p.a0)+delta)&255));
    return true;
}
template<class Reader>
bool observe_generic(Reader, const uint32_t* r,StagingPositions& out) {
    const uint32_t address=r[7];
    if(address<0x03005a00 || address>=0x03006200 || (address&7)) return false;
    const unsigned slot=(address-0x03005a00)/8;
    out[slot]={};
    const int x=int32_t(r[9]),y=int32_t(r[8]);
    if(x< -64 || x>319 || y< -32 || y>191) return false;
    const uint32_t attributes=r[12];
    const uint16_t a1=uint16_t(((r[0]&6)<<13)|((attributes>>14)<<12)|(unsigned(x)&511));
    const uint16_t a2=uint16_t(((attributes>>9)&7)<<12 |
        (((((attributes>>12)&3)^3)+1)&3)<<10 | (r[5]&1023));
    out[slot]={x,true,uint16_t(y&255),a1,a2,slot};
    return true;
}
template<class Reader>
bool observe_yoshi(Reader read,StagingPositions& out) {
    constexpr uint32_t player=0x03006d80;
    const unsigned anim=read(player+0x3c,2);
    if(anim>=516) return false;
    unsigned first=read(player+0x94,2)>>2;
    if(read(player+0x32,2)==0) first+=7;
    const unsigned count=read(0x081ac028+anim,1);
    if(first>=256 || count>256-first) return false;
    const uint32_t table=0x081ac634+read(0x081ac22c+anim*2,2);
    if(uint64_t(table)+count*4>0x08400000) return false;
    const uint32_t palette=((((read(player+0xa2,2)<<6)^0xc00)+0x400)&0xc00)|
        ((read(player+0xa0,2)<<11)&0xf000);
    const int anchor=int16_t(read(player+0x2c,2));
    for(unsigned i=0;i<count;++i) {
        out[first+i]={};
        const uint32_t t=table+i*4,flags=read(t,1);
        int dx=int8_t(read(t+1,1));
        unsigned flip=((flags>>2)&0x30)<<8, size=((flags<<1)&0x40)<<8;
        if(!(flags&8) && read(player+0x42,2)==0) {flip^=0x1000;dx=-dx;if(size==0)dx+=8;}
        const int x=anchor+dx;
        unsigned y=(int8_t(read(t+2,1))+read(player+0x2e,2))&255;
        if(uint32_t(x+64)>303)y=160;
        out[first+i]={x,true,uint16_t(y),uint16_t((unsigned(x)&511)|flip|size),uint16_t((i*2)|palette),first+i};
    }
    return true;
}
template<class Reader>
bool observe_yoshi_affine(Reader read,uint32_t address,StagingPositions& out) {
    if(address<0x03005a00 || address>=0x03006200 || (address&7))return false;
    auto& p=out[(address-0x03005a00)/8];
    if(!p.valid || p.a0!=read(address,2) || p.a1!=read(address+2,2) || p.a2!=read(address+4,2)) {
        p={};return false;
    }
    const unsigned anim=read(0x03006dbc,2);
    if((anim==0x198 || anim==0x164) && read(0x03006e3a,2)==2) {
        p.x-=16;p.a0=uint16_t((p.a0&0xff00)|((p.a0-16)&255)|0x300);
    } else p.a0=uint16_t((p.a0&~0x300)|0x100);
    p.a1=uint16_t((unsigned(p.x)&511)|0x8000|((read(0x030069d2,2)&31)<<9));
    return true;
}
// Read-only observations of the two player strip emitters. Match every
// resulting attribute again at compaction; predictions alone never authorize X.
template<class Reader>
bool observe_strip(Reader read,uint32_t pc,const uint32_t* r,StagingPositions& out) {
    if(pc!=0x0804211c && pc!=0x080421a8) return false;
    const unsigned first=r[1],end=r[2];
    if(first>=end || end>out.size()) return false;
    const uint32_t sp=r[13],table=r[0];
    if(sp<0x03000000 || sp>0x03007ff0 || table<0x08000000 ||
       uint64_t(table)+3*(end-first)>0x08400000) return false;
    const uint32_t v0=read(sp,4),v1=read(sp+4,4),v2=read(sp+8,4),v3=read(sp+12,4);
    uint32_t running=pc==0x0804211c ? v0 : v1;
    for(unsigned slot=first;slot<end;++slot) {
        const uint32_t t=table+3*(slot-first),b0=read(t,1),b1=read(t+1,1),b2=read(t+2,1);
        uint32_t x,y;
        if(pc==0x0804211c) { running+=v3;x=running+v2;y=b0+v1; }
        else {
            running+=8;y=running;
            x=uint32_t(int32_t(int8_t(b0)));
            if(int32_t(v2-1)<0) x=0u-x;
            x+=v0+v3;
        }
        out[slot]={int32_t(x),true,uint16_t(y&255),
            uint16_t(((b2<<6)&0x3000)|(x&511)),
            uint16_t(b1|r[3]|((b2<<11)&0xf000)),slot};
    }
    return true;
}
}
