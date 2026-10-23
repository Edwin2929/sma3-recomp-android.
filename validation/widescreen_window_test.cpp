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
template<std::size_t N>
bool same(const std::array<ObjectPosition,N>& a,const std::array<ObjectPosition,N>& b) {
    for(unsigned i=0;i<N;++i)if(!same(a[i],b[i]))return false;
    return true;
}
struct Fixture {
    static constexpr unsigned staging=0x03005a00,buffer=0x0202badc;
    std::map<unsigned,uint8_t> memory;
    std::array<uint32_t,16> r{};
    StagingPositions strips{},emitted{};
    ObjectPositions copies{},normal{};
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
    void object(unsigned slot,int x,int y,unsigned a0,unsigned a1,unsigned a2) {
        put(staging+slot*8,a0);put(staging+slot*8+2,a1);put(staging+slot*8+4,a2);
        put(0x0202c8b0+slot*4,uint16_t(x));put(0x0202c8b2+slot*4,uint16_t(y));
    }
    void group(unsigned source,unsigned destination,unsigned count) {
        r[3]=buffer+destination*8;r[4]=staging+source*8;r[5]=count;
        r[6]=0x1890;r[7]=0x0300702c;r[8]=0xf3ff;r[12]=0x800;
        put(0x03003cf0,destination,1);
    }
    Fixture() {
        put(0x0300702c,0x03002460,4);put(0x03007240,0x0300220c,4);
        put(0x03004ba6,0x1c);put(0x03006d68,0,1);put(0x03006af7,0,1);
        for(unsigned i=0;i<256;++i)put(staging+i*8,160);
        for(unsigned i=0;i<128;++i)put(0x07000000+i*8,160);
        object(57,-24,28,0x001c,0x41e8,0x0900);
        object(58,488,28,0x001c,0x41e8,0x0902);
        object(59,-24,160,0x00a0,0x41e8,0x0904);
        group(57,0,3);
    }
    bool observe() {
        const auto before=memory;const auto regs=r;const auto s=strips,e=emitted;
        const bool ok=observe_window_copies([&](unsigned a,unsigned w){return read(a,w);},r.data(),strips,emitted,copies);
        require(memory==before && r==regs,"copy observer changed guest memory or CPU");
        require(same(strips,s) && same(emitted,e),"copy observer changed source provenance");
        return ok;
    }
    void completed() {
        // Literal completed copy-loop results, including its hidden-Y entry.
        put(buffer,0x081c);put(buffer+2,0x41e8);put(buffer+4,0x0100);
        put(buffer+8,0x081c);put(buffer+10,0x41e8);put(buffer+12,0x0102);
        put(buffer+16,0x08a0);put(buffer+18,0x41e8);put(buffer+20,0x0104);
        put(0x03003cf0,3,1);
        resolve_object_positions([&](unsigned a,unsigned w){return read(a,w);},normal,false);
    }
    bool compose() {
        const auto before=memory;const auto regs=r;const auto cache=copies,original=normal;
        const bool ok=compose_window_submission([&](unsigned a,unsigned w){return read(a,w);},copies,normal);
        require(memory==before && r==regs && same(cache,copies),"composer changed guest state or copy cache");
        if(!ok)require(same(original,normal),"failed composition published a partial frame");
        return ok;
    }
    void display(const ObjectPositions& frame) {
        for(unsigned i=0;i<128;++i) {
            put(0x07000000+i*8,frame[i].a0);put(0x07000002+i*8,frame[i].a1);put(0x07000004+i*8,frame[i].a2);
        }
    }
    bool matches(const ObjectPositions& frame) const {
        return matches_display([&](unsigned a,unsigned w){return read(a,w);},frame);
    }
    void normal_count(unsigned count) {
        for(unsigned i=0;i<256;++i)put(staging+i*8,160);
        for(unsigned i=0;i<count;++i)object(i,int(i),20,0x0014,0x4000|i,0x1900+i);
        resolve_object_positions([&](unsigned a,unsigned w){return read(a,w);},normal,false);
    }
};

void exact_presentation() {
    Fixture f;require(f.observe(),"copy group rejected");
    require(f.copies[0].x==-24 && f.copies[1].x==488,"copy aliased distinct full X anchors");
    require(f.copies[2].source==59 && f.copies[2].a0==0x08a0,"hidden-Y copy was skipped");
    require(f.copies[0].a0==0x081c && f.copies[0].a1==0x41e8 && f.copies[0].a2==0x0100,"copy attributes incorrect");
    f.completed();require(f.compose(),"exact composition rejected");
    require(f.normal[0].source==57 && f.normal[1].source==58 && f.normal[2].source==59,"prefix source order lost");
    require(f.normal[3].source==57 && f.normal[3].x==-24 && f.normal[3].a0==0x001c && f.normal[3].a2==0x0900,"first original lost");
    require(f.normal[4].source==58 && f.normal[4].x==488 && f.normal[4].a2==0x0902,"second original lost");
    require(f.normal[5].source==256 && f.normal[5].a0==160,"unused OAM was not hidden");
    f.display(f.normal);require(f.matches(f.normal),"exact atomic presentation rejected");
    for(unsigned slot:{0u,1u,3u,4u})for(unsigned offset:{0u,2u,4u}) {
        const unsigned address=0x07000000+slot*8+offset,old=f.read(address,2);
        f.put(address,old^0x400);require(!f.matches(f.normal),"modified displayed triple accepted");f.put(address,old);
    }
    // A presentation built from two different completed frames matches neither.
    const auto current=f.normal;auto older=current;older[0].a2=0x0108;older[3].a2=0x0908;
    f.put(0x07000004,0x0108);
    require(!f.matches(current) && !f.matches(older),"mixed-age presentation accepted");
    f.display(older);require(f.matches(older) && !f.matches(current),"complete older presentation not distinguished");
    // Affine matrix indices stay unchanged; VBlank restores coefficients at
    // fixed OAM addresses separately from this prefixed attribute stream.
    Fixture affine;affine.object(57,-24,28,0x011c,0x8fe8,0x0900);
    require(affine.observe() && affine.copies[0].a1==0x8fe8,"copy shifted affine matrix index");
}

void exact_provenance() {
    Fixture s;s.strips[57]={-1048,true,0x001c,0x41e8,0x0900,57};
    require(s.observe() && s.copies[0].x==-1048,"complete producer X not retained");
    Fixture mismatch;mismatch.strips[57]={-1048,true,0x001c,0x41e8,0x0901,57};
    require(mismatch.observe() && mismatch.copies[0].x==-24,"partial producer attributes authorized X");
    Fixture unknown;unknown.put(0x0202c8b2+57*4,29);
    require(unknown.observe() && !unknown.copies[0].valid && unknown.copies[0].source==57,"unknown X falsely approved");
    unknown.completed();require(unknown.compose() && !unknown.normal[0].valid,"unknown copy cannot remain unresolved");
    Fixture generic;generic.put(0x0202c8b2+57*4,29);
    generic.emitted[57]={-24,true,0x001c,0x41e8,0x0900,57};
    require(generic.observe() && generic.copies[0].valid && generic.copies[0].x==-24,"exact generic fallback rejected");
    Fixture bad_source;bad_source.put(0x0202c8b2+57*4,29);
    bad_source.emitted[57]={-24,true,0x001c,0x41e8,0x0900,58};
    require(bad_source.observe() && !bad_source.copies[0].valid,"wrong source authorized copy");
    for(unsigned offset:{0u,2u,4u}) {
        Fixture f;require(f.observe(),"mutation setup failed");f.completed();
        f.put(Fixture::buffer+8+offset,f.read(Fixture::buffer+8+offset,2)^1);
        require(!f.compose(),"mutated completed copy accepted");
    }
    Fixture missing;missing.completed();require(!missing.compose(),"unobserved prefix accepted");
}

void rejected_groups() {
    for(unsigned reg:{6u,7u,8u,12u}) {
        Fixture f;f.copies[0]={700,true,1,2,3,57};f.r[reg]^=1;
        require(!f.observe() && !f.copies[0].valid && f.copies[0].source==256,"bad ABI retained stale copy");
    }
    struct Change { unsigned a,value,width; };
    for(const auto& c:{Change{0x0300702c,0x03002510,4},Change{0x03007240,0x03002210,4},
                       Change{0x03004ba6,0x1b,2},Change{0x03006d68,1,1},Change{0x03003cf0,1,1}}) {
        Fixture f;f.put(c.a,c.value,c.width);require(!f.observe(),"bad copy precondition accepted");
    }
    for(unsigned source:{Fixture::staging-8,Fixture::staging+256*8,Fixture::staging+1,0xfffffff8u}) {
        Fixture f;f.r[4]=source;require(!f.observe(),"invalid source range accepted");
    }
    for(unsigned destination:{Fixture::buffer-8,Fixture::buffer+128*8,Fixture::buffer+1,0xfffffffcu}) {
        Fixture f;f.r[3]=destination;require(!f.observe(),"invalid destination range accepted");
    }
    for(unsigned count:{0u,32u,0xffffffffu}) {Fixture f;f.r[5]=count;require(!f.observe(),"invalid group length accepted");}
    {Fixture f;f.group(255,0,2);require(!f.observe(),"source overflow accepted");}
    {Fixture f;f.group(57,127,2);require(!f.observe(),"destination overflow accepted");}
    Fixture last;last.object(255,272,40,0x0028,0x4110,0x1900);last.group(255,127,1);
    require(last.observe() && last.copies[127].source==255 && last.copies[127].x==272 && last.copies[127].a2==0x1100,"last valid group rejected");
}

void capacity_and_branches() {
    Fixture f;require(f.observe(),"capacity setup failed");f.completed();f.normal_count(126);
    f.copies[2]={};require(f.compose(),"unused clamped copy incorrectly required");
    require(f.normal[0].source==57 && f.normal[1].source==58 && f.normal[2].source==0 && f.normal[127].source==125,"prefix capacity clamp lost originals");
    Fixture full;full.completed();full.normal_count(128);const auto originals=full.normal;
    require(full.compose() && same(full.normal,originals),"full native OAM should clamp prefix to zero");
    Fixture overflow;overflow.completed();overflow.normal_count(129);require(!overflow.compose(),"unsupported native overflow accepted");
    Fixture direct;direct.completed();direct.put(0x03006af7,1,1);const auto normal=direct.normal;
    require(direct.compose() && same(direct.normal,normal),"direct-copy branch changed originals");
    Fixture zero;zero.completed();zero.put(0x03003cf0,0,1);const auto no_prefix=zero.normal;
    require(zero.compose() && same(zero.normal,no_prefix),"zero prefix changed originals");
    for(const auto& change:{std::pair{0x03007240u,0x03002210u},std::pair{0x03004ba6u,0x1bu},std::pair{0x03006d68u,1u}}) {
        Fixture bad;require(bad.observe(),"mode setup failed");bad.completed();
        bad.put(change.first,change.second,change.first==0x03007240?4:change.first==0x03006d68?1:2);
        require(!bad.compose(),"unsupported composition mode accepted");
    }
}
}
int main() {
    try {
        exact_presentation();exact_provenance();rejected_groups();capacity_and_branches();
        std::cout<<"PASS window: "<<checks<<" checks; exact copies/originals, full X, atomic presentation, stale triples, capacities and read-only guest\n";
    } catch(const std::exception& error) {
        std::cerr<<"FAIL window: "<<error.what()<<" after "<<checks<<" checks\n";return 1;
    }
}
