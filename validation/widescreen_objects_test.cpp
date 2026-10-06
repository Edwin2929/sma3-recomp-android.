#include "../src/widescreen_objects.h"
#include <iostream>
#include <map>
#include <stdexcept>

int main() {
    std::map<unsigned,uint16_t> memory;
    auto read=[&](unsigned a,unsigned n) {if(n!=2)throw std::runtime_error("width");return unsigned(memory[a]);};
    auto require=[](bool b) {if(!b)throw std::runtime_error("object mapping regression");};
    for(unsigned i=0;i<256;++i)memory[0x03005a00+i*8]=160;
    auto object=[&](unsigned source,unsigned packed,int x,int y,unsigned tile) {
        unsigned a=0x03005a00+source*8, b=0x07000000+packed*8;
        memory[a]=memory[b]=uint16_t(y&255);
        memory[a+2]=memory[b+2]=uint16_t(0x4000|(x&511));
        memory[a+4]=memory[b+4]=uint16_t(tile);
        memory[0x0202c8b0+source*4]=uint16_t(x);
        memory[0x0202c8b2+source*4]=uint16_t(y);
    };
    object(5,0,272,40,17);
    object(12,1,-240,40,17); // same nine X bits as +272, opposite world position
    object(255,2,-32,80,23);
    sma3::wide::ObjectPositions output;
    require(sma3::wide::resolve_object_positions(read,output)==3);
    require(output[0].valid && output[0].x==272);
    require(output[1].valid && output[1].x==-240);
    require(output[2].valid && output[2].x==-32);
    require(!output[3].valid);
    memory[0x07000004]=99;
    require(sma3::wide::resolve_object_positions(read,output)==2 && !output[0].valid);
    require(sma3::wide::resolve_object_positions(read,output,false)==3 && output[0].valid);
    memory[0x0202c8b2+12*4]=41;
    require(sma3::wide::resolve_object_positions(read,output)==1 && !output[1].valid);
    for(unsigned i=0;i<256;++i)memory[0x03005a00+i*8]=160;
    require(sma3::wide::resolve_object_positions(read,output)==0 && !output[2].valid);
    for(unsigned i=0;i<256;++i)object(i,i<128 ? i : 127,int(i),20,5);
    sma3::wide::resolve_object_positions(read,output); // output bounded to 128 entries
    std::cout<<"PASS: OAM compaction, signed X ambiguity, mismatches, reset and capacity\n";
}
