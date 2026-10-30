// No ROM required for unit cases. Optional private TCP buffers exercise the
// same decoder against a real normal-Yoshi OAM/VRAM submission.
#include "ghost_visual_detail.h"

#include <chrono>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
using namespace sma3;
using namespace sma3::ghost_visual_detail;
unsigned checks = 0;
void require(bool condition, const char* description) {
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
}
void write16(std::uint8_t* p, unsigned value) { p[0] = value; p[1] = value >> 8; }
void write32(std::uint8_t* p, unsigned value) { write16(p, value); write16(p + 2, value >> 16); }

struct Fixture {
    std::array<std::uint8_t,0x8000> iwram{};
    std::array<std::uint8_t,0x400> oam{}, pal{};
    std::array<std::uint8_t,0x18000> vram{};
    std::array<std::uint8_t,0x60> io{};
    Fixture() {
        iwram[0x6b05] = 0x0d;
        write32(iwram.data() + 0x7010, 0x0200000c);
        write32(iwram.data() + 0x7240, 0x0300220c);
        write16(iwram.data() + 0x4cb8, 3);
        write16(iwram.data() + 0x69d4, 100); write16(iwram.data() + 0x69dc, 200);
        write16(io.data(), 0x1040);
        for (unsigned i = 0; i < 128; ++i) write16(oam.data() + i * 8, 0x200);
        for (unsigned i = 1; i < 16; ++i) write16(pal.data() + 0x2a0 + i * 2, i * 1091);
    }
    Memory memory() const { return {iwram.data(),oam.data(),vram.data(),pal.data(),io.data(),
        iwram.size(),oam.size(),vram.size(),pal.size(),io.size()}; }
    void object(unsigned index, unsigned a0, unsigned a1, unsigned a2 = 0x5000) {
        auto* p = oam.data() + index * 8; write16(p,a0); write16(p+2,a1); write16(p+4,a2);
    }
    void texel(unsigned tile, unsigned x, unsigned y, unsigned color) {
        auto& b = vram[0x10000 + tile * 32 + y * 4 + x / 2];
        const unsigned shift = (x & 1) * 4;
        b = std::uint8_t((b & ~(15u << shift)) | (color << shift));
    }
    void tile(unsigned index, unsigned color) {
        for (unsigned y=0;y<8;++y) for(unsigned x=0;x<8;++x) texel(index,x,y,color);
    }
    void pattern() {
        for (unsigned y=0;y<8;++y) for(unsigned x=0;x<8;++x) texel(0,x,y,(y*8+x)%15+1);
        object(0,24,12);
    }
};

void decode_cases() {
    Fixture f; f.pattern();
    auto image = capture(f.memory());
    require(image.visible && image.room==3 && image.world_x==112 && image.world_y==224,
            "normal object uses room and world coordinates");
    require(image.image.width==8 && image.image.height==8,"normal tile dimensions");
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x)
        require(pixel(image.image,x,y)==(y*8+x)%15+1,"normal nibble order");
    for(unsigned flags:{0u,0x1000u,0x2000u,0x3000u}) {
        f.object(0,24,12|flags); const auto flipped=capture(f.memory());
        for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x) {
            const unsigned tx=(flags&0x1000)?7-x:x, ty=(flags&0x2000)?7-y:y;
            require(pixel(flipped.image,x,y)==(ty*8+tx)%15+1,"horizontal/vertical flips");
        }
    }
    f.object(0,24,12); f.object(1,24,12,0x5001); f.tile(1,15);
    require(pixel(capture(f.memory()).image,0,0)==1,"lower OAM index wins overlap");
    f.texel(0,0,0,0);
    require(pixel(capture(f.memory()).image,0,0)==15,"transparent texel reveals lower object");
    f.object(0,0x200,0); f.object(1,0x200,0); f.object(2,24,12,0x50c2); f.tile(0xc2,6);
    require(!capture(f.memory()).visible,"Baby Mario bank-5 art is excluded");
    f.object(2,0x200,0); f.object(0,24,12); f.tile(0,0); f.texel(0,3,5,9);
    image=capture(f.memory());
    require(image.visible && image.world_x==115 && image.world_y==229 &&
        image.image.width==1 && image.image.height==1 && image.image.pixels[0]==9,"transparent border crop and odd nibble");
    require(image.image.palette[9]==read16(f.pal.data()+0x2a0+18) && image.image.palette[1]==0 &&
        image.image.palette[0]==0,"unused palette entries canonicalized");
    for(std::size_t i=1;i<image.image.pixels.size();++i) require(image.image.pixels[i]==0,"unused pixels zero");
    f.object(0,250,508); f.tile(0,5); image=capture(f.memory());
    require(image.world_x==96 && image.world_y==194,"signed hardware X/Y wrapping");
    f.object(0,24,12); f.object(1,24,200); require(!capture(f.memory()).visible,"disjoint oversized atlas rejected");
}

void mapping_and_affine() {
    Fixture f;
    f.object(0,24,12|0x4000);
    for(unsigned tile=0;tile<4;++tile) f.tile(tile,tile+1);
    auto image=capture(f.memory());
    require(image.image.width==16 && image.image.height==16,"16x16 dimensions");
    require(pixel(image.image,0,0)==1 && pixel(image.image,8,0)==2 &&
        pixel(image.image,0,8)==3 && pixel(image.image,8,8)==4,"1D mapping row stride");
    write16(f.io.data(),0x1000); f.tile(32,7); f.tile(33,8); image=capture(f.memory());
    require(pixel(image.image,0,8)==7 && pixel(image.image,8,8)==8,"2D mapping row stride");
    f.pattern(); f.object(0,24|0x100,12);
    write16(f.oam.data()+6,256);write16(f.oam.data()+14,0);
    write16(f.oam.data()+22,0);write16(f.oam.data()+30,256);
    image=capture(f.memory());
    require(image.visible && image.world_x==112 && image.world_y==224,"affine identity position");
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x)
        require(pixel(image.image,x,y)==(y*8+x)%15+1,"affine identity texels");
    f.object(0,24|0x300,12); image=capture(f.memory());
    require(image.world_x==116 && image.world_y==228 && image.image.width==8 &&
        image.image.height==8,"affine double-size transparent border is cropped");
    f.object(0,24|0x100,12); write16(f.oam.data()+6,0);write16(f.oam.data()+14,256);
    write16(f.oam.data()+22,unsigned(-256));write16(f.oam.data()+30,0);
    image=capture(f.memory());
    require(image.world_x==113 && image.world_y==224 && image.image.width==7 &&
        image.image.height==8,"affine quarter turn center and crop");
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<7;++x)
        require(pixel(image.image,x,y)==((7-x)*8+y)%15+1,"affine quarter turn texels");
}

void guards() {
    Fixture f; f.pattern();
    for(unsigned offset:{0x6db2u,0x48b5u,0x4d46u,0x7294u}) {
        f.iwram[offset]=1; const auto image=capture(f.memory());
        require(!image.visible && image.room==3,"transformation/inventory/pause/Fuzzy hides pose"); f.iwram[offset]=0;
    }
    f.iwram[0x6b05]=0x0c;require(!capture(f.memory()).visible,"transition hides pose");f.iwram[0x6b05]=0x0d;
    write16(f.io.data(),0x1080);require(!capture(f.memory()).visible,"forced blank hides pose");
    write16(f.io.data(),0xf040);write16(f.io.data()+0x48,0x1010);write16(f.io.data()+0x4a,0x1010);
    require(capture(f.memory()).visible,"all-OBJ windows permitted");
    for(unsigned offset:{0x48u,0x49u,0x4au,0x4bu}) {
        f.io[offset]=0;require(!capture(f.memory()).visible,"restricted OBJ region hides pose");f.io[offset]=0x10;
    }
    write16(f.io.data()+0x50,0x90);write16(f.io.data()+0x54,8);
    require(!capture(f.memory()).visible,"brightness transition hides pose");
    write16(f.io.data()+0x50,0);require(capture(f.memory()).visible,"unused brightness coefficient harmless");
    f.object(0,24|0x1000,12);write16(f.io.data()+0x4c,0x1100);
    require(!capture(f.memory()).visible,"OBJ mosaic hides unsupported pose");
    f.object(0,24,12);write16(f.io.data()+0x4c,0);
    auto memory=f.memory();memory.vram_size=0x10000;require(!capture(memory).visible,"short VRAM rejected");
    memory=f.memory();memory.io_size=0x50;require(!capture(memory).visible,"short IO rejected");
    memory=f.memory();memory.iwram=nullptr;require(!capture(memory).visible,"missing RAM rejected");
    f.object(0,160,12);require(!capture(f.memory()).visible,"hidden y=160 OAM sentinel excluded");
}

void run_and_projection_cases() {
    GhostImage image;image.width=64;image.height=64;
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x)set_pixel(image,x,y,1);
    std::array<Run,kMaxRuns> output{};std::size_t count=0;
    require(runs(image,output,count)&&count==1&&output[0].width==64&&output[0].height==64,"solid texels merge to one rectangle");
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x)set_pixel(image,x,y,1+((x+y)&1));
    require(!runs(image,output,count)&&count==0,"pathological checkerboard rejected before draw");
    const Run run{0,0,8,8,1};Rectangle r;
    auto p=projection(960,640,0,0,960,640,240,160,true,false);
    require(project(p,run,112,224,100,200,r)&&r.x==48&&r.y==96&&r.width==32&&r.height==32,"Original projection");
    p=projection(2400,1080,0,0,2400,1080,240,160,true,false);
    require(project(p,run,112,224,100,200,r)&&r.x==120&&r.y==162&&r.width==80&&r.height==54,"Stretch has independent axis scales");
    p=projection(1424,640,0,0,1424,640,356,160,false,false);
    require(project(p,run,80,224,100,200,r)&&r.x==152,"panorama adds native viewport origin");
    p=projection(1424,640,0,0,1424,640,356,160,true,false);
    require(!project(p,run,80,224,100,200,r),"pillarbox forbids black margin paint");
    p=projection(960,640,0,0,960,640,240,160,true,true);
    require(!project(p,run,112,200,100,200,r),"top cinematic bar remains opaque");
    require(project(p,run,112,212,100,200,r)&&r.y==64&&r.height==16,"cinematic edge is clipped precisely");
    require(!project(p,run,112,352,100,200,r),"bottom cinematic bar remains opaque");
    require(!project(p,run,INT_MAX,INT_MIN,100,200,r),"extreme persisted coordinates cannot overflow");
    p=projection(960,640,-20,-10,960,640,240,160,false,false);
    require(project(p,run,104,201,100,200,r)&&r.x==0&&r.y==0,"drawable edges clipped");
    require(!projection(960,640,0,0,960,640,240,0,false,false).valid,"unsupported view height rejected");
}

void context_latching() {
    ContextLatch latch;int bus_a=0,bus_b=0;
    const Scene first{true,3,100,200},next{true,3,110,210},paused{false,3,110,210};
    require(latch.publish(first,false,&bus_a,10,1)&&!latch.displayed().valid,"first publication has no latched image context");
    require(latch.publish(next,true,&bus_a,11,1)&&latch.displayed().valid&&
        latch.displayed().scene.camera_x==100&&!latch.displayed().pillarbox,"display camera and margins belong to completed image");
    require(!latch.publish(paused,true,&bus_a,11,1)&&latch.displayed().scene.visible&&
        latch.displayed().scene.camera_x==100,"duplicate resize/120 present cannot advance context");
    require(latch.publish(paused,true,&bus_a,12,1)&&latch.displayed().scene.visible&&
        latch.displayed().scene.camera_x==110,"pause entrance keeps completed gameplay context");
    require(latch.publish(paused,true,&bus_a,13,1)&&!latch.displayed().scene.visible,"paused completed image hides ghost");
    require(latch.publish(first,false,&bus_a,14,1)&&!latch.displayed().scene.visible,"resume does not paint ahead of latched frame");
    require(latch.publish(first,false,&bus_a,15,1)&&latch.displayed().scene.visible,"first completed resumed image becomes visible");
    require(latch.publish(next,false,&bus_a,15,2)&&!latch.displayed().valid,"state restoration clears context even at equal frame number");
    require(latch.publish(first,false,&bus_a,16,2)&&latch.displayed().scene.camera_x==110,"post-restore context resumes normally");
    require(latch.publish(first,false,&bus_b,17,2)&&!latch.displayed().valid,"source switch never reuses other bus camera");
    require(latch.publish(first,false,&bus_b,16,2)&&!latch.displayed().valid,"frame rewind invalidates history");
    require(!latch.publish(first,false,nullptr,17,2)&&!latch.displayed().valid,"missing bus clears history");
}

template<std::size_t N>
void load(std::array<std::uint8_t,N>& bytes, const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);
    require(bool(file.read(reinterpret_cast<char*>(bytes.data()),N)),"private capture loaded");
    require(file.peek()==std::char_traits<char>::eof(),"private capture exact size");
}

void native_capture(const std::filesystem::path& source,const std::filesystem::path& output) {
    Fixture f;load(f.iwram,source/"iwram.bin");load(f.oam,source/"oam.bin");load(f.vram,source/"vram.bin");
    load(f.pal,source/"pal.bin");load(f.io,source/"io.bin");
    const auto image=capture(f.memory());require(image.visible,"native Yoshi captured");
    std::array<Run,kMaxRuns> rectangles{};std::size_t count=0;
    require(runs(image.image,rectangles,count),"native Yoshi fits bounded overlay");
    std::filesystem::create_directories(output);
    std::ofstream rgba(output/"native-yoshi.rgba",std::ios::binary);
    unsigned opaque=0;
    for(unsigned y=0;y<image.image.height;++y)for(unsigned x=0;x<image.image.width;++x) {
        const auto index=pixel(image.image,x,y);
        const auto color=image.image.palette[index];
        const std::array<std::uint8_t,4> bytes{{std::uint8_t((color&31)*255/31),
            std::uint8_t(((color>>5)&31)*255/31),std::uint8_t(((color>>10)&31)*255/31),std::uint8_t(index?255:0)}};
        rgba.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(index)++opaque;
    }
    volatile std::uint64_t checksum=0;
    constexpr unsigned iterations=20000;
    const auto start=std::chrono::steady_clock::now();
    for(unsigned i=0;i<iterations;++i){const auto pose=capture(f.memory());checksum+=pose.image.pixels[i%2048];}
    const auto midpoint=std::chrono::steady_clock::now();
    for(unsigned i=0;i<iterations;++i){runs(image.image,rectangles,count);checksum+=count;}
    const auto finish=std::chrono::steady_clock::now();
    const double capture_ns=double(std::chrono::duration_cast<std::chrono::nanoseconds>(midpoint-start).count())/iterations;
    const double runs_ns=double(std::chrono::duration_cast<std::chrono::nanoseconds>(finish-midpoint).count())/iterations;
    std::ofstream report(output/"native-visual-component.json");
    report<<"{\"scope\":\"host component from private real TCP OAM/VRAM; not a phone FPS result\","
        <<"\"width\":"<<unsigned(image.image.width)<<",\"height\":"<<unsigned(image.image.height)
        <<",\"world_x\":"<<image.world_x<<",\"world_y\":"<<image.world_y<<",\"room\":"<<image.room
        <<",\"opaque_texels\":"<<opaque<<",\"draw_calls\":"<<count<<",\"maximum_draw_calls\":"<<kMaxRuns
        <<",\"capture_mean_ns\":"<<capture_ns<<",\"run_build_mean_ns\":"<<runs_ns
        <<",\"iterations\":"<<iterations<<",\"checksum\":"<<checksum<<"}\n";
    std::cout<<"native capture "<<unsigned(image.image.width)<<'x'<<unsigned(image.image.height)
        <<" opaque="<<opaque<<" draw_calls="<<count<<" capture_ns="<<capture_ns<<" runs_ns="<<runs_ns<<'\n';
}
}  // namespace

int main(int argc,char** argv) {
    decode_cases();mapping_and_affine();guards();run_and_projection_cases();context_latching();
    if(argc==3)native_capture(argv[1],argv[2]);
    else require(argc==1,"usage: ghost_visual_test [private-input output]");
    std::cout<<"PASS ghost visual checks="<<checks<<'\n';
}
