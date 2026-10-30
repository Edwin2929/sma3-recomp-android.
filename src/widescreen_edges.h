#pragma once

#include "widescreen_tiles.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace sma3::wide {

// Presentation-only continuation beyond the two proven horizontal limits of
// the 4096x2048 level map. Empty map cells, camera stops and screen-table bit 7
// do not prove a wall and are deliberately not used to infer extra boundaries.
//
// The adjacent 32 pixels of BG0/1 form a small, reflected terrain band. The
// native tile entries retain palette, vertical flip and layer ownership. This
// never samples OBJ/HUD pixels, modifies the guest map or changes collision.
// Air in the source remains air, so the ordinary BG2/3 scenery shows through.
class EdgeTileCache {
public:
    // A fresh epoch is required before rendering each guest frame, including
    // after loading a state or changing sublevels. Resolving rows lazily also
    // supports a scroll value that changes between native scanlines.
    void begin_frame() { row_state_.fill(0); }
    void clear() { begin_frame(); }

    // Return false for the original map, unsupported coordinates or malformed
    // source data. The caller then retains its existing foreground fallback.
    // Reader must be a bounded, side-effect-free view of the current frame.
    template<class Reader>
    bool tile(Reader read, int layer, int x, int y, uint16_t* output) {
        if (!output || layer < 0 || layer > 1 || y < 0 || y >= kHeight ||
            (x >= 0 && x < kWidth)) return false;

        const unsigned side = x < 0 ? 0u : 1u;
        const unsigned row = unsigned(y) >> 3;
        const unsigned row_index = side * kRows + row;
        if (!row_state_[row_index]) resolve_row(read, side, row);
        if (row_state_[row_index] != 1) return false;

        // Unsigned conversion/complement also handles INT_MIN without signed
        // negation or overflowing subtraction. Each 64px period runs inward
        // for 32px, then outward; its boundary samples are exact reflections.
        const unsigned distance = side ? unsigned(x) - unsigned(kWidth)
                                       : ~unsigned(x);
        const unsigned phase = distance & 63u;
        const bool reflected = phase < 32u;
        const unsigned source = side
            ? (reflected ? 31u - phase : phase - 32u)
            : (reflected ? phase : 63u - phase);
        const unsigned column = source >> 3;
        const uint16_t entry = entries_[row_index * kEntriesPerRow +
                                        unsigned(layer) * kColumns + column];
        *output = uint16_t(entry ^ (reflected ? 0x0400u : 0u));
        return true;
    }

private:
    static constexpr int kWidth = 4096;
    static constexpr int kHeight = 2048;
    static constexpr unsigned kRows = unsigned(kHeight) / 8;
    static constexpr unsigned kColumns = 4;
    static constexpr unsigned kEntriesPerRow = 2 * kColumns;

    template<class Reader>
    void resolve_row(Reader read, unsigned side, unsigned row) {
        const unsigned row_index = side * kRows + row;
        // Cache failures too. A corrupt pointer must not trigger repeated
        // memory reads for every pixel or expose a partially resolved row.
        row_state_[row_index] = 2;
        const int source_x = side ? kWidth - 32 : 0;
        for (unsigned layer = 0; layer < 2; ++layer) {
            for (unsigned column = 0; column < kColumns; ++column) {
                uint16_t entry;
                if (!foreground_tile(read, source_x + int(column * 8),
                                     int(row * 8), int(layer), &entry)) return;
                entries_[row_index * kEntriesPerRow + layer * kColumns +
                         column] = entry;
            }
        }
        row_state_[row_index] = 1;
    }

    // 8 KiB of native tile entries and 512 bytes of row states; no heap, copied
    // framebuffer or per-pixel source reads once the requested row is cached.
    std::array<uint16_t, 2 * kRows * kEntriesPerRow> entries_{};
    std::array<uint8_t, 2 * kRows> row_state_{};
};

static_assert(sizeof(EdgeTileCache) <= 9 * 1024,
              "Panorama edge cache must stay bounded below 9 KiB");

} // namespace sma3::wide
