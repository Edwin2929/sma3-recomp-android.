#include "../src/widescreen_tile_cache.h"
#include <array>
#include <climits>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
uint64_t checks = 0;
uint64_t lookups = 0;
void require(bool ok, const char* message) {
    ++checks;
    if (!ok) throw std::runtime_error(message);
}

struct Memory {
    std::vector<uint8_t> ewram = std::vector<uint8_t>(0x40000);
    std::vector<uint8_t> iwram = std::vector<uint8_t>(0x8000);
    std::vector<uint8_t> rom = std::vector<uint8_t>(0x400000);

    uint8_t* at(uint32_t address, unsigned width) {
        if (address >= 0x02000000 && uint64_t(address) + width <= 0x02040000)
            return ewram.data() + address - 0x02000000;
        if (address >= 0x03000000 && uint64_t(address) + width <= 0x03008000)
            return iwram.data() + address - 0x03000000;
        if (address >= 0x08000000 && uint64_t(address) + width <= 0x08400000)
            return rom.data() + address - 0x08000000;
        throw std::runtime_error("unbounded memory read");
    }
    uint32_t read(uint32_t address, unsigned width) {
        require(width == 1 || width == 2 || width == 4, "unexpected source width");
        const auto* bytes = at(address, width);
        uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i) value |= uint32_t(bytes[i]) << (8*i);
        return value;
    }
    void put(uint32_t address, uint32_t value, unsigned width) {
        auto* bytes = at(address, width);
        for (unsigned i = 0; i < width; ++i) bytes[i] = uint8_t(value >> (8*i));
    }
    Memory() {
        put(0x03007010, 0x0200000c, 4);
        for (unsigned screen = 0; screen < 128; ++screen)
            put(0x0201b800 + screen, ((screen*7 + 3) & 63) | 0x80, 1);
        for (unsigned index = 0; index < 64; ++index)
            for (unsigned cell = 0; cell < 256; ++cell)
                put(0x0200000c + index*512 + cell*2,
                    (index*823 + cell*101 + 0x100) & 0xffff, 2);
        for (unsigned group = 0; group < 256; ++group) {
            put(0x081bad20 + group*4, 0x08200000 + group*2048, 4);
            put(0x081bc444 + group*4, 0x081d0000 + group*256, 4);
            for (unsigned id = 0; id < 256; ++id) {
                put(0x081d0000 + group*256 + id, (id + group) & 15, 1);
                for (unsigned q = 0; q < 4; ++q)
                    put(0x08200000 + group*2048 + id*8 + q*2,
                        (group*257 + id*29 + q*101) & 0xffff, 2);
            }
        }
        for (unsigned layer = 0; layer < 2; ++layer)
            for (unsigned i = 0; i < 4096; ++i)
                put((layer ? 0x0201dc00 : 0x0201bc00) + i*2,
                    (i*37 + layer*13) & 0xffff, 2);
    }
};

bool reference(Memory& memory, uint64_t& reads, int layer, int x, int y,
               unsigned rows, uint16_t* output) {
    auto read = [&](uint32_t a, unsigned n) { ++reads; return memory.read(a, n); };
    return layer < 2 ? sma3::wide::foreground_tile(read, x, y, layer, output)
        : sma3::wide::background_tile(read, x, y, layer, rows, output);
}

void compare(Memory& memory, sma3::wide::RasterTileCache& cache,
             int layer, int raster_x, int raster_y, int world_x, int world_y,
             unsigned rows, uint64_t& before, uint64_t& after) {
    ++lookups;
    uint16_t expected = 0xdead, actual = 0xdead;
    const bool available = reference(memory, before, layer, world_x, world_y,
                                     rows, &expected);
    auto read = [&](uint32_t a, unsigned n) { ++after; return memory.read(a, n); };
    const bool cached = cache.tile(read, layer, raster_x, raster_y,
                                   world_x, world_y, rows, &actual);
    require(available == cached, "availability changed");
    require(expected == actual, "tile, flags, palette or fallback changed");
}
} // namespace

int main() try {
    Memory memory;
    sma3::wide::RasterTileCache cache;
    uint64_t before = 0, after = 0;
    const std::array<std::array<int,2>,8> scrolls{{
        {{0,0}},{{7,7}},{{15,15}},{{255,255}},{{256,256}},{{1025,1027}},
        {{4005,1987}},{{-13,-11}}
    }};
    // Actual raster order: scanline, layer, left margin, then right margin.
    // Widths include asymmetric margins, extrema, all eight pixel phases,
    // foreground ownership, both flip bits and every palette bank.
    for (unsigned rows : {1u,16u,32u,64u,128u})
        for (const auto& scroll : scrolls)
            for (int width : {347,356,576}) {
                cache.begin_frame();
                const int left = (width - 240)/2;
                for (int y = 0; y < 160; ++y)
                    for (int layer = 0; layer < 4; ++layer)
                        for (int x = -left; x < width-left; ++x) {
                            if (x >= 0 && x < 240) continue;
                            compare(memory,cache,layer,x,y,x+scroll[0],y+scroll[1],
                                    rows,before,after);
                        }
            }
    require(after < before/3, "raster did not reduce source reads");
    const uint64_t raster_before = before, raster_after = after;

    // Consecutive reads exactly eight pixels long share a single resolver.
    cache.clear(); before = after = 0;
    for (int x = 0; x < 8; ++x)
        compare(memory,cache,1,x,13,x,103,0,before,after);
    require(before == after*8 && after > 0, "contiguous tile did not resolve once");

    // A different native row must observe changes even inside the same world
    // tile. Repeated/backward/skipped X must do the same for repeated renders.
    auto native_map_entry = [&] {
        return 0x0200000c + (memory.read(0x0201b800,1)&63)*512;
    };
    cache.begin_frame(); before = after = 0;
    compare(memory,cache,1,0,0,0,0,0,before,after);
    memory.put(native_map_entry(),0x1234,2);
    compare(memory,cache,1,1,1,1,1,0,before,after);
    memory.put(native_map_entry(),0x5678,2);
    compare(memory,cache,1,1,1,1,1,0,before,after);
    memory.put(native_map_entry(),0x9abc,2);
    compare(memory,cache,1,-5,1,2,1,0,before,after);
    memory.put(native_map_entry(),0xdef0,2);
    compare(memory,cache,1,3,1,3,1,0,before,after);
    require(before == after, "new row or nonconsecutive X reused an entry");

    // Every frame/restore clears entries, including negative availability.
    cache.begin_frame(); before = after = 0;
    compare(memory,cache,1,0,0,0,0,0,before,after);
    memory.put(native_map_entry(),0xabcd,2);
    cache.begin_frame();
    compare(memory,cache,1,1,0,1,0,0,before,after);
    memory.put(0x03007010,0,4); cache.clear();
    compare(memory,cache,1,2,0,2,0,0,before,after);
    memory.put(0x03007010,0x0200000c,4); cache.clear();
    compare(memory,cache,1,3,0,3,0,0,before,after);
    require(before == after, "frame/restore reused a stale entry");

    // Unavailable tiles cache the failure without writing to the caller's
    // output, then re-read a repaired pointer on the very next raster row.
    memory.put(0x03007010,0,4); cache.clear(); before = after = 0;
    for (int x = 0; x < 8; ++x)
        compare(memory,cache,1,x,3,x,3,0,before,after);
    require(before == after*8 && after > 0, "negative result was not reused");
    memory.put(0x03007010,0x0200000c,4);
    compare(memory,cache,1,8,4,7,4,0,before,after);

    // Changing an authored background's height must invalidate its mapping.
    cache.clear();
    for (unsigned rows : {0u,1u,16u,32u,64u,128u,129u,UINT_MAX})
        compare(memory,cache,2,int(rows&7),1,0,1024,rows,before,after);

    // Scroll/cell changes during a row never reuse another tile. Arbitrary
    // input order, wrapped coordinates, large signed values and repeat frames.
    for (int layer = 0; layer < 4; ++layer) {
        cache.clear();
        for (int x : {INT_MIN,INT_MIN+1,-65537,-65536,-9,-8,-1,0,1,7,8,4095,4096,INT_MAX-1,INT_MAX})
            for (int y : {INT_MIN,-65537,-1,0,1,7,8,2047,2048,65535,65536,INT_MAX})
                compare(memory,cache,layer,x,y,x,y,32,before,after);
    }
    cache.clear();
    for (int x = 0; x < 32; ++x)
        compare(memory,cache,1,x,5,(x%2)*256+x,x%16,0,before,after);

    // Guarded calls must not query the reader or write through output.
    uint16_t sentinel = 0xbeef;
    auto forbidden = [&](uint32_t,unsigned)->uint32_t {
        throw std::runtime_error("invalid input touched guest memory");
    };
    for (int layer : {INT_MIN,-1,4,INT_MAX})
        require(!cache.tile(forbidden,layer,0,0,0,0,32,&sentinel),"invalid layer accepted");
    require(!cache.tile(forbidden,0,0,0,0,0,0,nullptr),"null output accepted");
    require(sentinel == 0xbeef,"invalid lookup overwrote output");

    std::cout << "{\"result\":\"PASS\",\"checks\":" << checks
              << ",\"differential_lookups\":" << lookups
              << ",\"cache_bytes\":" << sizeof(cache)
              << ",\"raster_source_reads_before\":" << raster_before
              << ",\"raster_source_reads_after\":" << raster_after << "}\n";
} catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
}
