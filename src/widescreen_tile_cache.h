#pragma once

#include "widescreen_tiles.h"
#include <array>
#include <cstdint>

namespace sma3::wide {

// The PPU samples a regular background from left to right while guest memory
// is stable for that scanline. An authored 8x8 tile entry is identical for
// adjacent pixels in the same world cell. Reuse only that contiguous run:
// unlike a frame-wide map cache, the next raster row always reads live data.
//
// The caller must begin a fresh frame after a load, reset, or new raster frame.
// A repeated/skipped/reversed input X also starts a new run, so re-rendering a
// row cannot reuse its final lookup from an earlier invocation. No guest
// pointers, pixels, or decoded maps are retained.
class RasterTileCache {
public:
    void begin_frame() noexcept {
        for (auto& entry : entries_) entry.valid = false;
    }
    void clear() noexcept { begin_frame(); }

    template<class Reader>
    bool tile(Reader read, int layer, int raster_x, int raster_y,
              int world_x, int world_y, unsigned image_rows,
              uint16_t* output) {
        if (!output || layer < 0 || layer > 3) return false;

        auto& entry = entries_[unsigned(layer)];
        // Unsigned groups preserve the boundaries of negative coordinates
        // without implementation-defined division or overflowing INT_MIN.
        const uint32_t cell_x = uint32_t(world_x) & ~uint32_t(7);
        const uint32_t cell_y = uint32_t(world_y) & ~uint32_t(7);
        const unsigned rows = layer < 2 ? 0u : image_rows;
        const bool hit = entry.valid && entry.next_x == int64_t(raster_x) &&
            entry.raster_y == raster_y && entry.cell_x == cell_x &&
            entry.cell_y == cell_y && entry.image_rows == rows;
        if (!hit) {
            uint16_t value = 0;
            const bool available = layer < 2
                ? foreground_tile(read, world_x, world_y, layer, &value)
                : background_tile(read, world_x, world_y, layer, rows, &value);
            entry.raster_y = raster_y;
            entry.cell_x = cell_x;
            entry.cell_y = cell_y;
            entry.image_rows = rows;
            entry.value = value;
            entry.available = available;
            entry.valid = true;
        }
        entry.next_x = int64_t(raster_x) + 1;
        if (entry.available) *output = entry.value;
        return entry.available;
    }

private:
    struct Entry {
        int64_t next_x = 0;
        int raster_y = 0;
        uint32_t cell_x = 0;
        uint32_t cell_y = 0;
        unsigned image_rows = 0;
        uint16_t value = 0;
        bool available = false;
        bool valid = false;
    };
    std::array<Entry, 4> entries_{};
};

static_assert(sizeof(RasterTileCache) <= 128,
              "Raster tile cache must remain fixed and small");

} // namespace sma3::wide
