#include "../src/widescreen_rotating.h"
#include "../src/widescreen_window.h"
#include <iostream>
#include <map>
#include <stdexcept>

using namespace sma3::wide;
namespace {
unsigned checks=0;
void require(bool ok,const char* label) {
    ++checks;
    if(!ok)throw std::runtime_error(label);
}
bool same(const ObjectPosition& a,const ObjectPosition& b) {
    return a.x==b.x && a.valid==b.valid && a.a0==b.a0 && a.a1==b.a1 &&
           a.a2==b.a2 && a.source==b.source && a.screen_space==b.screen_space;
}
struct Fixture {
    static constexpr unsigned sprite=0x03003170,first=60,slot=62;
    static constexpr unsigned address=0x03005a00+slot*8,metadata=0x0202c8b0+slot*4;
    std::map<unsigned,uint8_t> memory;
    std::array<uint32_t,16> r{};
    StagingPositions emitted{},out{};
    void put(unsigned a,uint32_t value,unsigned width=2) {
        for(unsigned i=0;i<width;++i)memory[a+i]=uint8_t(value>>(i*8));
    }
    uint32_t read(unsigned a,unsigned width) const {
        require(width==1 || width==2 || width==4,"unsupported read width");
        uint32_t value=0;
        for(unsigned i=0;i<width;++i) {
            const auto found=memory.find(a+i);
            if(found!=memory.end())value|=uint32_t(found->second)<<(i*8);
        }
        return value;
    }
    void matrix(unsigned index,unsigned scale,unsigned phase,unsigned reciprocal) {
        constexpr unsigned flips[]={0x00,0x80,0x40,0xc0};
        put(0x030069d2,index);put(sprite+0x76,scale);put(sprite+0x72,phase);
        put(0x081af2cc+scale*2,reciprocal);put(0x08174b58+phase,flips[phase],1);
        const uint16_t sx=uint16_t(flips[phase]&0x40?0u-reciprocal:reciprocal);
        const uint16_t sy=uint16_t(flips[phase]&0x80?0u-reciprocal:reciprocal);
        put(0x030021a8,sx);put(0x030021aa,sy);put(0x030021ac,0);
        const unsigned a=0x03005a00+index*32;
        put(a+6,sx);put(a+14,0);put(a+22,0);put(a+30,sy);
    }
    void coordinates(int x,int y,unsigned a0,unsigned a1) {
        put(sprite+0x20,uint16_t(x));put(sprite+0x22,uint16_t(y));
        put(metadata,uint16_t(x));put(metadata+2,uint16_t(y));
        emitted[slot]={x,true,uint16_t(unsigned(y)&255),uint16_t(0x4000|(unsigned(x)&511)),0x1850,slot};
        put(address,a0);put(address+2,a1);put(address+4,0x1994);
    }
    Fixture() {
        r[0]=0x080784cb;r[4]=0x030069d2;r[7]=sprite;
        put(0x0300702c,0x03002460,4);
        put(sprite+0x24,8);put(sprite+0x2a,0x1945);put(sprite+0x32,0x76);
        put(sprite+0x34,first*4);put(sprite+0x94,0,1);put(sprite+0x62,0);
        put(sprite+0x3c,4);put(0x08171e88+8,0x0194);
        matrix(0,256,0,256);coordinates(-24,49,0x0329,0x41e0);
        out[first]={100,true,1,2,3,first};out[first+1]={200,true,1,2,3,first+1};
        out[slot]={700,true,1,2,3,slot};out[100]={777,true,0x0123,0x8456,0x789a,100};
    }
    bool observe(unsigned owned=slot) {
        const auto before=memory;const auto regs=r;const auto generic=emitted,prior=out;
        const bool ok=observe_scaling_triplet([&](unsigned a,unsigned w){return read(a,w);},r.data(),emitted,out);
        require(memory==before && r==regs,"triplet observer changed guest memory or CPU");
        for(unsigned i=0;i<out.size();++i) {
            require(same(generic[i],emitted[i]),"triplet observer changed generic provenance");
            if(i!=owned)require(same(out[i],prior[i]),"triplet observer changed another component");
        }
        return ok;
    }
    void reject(bool known=true) {
        require(!observe(),"invalid triplet accepted");
        if(known)require(!out[slot].valid && out[slot].source==256,"failed triplet retained stale provenance");
        else require(out[slot].valid && out[slot].x==700,"unknown owner cleared unrelated provenance");
    }
};

void valid_triplets() {
    struct Sample { int x,y,full_x;unsigned a0,a1; };
    constexpr Sample samples[]={
        {-24,49,-32,0x0329,0x41e0},{488,49,480,0x0329,0x41e0},
        {0,0,-8,0x03f8,0x41f8},{272,155,264,0x0393,0x4108},
        {136,-24,128,0x03e0,0x4080}};
    for(const auto& s:samples)for(unsigned id:{0x76u,0x77u}) {
        Fixture f;f.put(Fixture::sprite+0x32,id);f.coordinates(s.x,s.y,s.a0,s.a1);
        require(f.observe(),"valid triplet rejected");
        const auto& p=f.out[Fixture::slot];
        require(p.valid && !p.screen_space && p.source==Fixture::slot,"triplet provenance missing");
        require(p.x==s.full_x && p.a0==s.a0 && p.a1==s.a1 && p.a2==0x1994,"triplet final attributes/full X incorrect");
    }
    // The matrix helper changes coefficients at the current index; the third
    // component retains its own existing A1 matrix bits through FC00/3E00 masks.
    for(unsigned index:{0u,7u,31u})for(unsigned phase:{0u,1u,2u,3u}) {
        Fixture f;f.matrix(index,256,phase,256);
        require(f.observe() && f.out[Fixture::slot].a1==0x41e0,"matrix/flip phase rejected");
    }
    for(const auto& [scale,reciprocal]:{std::pair{0u,0xffffu},std::pair{128u,512u},std::pair{256u,256u},std::pair{511u,128u}}) {
        Fixture f;f.matrix(0,scale,0,reciprocal);require(f.observe(),"scale boundary rejected");
    }
    // Clipping writes only A0. A1/A2 and their already-proven full X survive.
    for(int y:{-41,168,190}) {
        Fixture f;f.coordinates(-24,y,0x0200,0x41e8);f.put(Fixture::address+4,0x1850);
        require(f.observe() && f.out[Fixture::slot].x==-24 && f.out[Fixture::slot].a0==0x0200 &&
                f.out[Fixture::slot].a1==0x41e8 && f.out[Fixture::slot].a2==0x1850,"clipped component changed untouched fields");
    }
    {Fixture f;f.put(Fixture::sprite+0x3c,31);f.put(0x08171e88+62,0x025e);f.put(Fixture::address+4,0x1a5e);
     require(f.observe() && f.out[Fixture::slot].a2==0x1a5e,"last dynamic tile rejected");}
    Fixture f;require(f.observe(),"display setup failed");ObjectPositions frame{};frame[0]=f.out[Fixture::slot];
    for(unsigned i=0;i<frame.size();++i) {
        f.put(0x07000000+i*8,frame[i].a0);f.put(0x07000002+i*8,frame[i].a1);f.put(0x07000004+i*8,frame[i].a2);
    }
    auto read=[&](unsigned a,unsigned w){return f.read(a,w);};
    require(matches_display(read,frame),"exact triplet display rejected");
    for(unsigned offset:{0u,2u,4u}) {
        const unsigned a=0x07000000+offset,old=f.read(a,2);
        f.put(a,old^0x400);require(!matches_display(read,frame),"changed display triple accepted");f.put(a,old);
    }
}

void window_pipeline() {
    Fixture f;require(f.observe(),"window pipeline setup failed");
    f.put(0x03007240,0x0300220c,4);f.put(0x03004ba6,0x1c);f.put(0x03006d68,0,1);f.put(0x03003cf0,0,1);
    std::array<uint32_t,16> r{};
    r[3]=0x0202badc;r[4]=Fixture::address;r[5]=1;r[6]=0x1890;r[7]=0x0300702c;r[8]=0xf3ff;r[12]=0x0800;
    const auto before=f.memory;const auto registers=f.r,copy_registers=r;
    ObjectPositions copies{};
    require(observe_window_copies([&](unsigned a,unsigned w){return f.read(a,w);},r.data(),f.out,f.emitted,copies),"window rejected final triplet");
    require(copies[0].valid && copies[0].x==-32 && copies[0].source==Fixture::slot && copies[0].a0==0x0b29 &&
            copies[0].a1==0x41e0 && copies[0].a2==0x1194,"copy used stale metadata instead of final full X");
    require(f.memory==before && f.r==registers && r==copy_registers,"window pipeline changed guest state");
}

void rejected_triplets() {
    for(unsigned matrix:{32u,0xffffu}) {Fixture f;f.put(0x030069d2,matrix);f.reject();}
    for(unsigned scale:{512u,0xffffu}) {Fixture f;f.put(Fixture::sprite+0x76,scale);f.reject();}
    for(unsigned phase:{4u,0xffffu}) {Fixture f;f.put(Fixture::sprite+0x72,phase);f.reject();}
    for(unsigned address:{0x03005a06u,0x03005a0eu,0x03005a16u,0x03005a1eu,0x030021a8u,0x030021aau,0x030021acu}) {
        Fixture f;f.put(address,7);f.reject();
    }
    {Fixture f;f.put(0x081af2cc+512,255);f.reject();}
    for(unsigned graphic:{32u,0xffffu}) {Fixture f;f.put(Fixture::sprite+0x3c,graphic);f.reject();}
    for(unsigned offset:{0u,2u,4u}) {Fixture f;f.put(Fixture::address+offset,f.read(Fixture::address+offset,2)^1);f.reject();}
    for(unsigned owner:{0x0300245fu,0x030034e0u,Fixture::sprite+1,0xfffffff0u}) {Fixture f;f.r[7]=owner;f.reject(false);}
    for(unsigned id:{0x75u,0x78u,0x13cu,0xffffu}) {Fixture f;f.put(Fixture::sprite+0x32,id);f.reject(false);}
    for(unsigned allocation:{241u,1016u,0xffffu}) {Fixture f;f.put(Fixture::sprite+0x34,allocation);f.reject(false);}
    {Fixture f;f.put(0x0300702c,0x03002510,4);f.reject(false);}
    for(unsigned reg:{0u,4u}) {Fixture f;f.r[reg]^=1;f.reject();}
    for(unsigned status:{0u,5u}) {Fixture f;f.put(Fixture::sprite+0x24,status);f.reject();}
    {Fixture f;f.put(Fixture::sprite+0x2a,0x2145);f.reject();}
    {Fixture f;f.put(Fixture::sprite+0x62,1);f.reject();}
    for(unsigned status:{128u,255u}) {Fixture f;f.put(Fixture::sprite+0x94,status,1);f.reject();}
    {Fixture f;f.put(Fixture::sprite+0x20,488);f.reject();}
    {Fixture f;f.put(Fixture::sprite+0x22,305);f.reject();}
    {Fixture f;f.put(Fixture::metadata,488);f.reject();}
    {Fixture f;f.put(Fixture::metadata+2,305);f.reject();}
    {Fixture f;f.emitted[Fixture::slot].valid=false;f.reject();}
    {Fixture f;f.emitted[Fixture::slot].source=61;f.reject();}
    {Fixture f;f.emitted[Fixture::slot].x+=512;f.reject();}
    {Fixture f;f.emitted[Fixture::slot].a0^=1;f.reject();}
    {Fixture f;f.emitted[Fixture::slot].a1^=1;f.reject();}
    {Fixture f;f.emitted[Fixture::slot].a2^=0x1000;f.reject();}
    Fixture last;last.put(Fixture::sprite+0x34,253*4);
    last.emitted[255]=last.emitted[Fixture::slot];last.emitted[255].source=255;
    last.put(0x0202c8b0+255*4,uint16_t(-24));last.put(0x0202c8b2+255*4,49);
    last.put(0x03005a00+255*8,0x0329);last.put(0x03005a02+255*8,0x41e0);last.put(0x03005a04+255*8,0x1994);
    require(last.observe(255) && last.out[255].source==255 && last.out[255].x==-32,"last valid triplet rejected");
}
}
int main() {
    try {
        valid_triplets();window_pipeline();rejected_triplets();
        std::cout<<"PASS scaling_triplet: "<<checks<<" checks; exact XY resize, clipping, dynamic tiles, affine phases, full provenance and window composition\n";
    } catch(const std::exception& error) {
        std::cerr<<"FAIL scaling_triplet: "<<error.what()<<" after "<<checks<<" checks\n";return 1;
    }
}
