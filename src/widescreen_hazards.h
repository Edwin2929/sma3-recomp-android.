#pragma once
#include "widescreen_strips.h"

namespace sma3::wide {
// Sprite 064, variant zero only. Sub0808E144 rewrites sixteen components and
// leaves the seventeenth unchanged. Its clipped branches write only A0, so a
// return-only observation cannot prove the retained A1/A2. Keep one invocation
// pending between 0808E144 (before PUSH) and 0808E33C (before POP).
// The integration must also reset this host-only capture at compaction and
// state-epoch changes. No observation here writes guest memory or registers.
struct Hazard64Capture {
    struct Component { uint16_t a0=160,a1=0,a2=0,mx=0,my=0; };
    bool pending=false;
    unsigned owner=0,stack=0,first=256;
    uint16_t x=0,y=0,angle=0;
    std::array<Component,17> before{};
};

template<class Reader>
bool observe_hazard64(Reader read,uint32_t pc,const uint32_t* r,
                      StagingPositions& out,Hazard64Capture& capture) {
    constexpr unsigned base=0x03002460,normal=0x0300220c,count=17;
    auto owner_in_range=[](unsigned owner) {
        return owner>=base && owner<base+24*0xb0 && (owner-base)%0xb0==0;
    };
    auto clear_group=[&](unsigned first) {
        for(unsigned i=0;i<count;++i)out[first+i]={};
    };
    auto identity=[&](unsigned owner,unsigned first) {
        return owner_in_range(owner) && read(0x0300702c,4)==base &&
            read(0x03007240,4)==normal && read(owner+0x32,2)==0x64 &&
            read(owner+0x24,2)==8 && read(owner+0x2a,2)==0x8809 &&
            read(owner+0x34,2)==first*4 && read(owner+0x94,1)<128 &&
            read(owner+0x6a,2)==0 && read(owner+0x70,2)==0 &&
            (read(owner+4,4)&0x1000)!=0 && read(normal+0x2a1a,2)==0;
    };
    if(pc==0x0808e144) {
        // Even a rejected new entry consumes any older pending invocation.
        capture={};
        const unsigned owner=r[0];
        if(!owner_in_range(owner))return false;
        const unsigned allocation=read(owner+0x34,2);
        if((allocation&3) || allocation>(out.size()-count)*4)return false;
        const unsigned first=allocation/4;
        clear_group(first);
        if(!identity(owner,first) || r[4]!=owner || r[14]!=0x0808e37d ||
           (r[13]&3) || r[13]<0x03000020 || r[13]>0x03008000)return false;
        Hazard64Capture next;
        next.owner=owner;next.stack=r[13];next.first=first;
        next.x=uint16_t(read(owner+0x20,2));
        next.y=uint16_t(read(owner+0x22,2));
        next.angle=uint16_t(read(owner+0x72,2));
        for(unsigned i=0;i<count;++i) {
            const unsigned a=0x03005a00+(first+i)*8,m=0x0202c8b0+(first+i)*4;
            auto& b=next.before[i];
            b={uint16_t(read(a,2)),uint16_t(read(a+2,2)),uint16_t(read(a+4,2)),
               uint16_t(read(m,2)),uint16_t(read(m+2,2))};
            // Data0818B94C: eight (0,-8) offsets, then nine (4,4).
            if(b.mx!=uint16_t(next.x+(i<8?0:4)) ||
               b.my!=uint16_t(int(next.y)+(i<8?-8:4)))return false;
        }
        next.pending=true;capture=next;
        return true;
    }
    if(pc!=0x0808e33c)return false;
    const Hazard64Capture old=capture;
    capture={};
    if(!old.pending || old.first>out.size()-count)return false;
    clear_group(old.first);
    if(!identity(old.owner,old.first) || r[8]!=old.owner ||
       (old.stack&3) || old.stack<0x03000020 || old.stack>0x03008000 ||
       r[13]!=old.stack-32 || read(r[13]+12,4)!=old.owner ||
       read(r[13]+28,4)!=0x0808e37d ||
       read(old.owner+0x20,2)!=old.x || read(old.owner+0x22,2)!=old.y ||
       read(old.owner+0x72,2)!=old.angle)return false;

    // Sub0808D620/690: radius 40, signed table coefficients, then ASR; the
    // outer small ring doubles the already-rounded inner ring, not its input.
    if(read(0x0817602a,1)!=16 || read(0x0817602b,1)!=16 ||
       read(0x0817602e,1)!=40 || read(0x081af2d2,2)!=21845)return false;
    const unsigned angle=(old.angle>>8)*2;
    const int cosine=int16_t(read(0x081af6ce + angle,2));
    const int sine=int16_t(read(0x081af74e + angle,2));
    if(cosine< -256 || cosine>256 || sine< -256 || sine>256)return false;
    const int c=(cosine*40)>>8,s=(sine*40)>>8;
    const int inner_c=(c*21845)>>16,inner_s=(s*21845)>>16;
    const unsigned center=uint16_t(old.x-8);
    // Completed second Sub0808DFF0 call. Early returns and the other variant
    // share 0808E33C but do not have this complete register/scratch identity.
    if(r[0]!=0x0808e33d || r[14]!=0x0808e33d ||
       r[2]!=0x03005a00+(old.first+15)*8 || r[4]!=uint16_t(int(center)-c) ||
       r[5]!=0x030069f4 || r[6]!=center || r[7]!=4 ||
       r[9]!=0x4848 || r[10]!=0x4800 || r[12]!=old.y ||
       read(normal+0x2a12,2)!=16 || read(normal+0x2a16,2)!=16 ||
       read(0x03006a02,2)!=uint16_t(c) || read(0x03006a04,2)!=uint16_t(s) ||
       read(0x03006a06,2)!=uint16_t(2*inner_c) ||
       read(0x03006a08,2)!=uint16_t(2*inner_s) ||
       read(normal+0x2a2a,2)!=uint16_t(c) ||
       read(normal+0x2a2e,2)!=uint16_t(s))return false;

    std::array<ObjectPosition,count> observed{};
    auto component=[&](unsigned i,int x,unsigned a0,unsigned a1,unsigned a2,bool clipped) {
        const unsigned slot=old.first+i,a=0x03005a00+slot*8,m=0x0202c8b0+slot*4;
        const auto& b=old.before[i];
        if(read(m,2)!=b.mx || read(m+2,2)!=b.my || read(a,2)!=a0 ||
           read(a+2,2)!=a1 || read(a+4,2)!=a2)return false;
        // Exact-attribute native-clipping sentinel: intentionally suppressed,
        // no usable full X. Re-enabling/reusing the source must not inherit it.
        observed[i]={clipped?0:x,!clipped,uint16_t(a0),uint16_t(a1),uint16_t(a2),slot,clipped};
        return true;
    };
    const int dx[4]={s,c,-s,-c},dy[4]={-c,s,c,-s};
    for(unsigned quadrant=0;quadrant<4;++quadrant) {
        const int x=int16_t(uint16_t(int(center)+dx[quadrant]));
        const int y=int16_t(uint16_t(int(old.y)+dy[quadrant]));
        const bool clipped=y< -16 || y>207;
        for(unsigned part=0;part<2;++part) {
            const unsigned i=quadrant*2+part;
            const auto& b=old.before[i];
            // The adjacent half is +16 from the first signed origin; do not
            // sign-wrap that second addition or recover X from packed bits.
            const int full_x=x+int(part)*16;
            const unsigned a0=clipped?160:unsigned(y)&255;
            const unsigned a1=clipped?b.a1:0x4000|(unsigned(full_x)&511);
            const unsigned a2=clipped?b.a2:0x4848+part*2;
            if(!component(i,full_x,a0,a1,a2,clipped))return false;
        }
    }
    for(unsigned ring=0;ring<2;++ring) {
        const int rc=inner_c*int(ring+1),rs=inner_s*int(ring+1);
        const int rx[4]={rs,rc,-rs,-rc},ry[4]={-rc,rs,rc,-rs};
        for(unsigned q=0;q<4;++q) {
            const unsigned i=8+ring*4+q;
            const auto& b=old.before[i];
            const int x=int16_t(uint16_t(int(old.x)+4+rx[q]));
            const int y=int16_t(uint16_t(int(old.y)+4+ry[q]));
            const bool clipped=y< -32 || y>191;
            const unsigned a0=clipped?0x200:unsigned(y)&255;
            const unsigned a1=clipped?b.a1:unsigned(x)&511;
            const unsigned a2=clipped?b.a2:(b.a2&0xfc00)|0x16d;
            if(!component(i,x,a0,a1,a2,clipped))return false;
        }
    }
    const auto& last=old.before[16];
    const bool clipped=(last.a0&255)==160;
    if(!clipped && ((last.a0&255)!=(last.my&255) || (last.a1&511)!=(last.mx&511)))return false;
    if(!component(16,int16_t(last.mx),last.a0,last.a1,last.a2,clipped))return false;
    // No partial group is published if any final triple or provenance differs.
    for(unsigned i=0;i<count;++i)out[old.first+i]=observed[i];
    return true;
}
}
