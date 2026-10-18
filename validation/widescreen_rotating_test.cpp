#include "../src/widescreen_rotating.h"
#include "../src/widescreen_ui.h"
#include <map>
#include <stdexcept>
#include <iostream>

static void test_rotating_pair() {
    using namespace sma3::wide;
    const unsigned sprite=0x03003430,first=37,a=0x03005a00+first*8;
    std::map<uint32_t,uint32_t> m;
    std::array<uint32_t,16> r{};
    StagingPositions out{};
    auto read=[&](uint32_t address,unsigned) {
        const auto it=m.find(address);return it==m.end()?0u:it->second;
    };
    auto check=[](bool ok){if(!ok)throw std::runtime_error("rotating pair regression");};
    auto setup=[&] {
        m.clear();r={};out={};r[4]=sprite;
        m[0x0300702c]=0x03002460;
        m[sprite+0x24]=8;m[sprite+0x2a]=2<<11;m[sprite+0x32]=0x13c;
        m[sprite+0x34]=first*4;m[sprite+0x5a]=2488;m[sprite+0x5c]=1864;
        m[sprite+0x7a]=0;m[0x030069d2]=2;
        m[0x030069d4]=2200;m[0x030069dc]=1812;
        m[0x081af6ce]=256;m[0x081af74e]=0;
        m[a]=0x0124;m[a+2]=0x8100;m[a+4]=0x099c;
        m[a+8]=0x0124;m[a+10]=0x8320;m[a+12]=0x099c;
        out[first]={123,true,0x0124,0x8100,0x099c,first};
        out[first+1]={155,true,0x0124,0x8320,0x099c,first+1};
        out[100]={777,true,0x0123,0x8456,0x789a,100};
    };
    auto observe=[&]{return observe_rotating_pair(read,r.data(),out);};
    auto check_rejected=[&] {
        const auto before=m;const auto registers=r;
        check(!observe());
        check(!out[first].valid && !out[first+1].valid);
        check(out[first].source==256 && out[first+1].source==256);
        check(out[100].valid && out[100].x==777 && out[100].source==100);
        check(m==before && r==registers);
    };

    // Exact values from the castle approach, crossing and departure. The last
    // case deliberately has the same OAM X as the first, 512 pixels farther left.
    struct Sample { unsigned cx,cy,a0,a10,a11;int x0,x1; };
    const Sample samples[]={{2200,1812,0x0124,0x8100,0x8320,256,288},
        {2324,1812,0x0124,0x8084,0x82a4,132,164},
        {2500,1840,0x0108,0x81d4,0x83f4,-44,-12},
        {2712,1812,0x0124,0x8100,0x8320,-256,-224}};
    for(const auto& sample:samples) {
        setup();m[0x030069d4]=sample.cx;m[0x030069dc]=sample.cy;
        m[a]=sample.a0;m[a+8]=sample.a0;m[a+2]=sample.a10;m[a+10]=sample.a11;
        const auto before=m;const auto registers=r;
        check(observe());check(m==before && r==registers);
        check(out[first].valid && out[first+1].valid);
        check(out[first].x==sample.x0 && out[first+1].x==sample.x1);
        check(out[first].a0==sample.a0 && out[first+1].a0==sample.a0);
        check(out[first].a1==sample.a10 && out[first+1].a1==sample.a11);
        check(out[first].a2==0x099c && out[first+1].a2==0x099c);
        check(out[first].source==first && out[first+1].source==first+1);
        check(!out[first].screen_space && !out[first+1].screen_space);
    }
    // Signed table values, guest rounding and discarded angle bits.
    setup();m[sprite+0x7a]=0xfe83;
    m[0x081af750]=uint16_t(-256);m[0x081af7d0]=uint16_t(-129);
    m[a]=0x0120;m[a+8]=0x0120;m[a+2]=0x80f0;m[a+10]=0x8330;
    check(observe() && out[first].x==240 && out[first+1].x==304);
    // Full 16-bit guest arithmetic, including its signed wrap boundary.
    setup();m[sprite+0x5a]=65528;m[0x030069d4]=8;m[a+2]=0x81d0;m[a+10]=0x83f0;
    check(observe() && out[first].x==-48 && out[first+1].x==-16);
    setup();m[sprite+0x5a]=0x801f;m[0x030069d4]=0;m[a+2]=0x81ff;m[a+10]=0x821f;
    check(observe() && out[first].x==32767 && out[first+1].x==-32737);
    setup();m[sprite+0x5a]=0x8020;m[0x030069d4]=0;m[a+2]=0x8000;m[a+10]=0x8220;
    check(observe() && out[first].x==-32768 && out[first+1].x==-32736);
    struct Vertical { int y;unsigned a0; };
    const Vertical vertical[]={{-33,0x01a0},{-32,0x01e0},{159,0x019f},{160,0x01a0}};
    for(const auto& sample:vertical) {
        setup();m[sprite+0x5c]=uint16_t(1812+16+sample.y);
        m[a]=sample.a0;m[a+8]=sample.a0;
        check(observe() && out[first].a0==sample.a0 && out[first+1].a0==sample.a0);
    }
    // Matrices and post-draw tile/palette/priority come from the final pair.
    setup();m[0x030069d2]=12;m[a+2]=0x9500;m[a+10]=0x9720;
    check(observe() && out[first].a1==0x9500 && out[first+1].a1==0x9720);
    setup();m[0x030069d2]=32;m[a+2]=0xbd00;m[a+10]=0xbf20;
    check(observe() && out[first].a1==0xbd00 && out[first+1].a1==0xbf20);
    for(unsigned tile:{0x099cu,0x019cu,0xf99cu}) {
        setup();m[a]=0x2524;m[a+8]=0x1924;m[a+4]=tile;m[a+12]=tile;
        check(observe() && out[first].a2==tile && out[first+1].a2==tile);
        check(out[first].a0==0x2524 && out[first+1].a0==0x1924);
    }
    setup();check(observe());
    ObjectPositions displayed{};displayed[0]=out[first];displayed[1]=out[first+1];
    for(unsigned i=0;i<displayed.size();++i) {
        m[0x07000000+i*8]=displayed[i].a0;
        m[0x07000002+i*8]=displayed[i].a1;
        m[0x07000004+i*8]=displayed[i].a2;
    }
    check(matches_display(read,displayed));
    for(unsigned i=0;i<2;++i)for(unsigned offset:{0u,2u,4u}) {
        const unsigned address=0x07000000+i*8+offset;
        m[address]^=0x400;check(!matches_display(read,displayed));m[address]^=0x400;
    }
    // Reject both components atomically, including stale attributes on only
    // the second piece and a formerly valid observation at the same allocation.
    const std::pair<unsigned,unsigned> invalid[]={
        {0x0300702c,0x03002510},{sprite+0x32,0x13b},{sprite+0x24,0},
        {sprite+0x24,9},{sprite+0x2a,1<<11},{0x030069d2,0},
        {0x030069d2,1},{0x030069d2,33},{a,0x0125},{a,0x0024},
        {a,0x0324},{a+8,0x0125},{a+2,0xc100},{a+2,0x8300},
        {a+2,0x8101},{a+10,0x8321},{a+10,0x8520},
        {a+4,0x099d},{a+12,0x019c}};
    for(const auto& change:invalid) { setup();m[change.first]=change.second;check_rejected(); }
    // An invalid owner/allocation cannot name two slots to erase or publish.
    for(unsigned owner:{0x0300245fu,0x030034e0u,sprite+1}) {
        setup();r[4]=owner;check(!observe() && out[first].valid && out[first+1].valid);
    }
    for(unsigned allocation:{first*4+1,255u*4,1024u,0xffffu}) {
        setup();m[sprite+0x34]=allocation;
        check(!observe() && out[first].valid && out[first+1].valid);
    }
    setup();m[sprite+0x34]=254*4;
    const unsigned last=0x03005a00+254*8;
    for(unsigned offset:{0u,2u,4u,8u,10u,12u})m[last+offset]=m[a+offset];
    check(observe() && out[254].valid && out[255].valid);
    check(out[254].source==254 && out[255].source==255);
}

int main() {
    std::map<uint32_t,uint32_t> m;
    auto read=[&](uint32_t a,unsigned){return m[a];};
    auto check=[](bool x){if(!x)throw std::runtime_error("rotating position regression");};
    sma3::wide::StagingPositions out{};uint32_t r[16]{};
    const unsigned a=0x03005a00,s=0x03004000;
    m[0x030021a4]=a;m[a]=0x4000;m[a+2]=0x4000;m[a+4]=0x998;
    r[0]=288;r[1]=65;
    check(sma3::wide::observe_rotating_position(read,r,out));
    check(out[0].x==272 && out[0].a1==0x4100 && out[0].a0==0x4031);
    r[0]=uint32_t(-224);check(sma3::wide::observe_rotating_position(read,r,out));
    check(out[0].x==-240 && out[0].a1==0x4100);
    r[0]=uint32_t(-16);r[1]=uint32_t(-16);
    check(sma3::wide::observe_rotating_position(read,r,out));
    check(out[0].x==-32 && out[0].a1==0x41e0 && out[0].a0==0x40e0);
    r[0]=272;r[1]=208;check(sma3::wide::observe_rotating_position(read,r,out));
    check(out[0].x==256 && out[0].a1==0x4100 && out[0].a0==0x40c0);
    m[0x030021a4]=a+1;check(!sma3::wide::observe_rotating_position(read,r,out));
    m[s+0x34]=0;m[s+0x32]=0x102;m[0x030069d2]=4;
    for(unsigned i=0;i<6;++i) {
        m[0x030021a4]=a+i*8;r[0]=64+i*8;r[1]=48;
        check(sma3::wide::observe_rotating_position(read,r,out));
        m[a+i*8]=out[i].a0;m[a+i*8+2]=out[i].a1;m[a+i*8+4]=out[i].a2;
    }
    ++m[a+4]; // Stale first component must not be promoted.
    check(sma3::wide::observe_rotating_affine(read,s,out));
    check(!out[0].valid && out[1].valid && out[1].x==56);
    check(out[1].a1==(0x8000|4<<9|56) && out[1].a0==0x120);
    check(out[3].a1==(0x8000|5<<9|72));
    m[s+0x34]=255*4;check(!sma3::wide::observe_rotating_affine(read,s,out));
    m[s+0x34]=0;m[0x030069d2]=31;check(!sma3::wide::observe_rotating_affine(read,s,out));
    r[7]=s;r[5]=a;r[0]=0x168;r[1]=0;m[s+0x34]=0;m[s+0x20]=48;
    m[a+2]=0x4030;m[a+4]=0x99c;m[0x030069d2]=1;
    check(sma3::wide::observe_scaled_component(read,r,out));
    check(out[0].x==32 && out[0].a0==0x368 && out[0].a1==0x8220 && out[0].a2==0x99c);
    r[8]=s;r[0]=0x300;r[1]=0x68;
    check(sma3::wide::observe_scaled_component(read,r,out,0x08063d32));
    check(out[0].x==32 && out[0].a0==0x368 && out[0].a1==0x8220);
    check(!sma3::wide::observe_scaled_component(read,r,out,0));
    m[s+0x20]=uint16_t(-16);m[a+2]=0x41f0;
    check(sma3::wide::observe_scaled_component(read,r,out) && out[0].x==-32);
    m[a+2]=200;check(!sma3::wide::observe_scaled_component(read,r,out));
    m[s+0x34]=4;check(!sma3::wide::observe_scaled_component(read,r,out));
    m[s+0x34]=0;m[a]=160;m[a+2]=0;r[0]=152;r[1]=0;r[7]=s;
    check(sma3::wide::observe_scaled_component(read,r,out));
    check(!out[0].valid && out[0].screen_space && out[0].a0==0x398 && out[0].a1==0x83f0);
    auto candidate=out[0];candidate.valid=true;candidate.screen_space=false;
    sma3::wide::apply_screen_observation(candidate,out[0]);
    check(!candidate.valid && candidate.screen_space);
    candidate=out[0];candidate.valid=true;candidate.screen_space=false;++candidate.a2;
    sma3::wide::apply_screen_observation(candidate,out[0]);check(candidate.valid);
    r[10]=s;r[7]=s+0x76;m[s+0x34]=0;r[5]=a+16;
    r[4]=160;r[0]=6;r[9]=0;m[s+0x20]=uint16_t(-16);
    m[a+18]=0x8000;m[a+20]=0x194;m[0x030069d2]=2;
    check(sma3::wide::observe_offset_affine(read,r,out));
    check(out[2].x==-24 && out[2].a0==0x19a && out[2].a1==0x85e8 && out[2].a2==0x194);
    m[s+0x34]=0xffff;r[5]=0x03025a08;m[a+10]=0;m[a+12]=0;
    check(sma3::wide::observe_offset_affine(read,r,out) && out[1].x==-24);
    ++r[5];check(!sma3::wide::observe_offset_affine(read,r,out));--r[5];
    ++r[7];check(!sma3::wide::observe_offset_affine(read,r,out));
    test_rotating_pair();
    std::cout<<"PASS: unclamped signed X, guest clamp boundaries, affine groups, final rotating pairs, stale attributes and capacity\n";
}
