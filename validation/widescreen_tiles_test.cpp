#include "../src/widescreen_tiles.h"
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>

int main() {
    std::map<uint32_t,uint8_t> bytes;
    auto put=[&](uint32_t a,uint32_t v,unsigned n) {
        for(unsigned i=0;i<n;++i)bytes[a+i]=uint8_t(v>>(8*i));
    };
    auto read=[&](uint32_t a,unsigned n) {
        uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(bytes[a+i])<<(8*i);return v;
    };
    auto require=[](bool ok) {if(!ok)throw std::runtime_error("widescreen tile regression");};
    put(0x03007010,0x0200000c,4);
    // Screen (2,3) uses index 7, with scrolling prevention bit set. The tile
    // lives at cell (4,5) in that screen, independent of its hardware ring slot.
    put(0x0201b800+3*16+2,0x87,1);
    put(0x0200000c+7*512+(5*16+4)*2,0x1234,2);
    put(0x081bad20+0x12*4,0x08200000,4);
    put(0x081bc444+0x12*4,0x08210000,4);
    put(0x08210000+0x34,0x0a,1); // only top-left and bottom-left on layer 0
    for(unsigned q=0;q<4;++q)put(0x08200000+0x34*8+q*2,0x2100+q,2);
    unsigned checks=0;
    for(int layer:{0,1})for(int q=0;q<4;++q) {
        uint16_t out=0;
        require(sma3::wide::foreground_tile(read,2*256+4*16+(q%2)*8,
            3*256+5*16+(q/2)*8,layer,&out));
        require(out==(layer==0 && q%2 ? 0x60ff : 0x2100+q));++checks;
    }
    for(auto xy:{std::pair<int,int>{-1,0},{4096,0},{0,-1},{0,2048}}) {
        uint16_t out=0;require(sma3::wide::foreground_tile(read,xy.first,xy.second,1,&out));
        require(out==0x60ff);++checks;
    }
    uint16_t out=0;
    require(!sma3::wide::foreground_tile(read,0,0,2,&out));
    require(!sma3::wide::foreground_tile(read,0,0,1,nullptr));
    put(0x03007010,0,4);require(!sma3::wide::foreground_tile(read,0,0,1,&out));
    for(unsigned flags:{0u,0x400u,0x800u,0xc00u}) {
        // Far-edge sample wraps to the final metatile of a 512x512 authored image.
        put(0x0201bc00+(31*32+31)*2,0x2000|flags,2);
        require(sma3::wide::background_tile(read,-1,-1,2,32,&out));
        require(out==(0x2000|flags)+((flags&0x400)?0:1)+((flags&0x800)?0:16));++checks;
    }
    require(!sma3::wide::background_tile(read,0,0,1,32,&out));
    require(!sma3::wide::background_tile(read,0,0,2,0,&out));
    require(!sma3::wide::background_tile(read,0,0,2,129,&out));
    std::cout<<"PASS: "<<checks<<" map/flip/boundary cases plus invalid inputs\n";
}
