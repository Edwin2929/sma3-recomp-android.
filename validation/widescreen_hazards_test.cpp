#include "../src/widescreen_hazards.h"
#include "../src/widescreen_ui.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

using namespace sma3::wide;
namespace {
unsigned checks=0;
void check(bool ok,const std::string& message) {
    ++checks;
    if(!ok)throw std::runtime_error(message);
}
int signed_half(uint32_t value) {
    const int low=int(value&65535);
    return low<32768?low:low-65536;
}
// An independent integer-division model of Thumb ASR, not the observer's >>.
int floor_divide(int value,unsigned bits) {
    const int divisor=1<<bits;
    return value<0?-((-value+divisor-1)/divisor):value/divisor;
}
struct Memory {
    std::map<uint32_t,uint8_t> bytes;
    uint32_t get(uint32_t address,unsigned width) const {
        check(width==1 || width==2 || width==4,"reader width");
        check((address>=0x02000000 && uint64_t(address)+width<=0x02040000) ||
              (address>=0x03000000 && uint64_t(address)+width<=0x03008000) ||
              (address>=0x08000000 && uint64_t(address)+width<=0x08400000),"reader bounds");
        uint32_t result=0;
        for(unsigned i=0;i<width;++i) {
            const auto it=bytes.find(address+i);
            if(it!=bytes.end())result|=uint32_t(it->second)<<(i*8);
        }
        return result;
    }
    void put(uint32_t address,uint32_t value,unsigned width=2) {
        for(unsigned i=0;i<width;++i)bytes[address+i]=uint8_t(value>>(i*8));
    }
};
bool same(const ObjectPosition& a,const ObjectPosition& b) {
    return a.x==b.x && a.valid==b.valid && a.a0==b.a0 && a.a1==b.a1 &&
        a.a2==b.a2 && a.source==b.source && a.screen_space==b.screen_space;
}
ObjectPosition canary(unsigned slot) {
    return {int(1000+slot),true,0x52,0x4300,0x9876,slot,false};
}

struct Fixture {
    static constexpr unsigned normal=0x0300220c,base=0x03002460;
    Memory memory;
    std::array<uint32_t,16> r{};
    StagingPositions positions{};
    Hazard64Capture capture{};
    unsigned first,owner,entry_stack;

    Fixture(int x=100,int y=80,unsigned angle=0,int cosine=256,int sine=0,
            unsigned source=67,unsigned sprite=base+22*0xb0,unsigned stack=0x03007e00)
        :first(source),owner(sprite),entry_stack(stack) {
        for(unsigned i=0;i<positions.size();++i)positions[i]=canary(i);
        r[0]=owner;r[4]=owner;r[13]=stack;r[14]=0x0808e37d;
        memory.put(0x0300702c,base,4);memory.put(0x03007240,normal,4);
        memory.put(owner+0x24,8);memory.put(owner+0x2a,0x8809);
        memory.put(owner+0x32,0x64);memory.put(owner+0x34,first*4);
        memory.put(owner+0x94,0,1);memory.put(owner+0x6a,0);memory.put(owner+0x70,0);
        memory.put(owner+4,0x47800,4);memory.put(normal+0x2a1a,0);
        memory.put(owner+0x20,uint16_t(x));memory.put(owner+0x22,uint16_t(y));
        memory.put(owner+0x72,angle);
        memory.put(0x0817602a,16,1);memory.put(0x0817602b,16,1);
        memory.put(0x0817602e,40,1);memory.put(0x081af2d2,21845);
        memory.put(0x081af6ce + (angle>>8)*2,uint16_t(cosine));
        memory.put(0x081af74e + (angle>>8)*2,uint16_t(sine));
        for(unsigned i=0;i<17;++i) {
            const unsigned a=address(i),m=metadata(i);
            const uint16_t mx=uint16_t(x+(i<8?0:4)),my=uint16_t(y+(i<8?-8:4));
            const int sx=signed_half(mx),sy=signed_half(my);
            const bool clipped=sx< -64 || sx>319 || sy< -32 || sy>191;
            memory.put(a,clipped?160:my&255);
            memory.put(a+2,(i<8?0x4000:0)|(mx&511));
            memory.put(a+4,i<8?0x4828+(i%2)*2:0x48bd);
            memory.put(m,mx);memory.put(m+2,my);
        }
    }
    unsigned address(unsigned i) const { return 0x03005a00+(first+i)*8; }
    unsigned metadata(unsigned i) const { return 0x0202c8b0+(first+i)*4; }
    bool observe(uint32_t pc) {
        const auto before=memory.bytes;const auto registers=r;
        auto read=[&](uint32_t a,unsigned w){return memory.get(a,w);};
        const bool result=observe_hazard64(read,pc,r.data(),positions,capture);
        check(memory.bytes==before,"observer changed guest memory");
        check(r==registers,"observer changed guest registers");
        return result;
    }
    bool enter() {return observe(0x0808e144);}
    bool finish() {return observe(0x0808e33c);}
    void check_unrelated() const {
        for(unsigned i=0;i<positions.size();++i)
            if(i<first || i>=first+17)check(same(positions[i],canary(i)),"unrelated source changed");
    }
    void check_cleared() const {
        for(unsigned i=0;i<17;++i)check(same(positions[first+i],ObjectPosition{}),"partial or stale group survived");
        check_unrelated();
    }
    void poison_group() {
        for(unsigned i=0;i<17;++i)positions[first+i]=canary(first+i);
    }

    // Small independent model of the producer's writes, with u16 temporaries
    // and unsigned Thumb clipping tests. The literal geometry cases below
    // separately constrain the expected full coordinates of these writes.
    void draw() {
        const auto at_entry=r;
        const unsigned sp=entry_stack-32;
        memory.put(sp,at_entry[8],4);memory.put(sp+4,at_entry[9],4);memory.put(sp+8,at_entry[10],4);
        for(unsigned i=0;i<4;++i)memory.put(sp+12+i*4,at_entry[4+i],4);
        memory.put(sp+28,at_entry[14],4);
        const unsigned theta=memory.get(owner+0x72,2)>>8;
        const int c=floor_divide(signed_half(memory.get(0x081af6ce + theta*2,2))*40,8);
        const int s=floor_divide(signed_half(memory.get(0x081af74e + theta*2,2))*40,8);
        const int ic=floor_divide(c*21845,16),is=floor_divide(s*21845,16);
        const unsigned x=memory.get(owner+0x20,2),y=memory.get(owner+0x22,2);
        const unsigned center=uint16_t(x+8-memory.get(0x0817602a,1));
        memory.put(normal+0x2a12,16);memory.put(normal+0x2a16,16);
        memory.put(normal+0x2a2a,uint16_t(c));memory.put(normal+0x2a2e,uint16_t(s));
        memory.put(0x03006a02,uint16_t(c));memory.put(0x03006a04,uint16_t(s));
        memory.put(0x03006a06,uint16_t(2*ic));memory.put(0x03006a08,uint16_t(2*is));
        const std::array<uint16_t,4> xs={uint16_t(center+s),uint16_t(center+c),uint16_t(center-s),uint16_t(center-c)};
        const std::array<uint16_t,4> ys={uint16_t(y-c),uint16_t(y+s),uint16_t(y+c),uint16_t(y-s)};
        for(unsigned q=0;q<4;++q) {
            const unsigned a=address(q*2);
            if(uint16_t(ys[q]+16)>223) {
                memory.put(a,160);memory.put(a+8,160);
            } else {
                memory.put(a,ys[q]&255);memory.put(a+8,ys[q]&255);
                memory.put(a+2,0x4000|(xs[q]&511));memory.put(a+10,0x4000|((xs[q]+16)&511));
                memory.put(a+4,0x4848);memory.put(a+12,0x484a);
            }
        }
        for(unsigned ring=0;ring<2;++ring) {
            const int rc=ic*int(ring+1),rs=is*int(ring+1);
            const uint16_t bx=uint16_t(x+4),by=uint16_t(y+4);
            const std::array<uint16_t,4> px={uint16_t(bx+rs),uint16_t(bx+rc),uint16_t(bx-rs),uint16_t(bx-rc)};
            const std::array<uint16_t,4> py={uint16_t(by-rc),uint16_t(by+rs),uint16_t(by+rc),uint16_t(by-rs)};
            for(unsigned q=0;q<4;++q) {
                const unsigned a=address(8+ring*4+q);
                if(uint16_t(py[q]+32)>223)memory.put(a,0x200);
                else {
                    memory.put(a,py[q]&255);memory.put(a+2,px[q]&511);
                    memory.put(a+4,(memory.get(a+4,2)&0xfc00)|0x16d);
                }
            }
        }
        r[0]=0x0808e33d;r[2]=address(15);r[4]=uint16_t(center-c);
        r[5]=0x030069f4;r[6]=center;r[7]=4;r[8]=owner;r[9]=0x4848;
        r[10]=0x4800;r[12]=y;r[13]=sp;r[14]=0x0808e33d;
    }
    void accepted() {
        check(enter(),"valid entry rejected");check(capture.pending,"entry not pending");check_cleared();
        draw();check(finish(),"completed producer rejected");check(!capture.pending,"capture reused after return");
        check_unrelated();
    }
};

void test_literal_geometry() {
    struct Sample {
        int x,y;unsigned angle;int cosine,sine;
        std::array<int,17> xs,ys;
    };
    // Explicit cases, including negative ASR and the doubled rounded inner
    // radius. They are test vectors, not claims of a new native replay.
    const Sample samples[]={
        {100,80,0,256,0,
         {92,108,132,148,92,108,52,68,104,117,104,91,104,130,104,78,104},
         {40,40,80,80,120,120,80,80,71,84,97,84,58,84,110,84,84}},
        {205,167,0xd300,115,229,
         {232,248,214,230,162,178,180,196,220,214,198,204,231,219,187,199,209},
         {150,150,202,202,184,184,132,132,166,182,176,160,161,193,181,149,171}},
        {-27,103,0x4b00,-68,-247,
         {-74,-58,-46,-30,4,20,-24,-8,-36,-27,-10,-19,-49,-31,3,-15,-23},
         {114,114,64,64,92,92,142,142,111,94,103,120,115,81,99,133,107}}
    };
    for(const auto& sample:samples) {
        Fixture f(sample.x,sample.y,sample.angle,sample.cosine,sample.sine);
        const auto before=f.memory;
        f.accepted();
        for(unsigned i=0;i<17;++i) {
            const auto& p=f.positions[f.first+i];
            const bool clipped=i<8?(sample.ys[i]< -16 || sample.ys[i]>207):
                i<16?(sample.ys[i]< -32 || sample.ys[i]>191):
                (before.get(f.address(i),2)&255)==160;
            check(p.source==f.first+i,"source identity");
            check(p.valid==!clipped && p.screen_space==clipped,"clipping classification");
            check(p.x==(clipped?0:sample.xs[i]),"literal full signed X");
            check(p.a0==f.memory.get(f.address(i),2) && p.a1==f.memory.get(f.address(i)+2,2) &&
                  p.a2==f.memory.get(f.address(i)+4,2),"exact final triple");
            if(clipped && i<16)check(p.a1==before.get(f.address(i)+2,2) &&
                  p.a2==before.get(f.address(i)+4,2),"partial clipped write preserved");
        }
    }
    Fixture tiny(100,80,0x01ff,-1,-1);tiny.accepted();
    check(tiny.positions[tiny.first].x==91,"negative ASR must round down");
    check(tiny.positions[tiny.first+8].x==103 && tiny.positions[tiny.first+12].x==102,
          "outer radius doubles after ASR");
}

void test_signed_coordinates_and_clipping() {
    for(int delta:{-512,0,512}) {
        Fixture f(100+delta);f.accepted();
        check(f.positions[f.first].x==92+delta,"packed-X alias must retain full anchor");
        check(f.positions[f.first].a1==0x405c,"packed-X alias reference");
    }
    Fixture edge(32765);edge.accepted();
    check(edge.positions[edge.first].x==32757 && edge.positions[edge.first+1].x==32773,
          "adjacent half must not acquire an extra signed wrap");
    check(edge.positions[edge.first+2].x==-32739,"quadrant origin signed wrap");
    for(int x:{-32768,-32767,32767,65535}) {
        Fixture f(x);f.accepted();
        check(f.positions[f.first].x==signed_half(uint16_t(x-8)),"signed16 anchor boundary");
    }
    for(int y:{-17,-16,207,208}) {
        Fixture f(100,y+40);const auto previous=f.memory;f.accepted();
        const auto& p=f.positions[f.first];
        const bool clipped=y< -16 || y>207;
        check(p.valid==!clipped && p.screen_space==clipped,"pair Y boundary");
        check(p.a0==unsigned(clipped?160:(y&255)),"pair A0 boundary");
        if(clipped)check(p.a1==previous.get(f.address(0)+2,2) && p.a2==previous.get(f.address(0)+4,2),
                         "pair clipping retained A1/A2");
    }
    for(int y:{-33,-32,191,192}) {
        Fixture f(100,y+9);const auto previous=f.memory;f.accepted();
        const auto& p=f.positions[f.first+8];
        const bool clipped=y< -32 || y>191;
        check(p.valid==!clipped && p.screen_space==clipped,"point Y boundary");
        check(p.a0==unsigned(clipped?0x200:(y&255)),"point A0 boundary");
        if(clipped)check(p.a1==previous.get(f.address(8)+2,2) && p.a2==previous.get(f.address(8)+4,2),
                         "point clipping retained A1/A2");
    }
    for(int y:{-32768,32767,65504,65535}) {Fixture f(100,y);f.accepted();}
}

void test_atomic_triples_and_metadata() {
    for(int y:{80,400})for(unsigned i=0;i<17;++i)for(unsigned offset:{0u,2u,4u}) {
        Fixture f(100,y);check(f.enter(),"mutation entry");f.draw();f.poison_group();
        const unsigned a=f.address(i)+offset;
        f.memory.put(a,f.memory.get(a,2)^0x4000);
        check(!f.finish(),"mutated final triple accepted");f.check_cleared();
        check(!f.capture.pending,"failed exit retained pending capture");
    }
    for(unsigned i=0;i<17;++i)for(unsigned offset:{0u,2u}) {
        Fixture f;check(f.enter(),"metadata entry");f.draw();f.poison_group();
        const unsigned a=f.metadata(i)+offset;
        f.memory.put(a,f.memory.get(a,2)+(offset==0?512:256));
        check(!f.finish(),"modulo-identical metadata mutation accepted");f.check_cleared();
    }
    // The untouched center is still part of the atomic group and needs exact
    // complete-coordinate metadata, not merely a plausible current A0/A1.
    for(unsigned offset:{0u,2u}) {
        Fixture f;const unsigned a=f.address(16)+offset;
        f.memory.put(a,f.memory.get(a,2)^1);
        check(f.enter(),"center capture entry");f.draw();
        check(!f.finish(),"incompatible unchanged center accepted");f.check_cleared();
    }
    // Arbitrary retained attributes are legitimate in producer-clipped pieces.
    Fixture retained(100,400);
    for(unsigned i=0;i<16;++i) {
        retained.memory.put(retained.address(i)+2,0xa135+i);
        retained.memory.put(retained.address(i)+4,0xf531+i);
    }
    retained.accepted();
    for(unsigned i=0;i<16;++i)check(retained.positions[retained.first+i].a1==0xa135+i &&
        retained.positions[retained.first+i].a2==0xf531+i,"retained attributes guessed");
}

void test_entry_and_return_provenance() {
    struct Mutation {unsigned offset,value,width;};
    const Mutation owner_changes[]={
        {0x24,7,2},{0x24,9,2},{0x2a,0x8009,2},{0x2a,0x8808,2},{0x32,0x55,2},
        {0x32,0x56,2},{0x32,0x15e,2},{0x94,128,1},{0x94,255,1},
        {0x6a,1,2},{0x70,1,2},{4,0,4}
    };
    for(const auto& m:owner_changes) {
        Fixture f;f.memory.put(f.owner+m.offset,m.value,m.width);
        check(!f.enter(),"wrong entry owner/phase accepted");f.check_cleared();
        check(!f.capture.pending,"rejected entry pending");
    }
    const Mutation globals[]={
        {0x0300702c,0x03002464,4},{0x03007240,0x03002210,4},
        {Fixture::normal+0x2a1a,2,2},{Fixture::normal+0x2a1a,256,2}
    };
    for(const auto& m:globals) {
        Fixture f;f.memory.put(m.offset,m.value,m.width);
        check(!f.enter(),"wrong entry global identity accepted");f.check_cleared();
    }
    for(unsigned owner:{0u,0x02002460u,0x0300245fu,0x03002464u,0x030034e0u,0xfffffff0u}) {
        Fixture f;f.capture.pending=true;f.r[0]=owner;
        check(!f.enter() && !f.capture.pending,"out-of-range owner/capture accepted");
        for(unsigned i=0;i<256;++i)check(same(f.positions[i],canary(i)),"unknown owner cleared unrelated slots");
    }
    for(unsigned allocation:{1u,269u,960u,1020u,0xfffcu,0xffffu}) {
        Fixture f;f.memory.put(f.owner+0x34,allocation);
        check(!f.enter() && !f.capture.pending,"invalid allocation accepted");
        for(unsigned i=0;i<256;++i)check(same(f.positions[i],canary(i)),"invalid allocation writes output");
    }
    for(unsigned reg:{4u,14u}) {
        Fixture f;f.r[reg]^=1;check(!f.enter(),"entry caller ABI accepted");f.check_cleared();
    }
    for(unsigned sp:{0u,0x02007fe0u,0x0300001cu,0x03000022u,0x03008004u,0xfffffffcu}) {
        Fixture f;f.r[13]=sp;check(!f.enter(),"entry stack bounds accepted");f.check_cleared();
    }
    for(unsigned i=0;i<17;++i)for(unsigned offset:{0u,2u}) {
        Fixture f;const unsigned a=f.metadata(i)+offset;f.memory.put(a,f.memory.get(a,2)+1);
        check(!f.enter(),"incomplete generic metadata accepted");f.check_cleared();
    }
    for(const auto& m:owner_changes) {
        Fixture f;check(f.enter(),"return identity entry");f.draw();f.poison_group();
        f.memory.put(f.owner+m.offset,m.value,m.width);
        check(!f.finish(),"changed return owner/phase accepted");f.check_cleared();
    }
    for(const auto& m:globals) {
        Fixture f;check(f.enter(),"return global entry");f.draw();
        f.memory.put(m.offset,m.value,m.width);
        check(!f.finish(),"changed return global identity accepted");f.check_cleared();
    }
    for(unsigned offset:{0x20u,0x22u,0x34u,0x72u}) {
        Fixture f;check(f.enter(),"return owner binding entry");f.draw();
        const unsigned a=f.owner+offset;
        f.memory.put(a,f.memory.get(a,2)+(offset==0x20?512:offset==0x22?256:offset==0x34?4:1));
        check(!f.finish(),"owner capture identity changed");f.check_cleared();
    }
    for(unsigned reg:{0u,2u,4u,5u,6u,7u,8u,9u,10u,12u,13u,14u}) {
        Fixture f;check(f.enter(),"return register entry");f.draw();f.r[reg]^=1;
        check(!f.finish(),"incomplete producer ABI accepted");f.check_cleared();
    }
    for(unsigned offset:{12u,28u}) {
        Fixture f;check(f.enter(),"return stack entry");f.draw();
        f.memory.put(f.r[13]+offset,f.memory.get(f.r[13]+offset,4)^1,4);
        check(!f.finish(),"wrong caller stack accepted");f.check_cleared();
    }
    const Mutation completed_changes[]={
        {0x0817602a,15,1},{0x0817602b,17,1},{0x0817602e,41,1},{0x081af2d2,21846,2},
        {0x081af6ce,257,2},{0x081af74e,uint16_t(-257),2},
        {Fixture::normal+0x2a12,15,2},{Fixture::normal+0x2a16,17,2},
        {0x03006a02,41,2},{0x03006a04,1,2},{0x03006a06,27,2},{0x03006a08,1,2},
        {Fixture::normal+0x2a2a,41,2},{Fixture::normal+0x2a2e,1,2}
    };
    for(const auto& m:completed_changes) {
        Fixture f;check(f.enter(),"completed state entry");f.draw();
        f.memory.put(m.offset,m.value,m.width);
        check(!f.finish(),"wrong constants/completed scratch accepted");f.check_cleared();
    }
}

void test_clipping_observation_lifetime() {
    Fixture f(100,400);f.accepted();
    for(unsigned i=0;i<17;++i) {
        const auto sentinel=f.positions[f.first+i];
        check(!sentinel.valid && sentinel.screen_space && sentinel.x==0,"suppressed component gained usable X");
        ObjectPosition p{700,false,sentinel.a0,sentinel.a1,sentinel.a2,sentinel.source,false};
        apply_screen_observation(p,sentinel);
        check(same(p,sentinel),"exact native-clipping sentinel rejected");
        for(unsigned attribute:{0u,1u,2u}) {
            ObjectPosition changed{700,false,sentinel.a0,sentinel.a1,sentinel.a2,sentinel.source,false};
            if(attribute==0)changed.a0^=0x200; // Includes re-enabling a small point.
            if(attribute==1)changed.a1^=1;
            if(attribute==2)changed.a2^=1;
            const auto before=changed;
            apply_screen_observation(changed,sentinel);
            check(same(changed,before) && !changed.screen_space,"mutated/re-enabled component inherited sentinel");
        }
        ObjectPosition reused{700,false,sentinel.a0,sentinel.a1,sentinel.a2,sentinel.source+1,false};
        const auto before=reused;apply_screen_observation(reused,sentinel);
        check(same(reused,before) && !reused.screen_space,"another source inherited sentinel");
    }
    Fixture pending;check(pending.enter(),"pending entry");
    pending.r[0]=0;check(!pending.enter() && !pending.capture.pending,"new failed entry reused old capture");
    pending.r[0]=pending.owner;pending.r[4]=pending.owner;pending.draw();
    check(!pending.finish(),"old exit accepted after rejected new entry");pending.check_cleared();

    Fixture original;check(original.enter(),"original pending entry");
    Fixture replacement(100,80,0,256,0,0,Fixture::base);
    replacement.capture=original.capture;
    check(replacement.enter() && replacement.capture.owner==replacement.owner,"new valid entry did not replace capture");
    replacement.draw();replacement.r[8]=original.owner;
    check(!replacement.finish(),"different invocation's exit accepted");replacement.check_cleared();

    for(unsigned reset_kind:{0u,1u}) {
        Fixture epoch;check(epoch.enter(),"epoch entry");epoch.draw();
        epoch.capture={}; // Both compaction and a state-epoch change reset this object.
        if(reset_kind==1)epoch.positions={};
        check(!epoch.finish(),"capture survived external lifecycle reset");
        check(!epoch.capture.pending,"epoch reset retained pending state");
    }
    Fixture consumed;consumed.accepted();
    check(!consumed.finish(),"completed capture consumed twice");
    Fixture early;check(early.enter(),"early return entry");
    early.r[8]=early.owner;early.r[13]=early.entry_stack-32;
    check(!early.finish(),"incomplete producer accepted");early.check_cleared();
    for(unsigned first:{240u,255u,256u,0xffffffffu}) {
        Fixture invalid;invalid.capture.pending=true;invalid.capture.first=first;
        check(!invalid.finish() && !invalid.capture.pending,"forged capture exceeded source capacity");
        for(unsigned i=0;i<256;++i)check(same(invalid.positions[i],canary(i)),"invalid capture changed output");
    }
}

void test_capacity_and_owner_boundaries() {
    for(unsigned first:{0u,239u})for(unsigned owner:{Fixture::base,Fixture::base+23*0xb0})
        for(unsigned stack:{0x03000020u,0x03008000u}) {
            Fixture f(100,80,0,256,0,first,owner,stack);f.accepted();
            for(unsigned i=0;i<17;++i)check(f.positions[first+i].source==first+i,"capacity source mismatch");
        }
    Fixture unknown;const auto before=unknown.positions;
    check(!unknown.observe(0x0808e33a),"unknown hook accepted");
    for(unsigned i=0;i<256;++i)check(same(before[i],unknown.positions[i]),"unknown hook changed output");
}
}

int main() {
    try {
        test_literal_geometry();
        test_signed_coordinates_and_clipping();
        test_atomic_triples_and_metadata();
        test_entry_and_return_provenance();
        test_clipping_observation_lifetime();
        test_capacity_and_owner_boundaries();
        std::cout<<"PASS widescreen_hazards_test: "<<checks<<" checks\n";
    } catch(const std::exception& e) {
        std::cerr<<"FAIL widescreen_hazards_test: "<<e.what()<<" after "<<checks<<" checks\n";
        return 1;
    }
}
