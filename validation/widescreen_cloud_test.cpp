#include "../src/widescreen_rotating.h"
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

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
    static constexpr unsigned sprite=0x03003430,sp=0x03007e00,first=37;
    static constexpr unsigned address=0x03005a00+first*8,metadata=0x0202c8b0+first*4;
    std::map<unsigned,uint8_t> memory;
    std::array<uint32_t,16> r{};
    StagingPositions out{};
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
    void matrix(unsigned index,unsigned reciprocal) {
        r[0]=index+1;r[4]=0x03005a00+index*32;r[5]=0x03007030;
        r[6]=0x030069d2;r[7]=reciprocal;
        put(0x030069d2,index+1);
        put(r[4]+6,reciprocal);put(r[4]+14,0);put(r[4]+22,0);put(r[4]+30,reciprocal);
    }
    Fixture() {
        r[13]=sp;put(sp+8,sprite,4);put(sp+16,0x080dc495,4);
        put(0x0300702c,0x03002460,4);
        put(sprite+0x24,8);put(sprite+0x2a,0x0804);put(sprite+0x32,0xbb);
        put(sprite+0x34,first*4);put(sprite+0x6a,2);put(sprite+0x94,2,1);
        put(sprite+0x20,136);put(sprite+0x22,133);
        put(metadata,136);put(metadata+2,133);
        put(sprite+0x7a,256);put(0x081af2cc+256*2,256);matrix(0,256);
        put(address,0x017d);put(address+2,0x8080);put(address+4,0x098c);
        out[first]={700,true,0x0123,0x8234,0x9456,first};
        out[100]={777,true,0x0123,0x8456,0x789a,100};
    }
    bool observe(unsigned owned=first) {
        const auto before=memory;const auto regs=r;const auto prior=out;
        const bool ok=observe_bridge_cloud([&](unsigned a,unsigned w){return read(a,w);},r.data(),out);
        require(memory==before && r==regs,"observer changed guest memory or CPU");
        for(unsigned i=0;i<out.size();++i)
            if(i!=owned)require(same(out[i],prior[i]),"observer changed unrelated allocation");
        return ok;
    }
    void reject(bool known=true) {
        require(!observe(),"invalid cloud accepted");
        if(known)require(!out[first].valid && out[first].source==256,"failed cloud retained stale provenance");
        else require(out[first].valid && out[first].x==700,"unknown owner cleared unrelated provenance");
    }
};

void valid_clouds() {
    // Literal ARM results, including two anchors with identical nine-bit X.
    // Underflow is deliberately not masked again after the guest subtraction.
    struct Sample { int x,y,full_x;unsigned a0,a1; };
    constexpr Sample samples[]={
        {136,133,128,0x017d,0x8080},{-376,133,-384,0x017d,0x8080},
        {280,133,272,0x017d,0x8110},{-24,133,-32,0x017d,0x81e0},
        {4,133,-4,0x017d,0xfffc},{0,4,-8,0xfffc,0xfff8},
        {136,-24,128,0x01e0,0x8080},{136,264,128,0x0100,0x8080},
        {32767,133,32759,0x017d,0x81f7},{-32768,133,-32776,0x017d,0xfff8}};
    for(const auto& s:samples)for(unsigned caller:{0x080dc487u,0x080dc495u}) {
        Fixture f;
        f.put(Fixture::sp+16,caller,4);
        f.put(Fixture::sprite+0x20,uint16_t(s.x));f.put(Fixture::sprite+0x22,uint16_t(s.y));
        f.put(Fixture::metadata,uint16_t(s.x));f.put(Fixture::metadata+2,uint16_t(s.y));
        f.put(Fixture::address,s.a0);f.put(Fixture::address+2,s.a1);
        require(f.observe(),"valid cloud rejected");
        const auto& p=f.out[Fixture::first];
        require(p.valid && !p.screen_space && p.source==Fixture::first,"cloud provenance missing");
        require(p.x==s.full_x && p.a0==s.a0 && p.a1==s.a1 && p.a2==0x098c,"cloud attributes/full X changed");
    }
    for(unsigned tile:{0x098cu,0x318cu,0xf98cu}) {
        Fixture f;f.put(Fixture::address+4,tile);
        require(f.observe() && f.out[Fixture::first].a2==tile,"final tile or priority not preserved");
    }
    for(const auto& [index,a1]:{std::pair{0u,0x8080u},std::pair{7u,0x8e80u},std::pair{31u,0xbe80u}}) {
        Fixture f;f.matrix(index,256);f.put(Fixture::address+2,a1);
        require(f.observe() && f.out[Fixture::first].a1==a1,"affine matrix index lost");
    }
    struct Scale { unsigned scale,index,reciprocal; };
    for(const auto& s: {Scale{256,256,256},Scale{495,495,132},Scale{496,496,132},
                        Scale{856,496,132},Scale{879,496,132}}) {
        Fixture f;f.put(Fixture::sprite+0x7a,s.scale);f.put(0x081af2cc+s.index*2,s.reciprocal);
        f.matrix(0,s.reciprocal);require(f.observe(),"valid/clamped scale rejected");
    }
    Fixture f;require(f.observe(),"display setup failed");
    ObjectPositions frame{};frame[0]=f.out[Fixture::first];
    for(unsigned i=0;i<frame.size();++i) {
        f.put(0x07000000+i*8,frame[i].a0);f.put(0x07000002+i*8,frame[i].a1);f.put(0x07000004+i*8,frame[i].a2);
    }
    auto read=[&](unsigned a,unsigned w){return f.read(a,w);};
    require(matches_display(read,frame),"exact display rejected");
    for(unsigned offset:{0u,2u,4u}) {
        const unsigned a=0x07000000+offset,old=f.read(a,2);
        f.put(a,old^0x400);require(!matches_display(read,frame),"mutated display attribute accepted");f.put(a,old);
    }
}

void rejected_clouds() {
    for(unsigned scale:{0u,255u,880u,0xffffu}) {Fixture f;f.put(Fixture::sprite+0x7a,scale);f.reject();}
    for(unsigned counter:{0u,33u,0xffffu}) {Fixture f;f.put(0x030069d2,counter);f.reject();}
    for(unsigned offset:{6u,14u,22u,30u}) {Fixture f;f.put(0x03005a00+offset,7);f.reject();}
    for(unsigned reg:{0u,4u,5u,6u,7u}) {Fixture f;f.r[reg]^=1;f.reject();}
    {Fixture f;f.put(0x081af2cc+512,255);f.reject();}
    for(unsigned sp:{0x02fffffcu,0x03007ff0u,Fixture::sp+1,0xfffffff0u}) {Fixture f;f.r[13]=sp;f.reject(false);}
    for(unsigned caller:{0u,0x080dc486u,0x080def6du,0x080dc02bu}) {Fixture f;f.put(Fixture::sp+16,caller,4);f.reject(false);}
    for(unsigned owner:{0x0300245fu,0x030034e0u,Fixture::sprite+1,0xfffffff0u}) {
        Fixture f;f.put(Fixture::sp+8,owner,4);f.reject(false);
    }
    for(unsigned id:{0xbau,0xbcu,0xc1u,0x194u}) {Fixture f;f.put(Fixture::sprite+0x32,id);f.reject(false);}
    {Fixture f;f.put(0x0300702c,0x03002510,4);f.reject(false);}
    for(unsigned allocation:{149u,1024u,0xffffu}) {Fixture f;f.put(Fixture::sprite+0x34,allocation);f.reject(false);}
    for(unsigned status:{0u,5u}) {Fixture f;f.put(Fixture::sprite+0x24,status);f.reject();}
    for(unsigned phase:{1u,3u}) {Fixture f;f.put(Fixture::sprite+0x6a,phase);f.reject();}
    for(unsigned flags:{0x2804u,0x0805u}) {Fixture f;f.put(Fixture::sprite+0x2a,flags);f.reject();}
    {Fixture f;f.put(Fixture::metadata,648);f.reject();}
    {Fixture f;f.put(Fixture::metadata+2,389);f.reject();}
    {Fixture f;f.put(Fixture::sprite+0x94,255,1);f.reject();}
    for(unsigned offset:{0u,2u}) {Fixture f;f.put(Fixture::address+offset,f.read(Fixture::address+offset,2)^1);f.reject();}
    // The final staging slot is legal and cannot erase the earlier allocation.
    Fixture f;f.put(Fixture::sprite+0x34,255*4);
    const unsigned last=0x03005a00+255*8,meta=0x0202c8b0+255*4;
    f.put(last,0x017d);f.put(last+2,0x8080);f.put(last+4,0x098c);f.put(meta,136);f.put(meta+2,133);
    require(f.observe(255) && f.out[255].valid && f.out[255].x==128 && f.out[255].source==255,"last staging slot rejected");
}
}
int main() {
    try {
        valid_clouds();rejected_clouds();
        std::cout<<"PASS cloud: "<<checks<<" checks; signed/full X, exact final attributes, matrix/scale, ABI, stale provenance, read-only guest\n";
    } catch(const std::exception& error) {
        std::cerr<<"FAIL cloud: "<<error.what()<<" after "<<checks<<" checks\n";return 1;
    }
}
