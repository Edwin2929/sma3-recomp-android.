// CPU helper benchmark on a frozen, user-owned GBAS v2 state and ROM.
// Both routes retain the existing exterior-edge cache and complete-BG3 path.
// Only the normal authored-tile lookup changes. This is not a PPU/GPU/FPS
// benchmark: it deliberately calls all enabled BGs in each lateral scanline,
// independently of the PPU's window/opacity decisions.
#include "../src/widescreen_edges.h"
#include "../src/widescreen_tile_cache.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <time.h>
#include <vector>

namespace {
using Bytes = std::vector<uint8_t>;
Bytes load(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("input unavailable");
    return Bytes(std::istreambuf_iterator<char>(file), {});
}
uint32_t u32(const Bytes& bytes, size_t offset) {
    if (offset > bytes.size() || bytes.size()-offset < 4)
        throw std::runtime_error("truncated state");
    return uint32_t(bytes[offset]) | (uint32_t(bytes[offset+1])<<8) |
        (uint32_t(bytes[offset+2])<<16) | (uint32_t(bytes[offset+3])<<24);
}
uint16_t u16(const uint8_t* bytes) {
    return uint16_t(bytes[0] | (unsigned(bytes[1])<<8));
}
double cpu_seconds() {
    timespec value{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value))
        throw std::runtime_error("thread CPU clock unavailable");
    return double(value.tv_sec) + double(value.tv_nsec)*1e-9;
}
struct Memory {
    Bytes rom, state;
    const uint8_t *ewram=nullptr, *iwram=nullptr, *io=nullptr;
    Memory(const char* rom_path,const char* state_path)
        :rom(load(rom_path)),state(load(state_path)) {
        if (state.size()<52 || std::memcmp(state.data(),"GBAS",4) || u32(state,4)!=2)
            throw std::runtime_error("unsupported state");
        size_t position=52;
        for (unsigned i=0;i<u32(state,48);++i) {
            const size_t bytes=u32(state,position+4);
            if (position+8>state.size() || bytes>state.size()-position-8)
                throw std::runtime_error("truncated section");
            if (!std::memcmp(state.data()+position,"BUS0",4)) {
                if (bytes<0x48000) throw std::runtime_error("truncated bus");
                ewram=state.data()+position+8; iwram=ewram+0x40000;
            }
            if (!std::memcmp(state.data()+position,"IO_0",4)) {
                if (bytes<0x60) throw std::runtime_error("truncated IO");
                io=state.data()+position+8;
            }
            position+=8+bytes;
        }
        if (!ewram || !iwram || !io) throw std::runtime_error("missing state section");
    }
};
template<bool Count> struct Reader {
    const Memory& memory;
    uint64_t reads=0;
    uint32_t operator()(uint32_t address,unsigned width) {
        if constexpr(Count) ++reads;
        const uint8_t* bytes=nullptr;
        size_t offset=0,size=0;
        if(address>=0x02000000 && address<0x02040000) {
            bytes=memory.ewram;offset=address-0x02000000;size=0x40000;
        } else if(address>=0x03000000 && address<0x03008000) {
            bytes=memory.iwram;offset=address-0x03000000;size=0x8000;
        } else if(address>=0x08000000 && address<0x08400000) {
            bytes=memory.rom.data();offset=address-0x08000000;size=memory.rom.size();
        }
        if(!bytes || offset+width>size) return 0;
        uint32_t value=0;
        for(unsigned i=0;i<width;++i)value|=uint32_t(bytes[offset+i])<<(8*i);
        return value;
    }
};
struct Provider {
    const Memory& memory;
    sma3::wide::EdgeTileCache edges;
    sma3::wide::RasterTileCache raster;
    std::array<unsigned,2> rows{};
    explicit Provider(const Memory& source):memory(source) {
        Reader<false> read{memory};
        for(unsigned i=0;i<2;++i) {
            const unsigned id=read(0x03004ba2+i*4,2);
            if(id>(i==0?0x20u:0x2fu))continue;
            const unsigned pointer=read((i==0?0x081675e4u:0x0816766cu)+id*4,4);
            if(pointer<0x08000000 || pointer>=0x083ffffc)continue;
            const unsigned header=read(pointer,4),bytes=header>>8;
            if((header&255)==0x10 && bytes>=64 && bytes<=8192 && bytes%64==0)
                rows[i]=bytes/64;
        }
    }
    void begin(bool cached) {
        edges.begin_frame();
        if(cached)raster.begin_frame();
    }
    template<bool Count>
    __attribute__((noinline)) uint32_t sample(Reader<Count>& reader,bool cached,
                                             int layer,int x,int y) {
        uint16_t value=0x60ff;
        if(layer==3 && sma3::wide::complete_background_map(
            layer,u16(memory.io+8+layer*2),rows[1])) return (2u<<16)|value;
        const int wx=x+int(int16_t(u16(memory.io+0x10+layer*4)));
        const int wy=y+int(int16_t(u16(memory.io+0x12+layer*4)));
        auto read=[&](uint32_t a,unsigned n){return reader(a,n);};
        if(layer<2 && (wx<0 || wx>=4096) && edges.tile(read,layer,wx,wy,&value))
            return (1u<<16)|value;
        const bool available=cached
            ?raster.tile(read,layer,x,y,wx,wy,layer<2?0u:rows[layer-2],&value)
            :layer<2?sma3::wide::foreground_tile(read,wx,wy,layer,&value)
                     :sma3::wide::background_tile(read,wx,wy,layer,rows[layer-2],&value);
        return (uint32_t(available)<<16)|value;
    }
};
template<bool Count>
uint64_t run(Provider& provider,Reader<Count>& reader,bool cached,unsigned frames,int width) {
    uint64_t sum=0;
    const int left=(width-240)/2;
    const uint16_t dispcnt=u16(provider.memory.io);
    for(unsigned frame=0;frame<frames;++frame) {
        provider.begin(cached);
        for(int y=0;y<160;++y)
            for(int layer=0;layer<4;++layer) {
                if(!(dispcnt&(0x100u<<layer)))continue;
                for(int x=-left;x<width-left;++x) {
                    if(x>=0 && x<240)continue;
                    sum+=provider.sample(reader,cached,layer,x,y);
                }
            }
    }
    return sum;
}
volatile uint64_t observed_checksum=0;
} // namespace

int main(int argc,char** argv) try {
    if(argc<3 || argc>5)throw std::runtime_error("usage: benchmark ROM STATE [frames=200] [width=356]");
    const unsigned frames=argc>3?unsigned(std::stoul(argv[3])):200;
    const int width=argc>4?std::stoi(argv[4]):356;
    if(!frames || frames>10000 || width<240 || width>576)
        throw std::runtime_error("invalid benchmark limits");
    Memory memory(argv[1],argv[2]); Provider baseline(memory),candidate(memory);
    // Compare every individual returned entry before timing, not just a hash.
    Reader<true> before{memory},after{memory};
    baseline.begin(false);candidate.begin(true);
    const int left=(width-240)/2;
    uint64_t compared=0;
    for(int y=0;y<160;++y)for(int layer=0;layer<4;++layer) {
        if(!(u16(memory.io)&(0x100u<<layer)))continue;
        for(int x=-left;x<width-left;++x) {
            if(x>=0 && x<240)continue;
            if(baseline.sample(before,false,layer,x,y)!=candidate.sample(after,true,layer,x,y))
                throw std::runtime_error("differential output mismatch");
            ++compared;
        }
    }
    Reader<false> timed(memory);
    const uint64_t expected=run(baseline,timed,false,1,width);
    if(run(candidate,timed,true,1,width)!=expected)throw std::runtime_error("checksum mismatch");
    run(baseline,timed,false,8,width);run(candidate,timed,true,8,width);
    std::array<std::array<double,7>,2> samples{};
    for(unsigned round=0;round<7;++round)
        for(unsigned order=0;order<2;++order) {
            const unsigned mode=order^(round&1);
            Provider& provider=mode?candidate:baseline;
            const double start=cpu_seconds();
            const uint64_t sum=run(provider,timed,mode!=0,frames,width);
            const double elapsed=cpu_seconds()-start;
            observed_checksum=sum;
            if(sum!=expected*frames)throw std::runtime_error("timed output changed");
            samples[mode][round]=elapsed*1000.0/frames;
        }
    std::cout<<std::fixed<<std::setprecision(6)
        <<"{\"kind\":\"frozen_scene_tile_lookup_cpu\",\"result\":\"PASS\",\"cache_bytes\":"
        <<sizeof(sma3::wide::RasterTileCache)<<",\"width\":"<<width
        <<",\"frames_per_sample\":"<<frames<<",\"compared_entries\":"<<compared
        <<",\"source_reads_before\":"<<before.reads<<",\"source_reads_after\":"<<after.reads
        <<",\"image_rows\":["<<baseline.rows[0]<<','<<baseline.rows[1]<<"],\"scenarios\":[";
    for(unsigned mode=0;mode<2;++mode) {
        auto sorted=samples[mode];std::sort(sorted.begin(),sorted.end());
        if(mode)std::cout<<',';
        std::cout<<"{\"name\":\""<<(mode?"cached":"baseline")<<"\",\"median_cpu_ms_per_frame\":"
            <<sorted[3]<<",\"cpu_samples_ms\":[";
        for(unsigned i=0;i<7;++i){if(i)std::cout<<',';std::cout<<samples[mode][i];}
        std::cout<<"]}";
    }
    std::cout<<"],\"limit\":\"Host thread CPU helper workload only; timing omits read counters and excludes PPU/GPU/Android/display FPS\"}\n";
} catch(const std::exception& error) {
    std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;
}
