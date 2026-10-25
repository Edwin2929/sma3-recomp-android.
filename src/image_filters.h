#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace sma3 {

// Stable persisted/JNI IDs. These affect presentation only, never guest pixels.
enum class ImageFilter : int {
    Balanced = 0, Nearest = 1, Bilinear = 2, Scale2x = 3, Scale3x = 4, Crt = 5,
    Cinematic = 6
};
constexpr ImageFilter validated_image_filter(int value) {
    return value >= 0 && value <= 6 ? static_cast<ImageFilter>(value)
                                 : ImageFilter::Balanced;
}
inline std::atomic<ImageFilter> image_filter{ImageFilter::Balanced};

// Bound allocations and integer pitches before examining either pixel buffer.
// This is far above SMA3's 240/356 x 160 presentation, including tiny/odd inputs
// used to test edge replication. A rejected image falls back to the raw texture.
inline bool filtered_image_size(int width, int height, int factor,
                                int& out_width, int& out_height,
                                std::size_t& bytes) {
    out_width = out_height = 0;
    bytes = 0;
    if (width <= 0 || height <= 0 || (factor != 2 && factor != 3) ||
        width > 4096 || height > 4096) return false;
    const std::uint64_t w = std::uint64_t(width) * factor;
    const std::uint64_t h = std::uint64_t(height) * factor;
    constexpr std::uint64_t max_pixels = 16u * 1024u * 1024u;
    if (w * h > max_pixels || w * 3u > std::numeric_limits<int>::max())
        return false;
    out_width = static_cast<int>(w);
    out_height = static_cast<int>(h);
    bytes = static_cast<std::size_t>(w * h * 3u);
    return true;
}

// Independent RGB24 implementation of Andrea Mazzoleni's published Scale2x /
// Scale3x rules, including nearest-edge replication. Algorithm specification:
// https://www.scale2x.it/algorithm . No external filter library is bundled.
// source and destination must be disjoint; neither source nor its padding is
// modified. RGB byte access avoids unaligned loads and host-endian assumptions.
inline bool scale_pixels_rgb24(const std::uint8_t* source, std::size_t source_bytes,
                               int width, int height, int factor,
                               std::uint8_t* destination, std::size_t destination_bytes) {
    int out_width = 0, out_height = 0;
    std::size_t required = 0;
    if (!source || !destination ||
        !filtered_image_size(width, height, factor, out_width, out_height, required))
        return false;
    const std::size_t input_bytes = std::size_t(width) * height * 3u;
    if (source_bytes < input_bytes || destination_bytes < required) return false;
    const auto input_start = reinterpret_cast<std::uintptr_t>(source);
    const auto output_start = reinterpret_cast<std::uintptr_t>(destination);
    if (input_start > std::numeric_limits<std::uintptr_t>::max() - input_bytes ||
        output_start > std::numeric_limits<std::uintptr_t>::max() - required ||
        (input_start < output_start + required && output_start < input_start + input_bytes))
        return false;
    const auto load = [source, width](int x, int y) {
        const auto* p = source + (std::size_t(y) * width + x) * 3u;
        return (std::uint32_t(p[0]) << 16) | (std::uint32_t(p[1]) << 8) | p[2];
    };
    const auto store = [destination, out_width](int x, int y, std::uint32_t color) {
        auto* p = destination + (std::size_t(y) * out_width + x) * 3u;
        p[0] = static_cast<std::uint8_t>(color >> 16);
        p[1] = static_cast<std::uint8_t>(color >> 8);
        p[2] = static_cast<std::uint8_t>(color);
    };
    for (int y = 0; y < height; ++y) {
        const int above = std::max(0, y - 1), below = std::min(height - 1, y + 1);
        for (int x = 0; x < width; ++x) {
            const int left = std::max(0, x - 1), right = std::min(width - 1, x + 1);
            const auto center = load(x, y);
            const auto north = load(x, above), south = load(x, below);
            const auto west = load(left, y), east = load(right, y);
            std::uint32_t out[9];
            std::fill_n(out, factor * factor, center);
            if (north != south && west != east) {
                out[0] = west == north ? west : center;
                out[factor - 1] = north == east ? east : center;
                out[(factor - 1) * factor] = west == south ? west : center;
                out[factor * factor - 1] = south == east ? east : center;
                if (factor == 3) {
                    const auto nw = load(left, above), ne = load(right, above);
                    const auto sw = load(left, below), se = load(right, below);
                    out[1] = ((west == north && center != ne) ||
                              (north == east && center != nw)) ? north : center;
                    out[3] = ((west == north && center != sw) ||
                              (west == south && center != nw)) ? west : center;
                    out[5] = ((north == east && center != se) ||
                              (south == east && center != ne)) ? east : center;
                    out[7] = ((west == south && center != se) ||
                              (south == east && center != sw)) ? south : center;
                }
            }
            for (int dy = 0; dy < factor; ++dy)
                for (int dx = 0; dx < factor; ++dx)
                    store(x * factor + dx, y * factor + dy, out[dy * factor + dx]);
        }
    }
    return true;
}

} // namespace sma3
