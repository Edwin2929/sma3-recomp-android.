#pragma once
#include <array>
#include <cstdint>

namespace sma3::wide {
struct ObjectPosition { int x=0; bool valid=false; uint16_t a0=160,a1=0,a2=0; unsigned source=256; };
using ObjectPositions=std::array<ObjectPosition,128>;

// Sub080004A0 compacts 256 staging slots into packed OAM by skipping Y=160.
// Metadata is indexed by the ORIGINAL staging slot, not the packed OAM slot.
// Require both the actual displayed attributes and metadata coordinates to
// match. A mismatched/incomplete frame remains unresolved, never guessed.
template<class Reader>
unsigned resolve_object_positions(Reader read, ObjectPositions& output, bool check_display=true) {
    output={};
    unsigned packed=0,matched=0;
    for(unsigned source=0;source<256 && packed<output.size();++source) {
        const unsigned staging=0x03005a00+source*8;
        const unsigned a0=read(staging,2);
        if ((a0&255)==160) continue;
        const unsigned a1=read(staging+2,2), a2=read(staging+4,2);
        const unsigned displayed=0x07000000+packed*8;
        const int x=int16_t(read(0x0202c8b0+source*4,2));
        const int y=int16_t(read(0x0202c8b2+source*4,2));
        output[packed]={x,false,uint16_t(a0),uint16_t(a1),uint16_t(a2)};
        if ((!check_display || (a0==read(displayed,2) && a1==read(displayed+2,2) && a2==read(displayed+4,2)))
            && (unsigned(x)&511)==(a1&511) && (unsigned(y)&255)==(a0&255)) {
            output[packed]={x,true,uint16_t(a0),uint16_t(a1),uint16_t(a2)};++matched;
        }
        output[packed].source=source;
        ++packed;
    }
    return matched;
}

// Choose one coherent submission, not a mixture of matching sprites from
// different frames. VBlank can display the previous completed submission.
template<class Reader>
bool matches_display(Reader read, const ObjectPositions& snapshot) {
    for(unsigned i=0;i<snapshot.size();++i) {
        const unsigned a=0x07000000+i*8;
        const auto& p=snapshot[i];
        const auto a0=read(a,2);
        if((p.a0&255)==160 && (a0&255)==160) continue;
        if(p.a0!=a0 || p.a1!=read(a+2,2) || p.a2!=read(a+4,2)) return false;
    }
    return true;
}
}
