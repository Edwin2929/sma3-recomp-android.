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

// 08075E7A is sprite 13C's common return, after 08075E84 draws both
// components, 080755E4 assigns their matrices and 0805EDA0 adjusts priority.
// The drawing/matrix order differs while paused. Observe their final triples
// here, deriving full signed coordinates from the same anchors and ROM table
// as 0808CE0C; never reconstruct X from its nine-bit OAM representation.
template<class Reader>
bool observe_rotating_pair(Reader read,const uint32_t* r,StagingPositions& out) {
    const unsigned sprite=r[4],base=0x03002460;
    if(sprite<base || sprite>=base+24*0xb0 || (sprite-base)%0xb0)return false;
    const unsigned allocation=read(sprite+0x34,2);
    if((allocation&3) || allocation>254*4)return false;
    const unsigned first=allocation/4;
    // A failed observation of either owned component invalidates the pair.
    // Do not leave an earlier observation attached to a reused allocation.
    out[first]={};out[first+1]={};
    if(read(0x0300702c,4)!=base || read(sprite+0x32,2)!=0x13c ||
       read(sprite+0x24,2)!=8 || (read(sprite+0x2a,2)>>11)!=2)return false;
    const unsigned next_matrix=read(0x030069d2,2);
    if(next_matrix<2 || next_matrix>32)return false;
    const unsigned angle=read(sprite+0x7a,2)&0x1fe;
    const int dx=(8*int(int16_t(read(0x081af6ce + angle,2)))+128)>>8;
    const int dy=(8*int(int16_t(read(0x081af74e + angle,2)))+128)>>8;
    const int wx=uint16_t(read(sprite+0x5a,2)),wy=uint16_t(read(sprite+0x5c,2));
    const int cx=uint16_t(read(0x030069d4,2)),cy=uint16_t(read(0x030069dc,2));
    const int x[2]={int16_t(uint16_t(wx+dx-40-cx)),int16_t(uint16_t(wx-dx+8-cx))};
    const int y=int16_t(uint16_t(wy+dy-cy-16));
    const unsigned stored_y=y>=-32 && y<160 ? unsigned(y)&255 : 160;
    std::array<ObjectPosition,2> observed{};
    for(unsigned i=0;i<2;++i) {
        const unsigned a=0x03005a00+(first+i)*8;
        const auto a0=uint16_t(read(a,2)),a1=uint16_t(read(a+2,2)),a2=uint16_t(read(a+4,2));
        const unsigned expected_a1=0x8000|((next_matrix-2+i)<<9)|(unsigned(x[i])&511);
        if((a0&0x3ff)!=(0x100|stored_y) || a1!=expected_a1)return false;
        observed[i]={x[i],true,a0,a1,a2,first+i};
    }
    // 08075F6C copies the first tile/palette/priority to the second piece;
    // the post-draw priority routine treats both allocated pieces equally.
    if(observed[0].a2!=observed[1].a2)return false;
    out[first]=observed[0];out[first+1]=observed[1];
    return true;
}
// 080DC02A returns from 080DBF6C after the bridge cloud's shrinking sprite
// and affine matrix have been written. Only the two reviewed phase-2 callers
// preserve the owning cloud in saved r6. Other users of this shared routine
// retain their existing clipping. All reads observe guest memory unchanged.
template<class Reader>
bool observe_bridge_cloud(Reader read,const uint32_t* r,StagingPositions& out) {
    const unsigned sp=r[13],base=0x03002460;
    if(sp<0x03000000 || sp>0x03007fec || (sp&3))return false;
    const unsigned caller=read(sp+16,4);
    if(caller!=0x080dc487 && caller!=0x080dc495)return false;
    const unsigned sprite=read(sp+8,4);
    if(sprite<base || sprite>=base+24*0xb0 || (sprite-base)%0xb0)return false;
    if(read(0x0300702c,4)!=base || read(sprite+0x32,2)!=0xbb)return false;
    const unsigned allocation=read(sprite+0x34,2);
    if((allocation&3) || allocation>=1024)return false;
    const unsigned first=allocation/4,a=0x03005a00+first*8;
    out[first]={};
    if(read(sprite+0x24,2)!=8 || read(sprite+0x6a,2)!=2 ||
       read(sprite+0x94,1)==255 || read(sprite+0x2a,2)!=0x0804)return false;

    const unsigned next_matrix=read(0x030069d2,2);
    if(next_matrix<1 || next_matrix>32)return false;
    const unsigned matrix=next_matrix-1,matrix_address=0x03005a00+matrix*32;
    // These registers distinguish a completed write from the early exits at
    // 080DBFAA/080DBFB2, which share the same return label.
    if(r[4]!=matrix_address || r[5]!=0x03007030 || r[6]!=0x030069d2 ||
       r[0]!=next_matrix)return false;
    const unsigned scale=read(sprite+0x7a,2);
    if(scale<256 || scale>879)return false;
    const unsigned reciprocal=read(0x081af2cc+(scale<496?scale:496)*2,2);
    if(r[7]!=reciprocal || read(matrix_address+6,2)!=reciprocal ||
       read(matrix_address+14,2)!=0 || read(matrix_address+22,2)!=0 ||
       read(matrix_address+30,2)!=reciprocal)return false;

    // The single 16x16 phase-2 component uses the signed screen anchor.
    // Require its complete, unmasked staging coordinates to agree as well;
    // two positions separated by 512 pixels must never be interchangeable.
    const int x=int16_t(read(sprite+0x20,2)),y=int16_t(read(sprite+0x22,2));
    const unsigned metadata=0x0202c8b0+first*4;
    if(int16_t(read(metadata,2))!=x || int16_t(read(metadata+2,2))!=y)return false;
    // Thumb subtracts before OR and truncates only at STRH. Preserve that
    // behavior even when a coordinate underflows its hardware field.
    const auto a0=uint16_t(((unsigned(y)&255)-8u) | 0x100u);
    const auto a1=uint16_t(((unsigned(x)&511)-8u) | 0xffff8000u | (matrix<<9));
    if(read(a,2)!=a0 || read(a+2,2)!=a1)return false;
    out[first]={x-8,true,a0,a1,uint16_t(read(a+4,2)),first};
    return true;
}

// 080784CA follows the completed matrix write in 08078370. For sprites
// 076/077, the third component's enlarged affine box moves its origin by -8
// on each axis without updating the generic coordinate metadata. Retain the
// generic emitter's full X and verify the exact reviewed attribute transform.
template<class Reader>
bool observe_scaling_triplet(Reader read,const uint32_t* r,
        const StagingPositions& emitted,StagingPositions& out) {
    const unsigned sprite=r[7],base=0x03002460;
    if(sprite<base || sprite>=base+24*0xb0 || (sprite-base)%0xb0 ||
       read(0x0300702c,4)!=base)return false;
    const unsigned id=read(sprite+0x32,2);
    if(id!=0x76 && id!=0x77)return false;
    const unsigned allocation=read(sprite+0x34,2);
    if((allocation&3) || allocation>253*4)return false;
    const unsigned slot=allocation/4+2,a=0x03005a00+slot*8;
    out[slot]={};
    if(read(sprite+0x24,2)!=8 || (read(sprite+0x2a,2)>>11)!=3 ||
       read(sprite+0x94,1)>=128 || read(sprite+0x62,2)!=0 ||
       r[4]!=0x030069d2 || r[0]!=0x080784cb)return false;
    const auto& original=emitted[slot];
    const int x=int16_t(read(sprite+0x20,2)),y=int16_t(read(sprite+0x22,2));
    if(!original.valid || original.source!=slot || original.x!=x ||
       (original.a1&511)!=(unsigned(x)&511) || (original.a0&255)!=(unsigned(y)&255) ||
       int16_t(read(0x0202c8b0+slot*4,2))!=x ||
       int16_t(read(0x0202c8b2+slot*4,2))!=y)return false;
    const unsigned matrix=read(0x030069d2,2),scale=read(sprite+0x76,2),phase=read(sprite+0x72,2);
    if(matrix>=32 || scale>=512 || phase>=4)return false;
    const unsigned reciprocal=read(0x081af2cc+scale*2,2);
    const unsigned flips=read(0x08174b58+phase,1);
    const auto sx=uint16_t(flips&0x40?0u-reciprocal:reciprocal);
    const auto sy=uint16_t(flips&0x80?0u-reciprocal:reciprocal);
    const unsigned m=0x03005a00+matrix*32;
    if(read(0x030021a8,2)!=sx || read(0x030021aa,2)!=sy || read(0x030021ac,2)!=0 ||
       read(m+6,2)!=sx || read(m+14,2)!=0 || read(m+22,2)!=0 || read(m+30,2)!=sy)return false;
    auto p=original;
    if(uint16_t(y+40)>207) {
        // The guest disables this component before changing its X or tile.
        p.a0=0x0200;
    } else {
        const unsigned graphic=read(sprite+0x3c,2);
        if(graphic>=32)return false; // The 32 dynamic-slot tile indices.
        p.x-=8;
        p.a0=uint16_t((p.a0&0xfc00)|0x300|((unsigned(y)-8)&255));
        p.a1=uint16_t((p.a1&0x3e00)|0x4000|((unsigned(x)-8)&511));
        p.a2=uint16_t((p.a2&0xfc00)|read(0x08171e88+graphic*2,2));
    }
    if(read(a,2)!=p.a0 || read(a+2,2)!=p.a1 || read(a+4,2)!=p.a2)return false;
    out[slot]=p;
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
    const auto a0=uint16_t(pc==0x08063b52?(r[1]|(r[0]&255)|0x300):(r[1]|r[0]));
    const auto final_a1=uint16_t(0x8000|((read(0x030069d2,2)&31)<<9)|((a1-16)&511));
    // This routine can modify a cleared, unemitted slot during entry. Its zero
    // X is a staging sentinel, not a world coordinate. Preserve native clipping
    // only for this producer and these exact final attributes.
    if(read(a,2)==160 && a1==0) {
        out[slot]={0,false,a0,final_a1,uint16_t(read(a+4,2)),slot,true};
        return true;
    }
    const int anchor=int16_t(read(sprite+0x20,2));
    const int offset=int(((a1-(unsigned(anchor)&511)+256)&511))-256;
    if(offset< -64 || offset>64)return false;
    out[slot]={anchor+offset-16,true,a0,final_a1,
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
