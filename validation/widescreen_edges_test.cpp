#include "../src/widescreen_edges.h"
#include <climits>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

struct Memory {
    std::map<uint32_t, uint8_t> bytes;
    unsigned reads = 0;

    void put(uint32_t address, uint32_t value, unsigned width) {
        for (unsigned i = 0; i < width; ++i)
            bytes[address + i] = uint8_t(value >> (i * 8));
    }

    uint32_t read(uint32_t address, unsigned width) {
        require(width == 1 || width == 2 || width == 4, "unexpected read width");
        ++reads;
        uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i) {
            const auto found = bytes.find(address + i);
            if (found == bytes.end())
                throw std::runtime_error("unguarded source read at " +
                                         std::to_string(address + i));
            value |= uint32_t(found->second) << (i * 8);
        }
        return value;
    }

    Memory() {
        put(0x03007010, 0x0200000c, 4);
        put(0x081bad20 + 0x12 * 4, 0x08200000, 4);
        put(0x081bc444 + 0x12 * 4, 0x08210000, 4);
        // Distinct native palettes, every H/V flip combination and every
        // layer-ownership mask, all independently of world quadrant selection.
        for (unsigned id = 0; id < 256; ++id) {
            put(0x08210000 + id, id & 15, 1);
            for (unsigned quadrant = 0; quadrant < 4; ++quadrant) {
                const unsigned entry = ((id * 4 + quadrant) & 0x3ff) |
                    ((id & 15) << 12) | ((id & 1) << 10) | ((id & 2) << 10);
                put(0x08200000 + id * 8 + quadrant * 2, entry, 2);
            }
        }
        // Only the two real map edges exist in this synthetic reader. Any
        // attempt to inspect an interior column or an out-of-map row fails.
        for (unsigned side = 0; side < 2; ++side) {
            for (unsigned screen_y = 0; screen_y < 8; ++screen_y) {
                const unsigned index = 1 + screen_y * 2 + side;
                put(0x0201b800 + screen_y * 16 + (side ? 15 : 0),
                    index | 0x80, 1); // camera bit is not a physical wall
                for (unsigned cell_y = 0; cell_y < 16; ++cell_y) {
                    for (unsigned column = 0; column < 2; ++column) {
                        const unsigned id = ((screen_y * 16 + cell_y) * 4 +
                                             side * 2 + column) & 255;
                        const unsigned cell = cell_y * 16 +
                                              (side ? 14 : 0) + column;
                        put(0x0200000c + index * 512 + cell * 2, 0x1200 | id, 2);
                    }
                }
            }
        }
    }
};

// Reference expressed in signed 64-bit pixel coordinates, independently of
// the cache's unsigned bit arithmetic and hardware-tile phase calculation.
int reflected_source(int x, bool* reflect) {
    const bool right = x >= 4096;
    const int64_t distance = right ? int64_t(x) - 4096 : -int64_t(x) - 1;
    const int phase = int(distance % 64);
    *reflect = phase < 32;
    const int inward = phase < 32 ? phase : 63 - phase;
    return right ? 4095 - inward : inward;
}
} // namespace

int main() {
    Memory memory;
    auto read = [&](uint32_t address, unsigned width) {
        return memory.read(address, width);
    };
    sma3::wide::EdgeTileCache cache;
    unsigned checks = 0;
    uint16_t output = 0xdead;

    // Guards must happen before any reader call and leave the result alone.
    for (int x : {0, 1, 239, 240, 1024, 4095}) {
        require(!cache.tile(read, 0, x, 0, &output), "in-map scenery changed");
        require(output == 0xdead, "unhandled lookup wrote output");
    }
    for (int y : {INT_MIN, -1, 2048, INT_MAX})
        require(!cache.tile(read, 1, -1, y, &output), "invalid Y accepted");
    for (int layer : {INT_MIN, -1, 2, 3, INT_MAX})
        require(!cache.tile(read, layer, -1, 0, &output), "invalid layer accepted");
    require(!cache.tile(read, 1, -1, 0, nullptr), "null output accepted");
    require(memory.reads == 0, "guarded lookup touched guest memory");

    // All subpixel phases, quadrants, palettes and native flip combinations,
    // both ends and several complete 64px reflection periods.
    for (int y = 0; y < 2048; y += 7) {
        for (int distance = 0; distance < 192; ++distance) {
            for (int x : {-1 - distance, 4096 + distance}) {
                bool reflect;
                const int source = reflected_source(x, &reflect);
                for (int layer : {0, 1}) {
                    uint16_t expected;
                    require(sma3::wide::foreground_tile(read, source, y, layer,
                                                       &expected), "oracle failed");
                    expected ^= reflect ? 0x400 : 0;
                    require(cache.tile(read, layer, x, y, &output), "edge missing");
                    require(output == expected, "entry, flip or layer mismatch");
                    // The native PPU chooses destination x&7 before HFlip.
                    // Compare the actual sampled native pixel, not just IDs.
                    const unsigned destination_phase = unsigned(x) & 7;
                    const unsigned cache_pixel = destination_phase ^
                                                  ((output & 0x400) ? 7 : 0);
                    const unsigned original_flip = (expected ^
                        (reflect ? 0x400 : 0)) & 0x400;
                    const unsigned source_pixel = (unsigned(source) & 7) ^
                                                   (original_flip ? 7 : 0);
                    require(cache_pixel == source_pixel, "reflection seam phase");
                    ++checks;
                }
            }
        }
    }
    for (int x : {INT_MIN, INT_MIN + 1, -4097, -4096, -65, -64, -33, -32,
                  -9, -8, -1, 4096, 4103, 4127, 4128, 4159, 4160,
                  INT_MAX - 1, INT_MAX}) {
        for (int y : {0, 7, 8, 15, 16, 255, 256, 2047}) {
            bool reflect;
            const int source = reflected_source(x, &reflect);
            uint16_t expected;
            require(sma3::wide::foreground_tile(read, source, y, 1, &expected),
                    "extreme oracle failed");
            require(cache.tile(read, 1, x, y, &output), "extreme lookup failed");
            require(output == uint16_t(expected ^ (reflect ? 0x400 : 0)),
                    "extreme coordinate mismatch");
            ++checks;
        }
    }

    // A row resolves both layers once. Any later pixel or layer lookup in the
    // row must be memory-free; a different Y phase in the same 8px row too.
    cache.begin_frame();
    memory.reads = 0;
    require(cache.tile(read, 0, -1, 48, &output), "first lazy row failed");
    const unsigned row_reads = memory.reads;
    require(row_reads > 0 && row_reads <= 64, "unbounded row work");
    for (int x = -1; x >= -512; --x)
        for (int layer : {0, 1})
            require(cache.tile(read, layer, x, 55, &output), "cached row failed");
    require(memory.reads == row_reads, "per-pixel source read");
    require(cache.tile(read, 1, 4096, 48, &output), "other edge missing");
    require(memory.reads > row_reads && memory.reads <= 2 * 64,
            "sides alias or work is unbounded");

    // A native source change is observed only in the next frame. This models
    // terrain animation/state loads without allowing stale multi-frame art.
    cache.begin_frame();
    require(cache.tile(read, 1, -1, 0, &output), "epoch setup failed");
    const uint16_t old = output;
    memory.put(0x08200000, 0xbabc, 2);
    require(cache.tile(read, 1, -1, 0, &output) && output == old,
            "row did not retain its frame snapshot");
    cache.begin_frame();
    require(cache.tile(read, 1, -1, 0, &output) && output == (0xbabc ^ 0x400),
            "new frame retained stale tiles");

    // Invalid source pointers cannot partially expose a row. Failures are
    // cached, cleared on the next frame and recovered without guest writes.
    for (const auto& broken : {
            std::pair<uint32_t, uint32_t>{0x03007010, 0},
            {0x081bc444 + 0x12 * 4, 0x07ffffff},
            {0x081bc444 + 0x12 * 4, 0x08400000},
            {0x081bad20 + 0x12 * 4, 0x07ffffff},
            {0x081bad20 + 0x12 * 4, 0x08400000}}) {
        const uint32_t previous = read(broken.first, 4);
        memory.put(broken.first, broken.second, 4);
        cache.clear();
        output = 0xdead;
        require(!cache.tile(read, 1, -1, 32, &output), "bad pointer accepted");
        require(output == 0xdead, "partial row escaped");
        const unsigned failed_reads = memory.reads;
        require(!cache.tile(read, 0, -8, 39, &output), "failed row changed state");
        require(memory.reads == failed_reads, "failed row retried every pixel");
        memory.put(broken.first, previous, 4);
        cache.begin_frame();
        require(cache.tile(read, 1, -1, 32, &output), "valid frame did not recover");
        ++checks;
    }

    std::cout << "PASS: " << checks
              << " reflected edge/quadrant/flip/extreme cases; native map guards, "
                 "lazy read budget, frame reset and corrupt-pointer recovery; "
              << sizeof(cache) << "-byte cache\n";
}
