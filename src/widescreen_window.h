#pragma once
#include "widescreen_strips.h"

namespace sma3::wide {
// 0805EE4E copies a completed group to the separate OBJ-window buffer.
// Observe the complete loop at entry: generated back edges do not re-enter
// the function hook. The copies retain each source's full X, not its nine
// hardware bits. All guest attributes are checked again at submission.
template<class Reader>
bool observe_window_copies(Reader read,const uint32_t* r,
        const StagingPositions& strips,const StagingPositions& emitted,
        ObjectPositions& copies) {
    constexpr unsigned staging=0x03005a00,buffer=0x0202badc;
    const unsigned src=r[4],dst=r[3],count=r[5];
    if(dst<buffer || dst>=buffer+128*8 || (dst-buffer)%8 ||
       count==0 || count>31)return false;
    const unsigned first=(dst-buffer)/8;
    if(count>copies.size()-first)return false;
    for(unsigned i=0;i<count;++i)copies[first+i]={};
    if(src<staging || src>=staging+256*8 || (src-staging)%8)return false;
    const unsigned source=(src-staging)/8;
    if(count>256-source || r[6]!=0x1890 || r[7]!=0x0300702c ||
       r[8]!=0xf3ff || r[12]!=0x800 ||
       read(0x0300702c,4)!=0x03002460 || read(0x03007240,4)!=0x0300220c ||
       read(0x03004ba6,2)!=0x1c || read(0x03006d68,1)!=0 ||
       read(0x03003cf0,1)!=first)return false;
    for(unsigned i=0;i<count;++i) {
        const unsigned slot=source+i,a=src+i*8;
        const auto a0=uint16_t(read(a,2)),a1=uint16_t(read(a+2,2)),a2=uint16_t(read(a+4,2));
        const int x=int16_t(read(0x0202c8b0+slot*4,2));
        const int y=int16_t(read(0x0202c8b2+slot*4,2));
        ObjectPosition p{x,(unsigned(x)&511)==(a1&511) && (unsigned(y)&255)==(a0&255),
                         a0,a1,a2,slot};
        const auto exact=[&](const ObjectPosition& q) {
            return q.valid && q.source==slot && q.a0==a0 && q.a1==a1 && q.a2==a2;
        };
        if(exact(strips[slot]))p=strips[slot];
        if(!p.valid && exact(emitted[slot]))p=emitted[slot];
        p.a0|=0x0800;
        p.a2&=0xf3ff;
        copies[first+i]=p;
    }
    return true;
}

// VBlank at 08000D42..08000D7A prepends the window buffer and then copies
// the normal compacted buffer. Reproduce its capacity clamp without mixing
// submissions or relaxing matches_display. Unknown or mutated copies cannot
// authorize a presentation. A recognized copy with unknown X stays unresolved.
template<class Reader>
bool compose_window_submission(Reader read,const ObjectPositions& copies,
        ObjectPositions& normal) {
    if(read(0x03006af7,1)!=0)return true; // The guest's direct-copy branch.
    if(read(0x0300702c,4)!=0x03002460)return false;
    unsigned count=read(0x03003cf0,1);
    if(count==0)return true;
    if(read(0x03007240,4)!=0x0300220c || read(0x03004ba6,2)!=0x1c ||
       read(0x03006d68,1)!=0)return false;
    unsigned originals=0;
    for(unsigned source=0;source<256;++source)
        if((read(0x03005a00+source*8,2)&255)!=160)++originals;
    if(originals>128)return false;
    if(count>128-originals)count=128-originals;
    if(count==0)return true;
    ObjectPositions composed{};
    for(unsigned i=0;i<count;++i) {
        const auto& p=copies[i];const unsigned a=0x0202badc+i*8;
        if(p.source>=256 || p.a0!=read(a,2) || p.a1!=read(a+2,2) ||
           p.a2!=read(a+4,2))return false;
        composed[i]=p;
    }
    for(unsigned i=0;i<128-count;++i)composed[count+i]=normal[i];
    normal=composed;
    return true;
}
}
