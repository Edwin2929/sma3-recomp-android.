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

namespace detail {
inline std::uint32_t load_rgb24(const std::uint8_t* p) {
    return (std::uint32_t(p[0]) << 16) | (std::uint32_t(p[1]) << 8) | p[2];
}
inline void store_rgb24(std::uint8_t* p, std::uint32_t color) {
    p[0] = static_cast<std::uint8_t>(color >> 16);
    p[1] = static_cast<std::uint8_t>(color >> 8);
    p[2] = static_cast<std::uint8_t>(color);
}
// Specialize the two fixed output layouts once per image. Row pointers avoid
// per-output-pixel coordinate products and the temporary generic 3x3 block;
// the original equality rules and nearest-edge replication remain unchanged.
template<int Factor>
inline void scale_rgb24_rows(const std::uint8_t* source, int width, int height,
                             std::uint8_t* destination) {
    const std::size_t pitch = std::size_t(width) * 3u;
    const std::size_t output_pitch = pitch * Factor;
    for (int y = 0; y < height; ++y) {
        const auto* row = source + std::size_t(y) * pitch;
        const auto* above = y == 0 ? row : row - pitch;
        const auto* below = y + 1 == height ? row : row + pitch;
        auto* out0 = destination + std::size_t(y) * Factor * output_pitch;
        auto* out1 = out0 + output_pitch;
        auto* out2 = Factor == 3 ? out1 + output_pitch : nullptr;
        for (int x = 0; x < width; ++x) {
            const int left = x == 0 ? 0 : -3;
            const int right = x + 1 == width ? 0 : 3;
            const auto center = load_rgb24(row);
            const auto north = load_rgb24(above), south = load_rgb24(below);
            const auto west = load_rgb24(row + left), east = load_rgb24(row + right);
            std::uint32_t nw = center, ne = center, sw = center, se = center;
            const bool edge = north != south && west != east;
            if (edge) {
                nw = west == north ? west : center;
                ne = north == east ? east : center;
                sw = west == south ? west : center;
                se = south == east ? east : center;
            }
            if constexpr (Factor == 2) {
                store_rgb24(out0, nw); store_rgb24(out0 + 3, ne);
                store_rgb24(out1, sw); store_rgb24(out1 + 3, se);
            } else {
                std::uint32_t n = center, w = center, e = center, s = center;
                if (edge) {
                    const auto a = load_rgb24(above + left), c = load_rgb24(above + right);
                    const auto g = load_rgb24(below + left), i = load_rgb24(below + right);
                    n = ((west == north && center != c) || (north == east && center != a)) ? north : center;
                    w = ((west == north && center != g) || (west == south && center != a)) ? west : center;
                    e = ((north == east && center != i) || (south == east && center != c)) ? east : center;
                    s = ((west == south && center != i) || (south == east && center != g)) ? south : center;
                }
                store_rgb24(out0, nw); store_rgb24(out0 + 3, n); store_rgb24(out0 + 6, ne);
                store_rgb24(out1, w); store_rgb24(out1 + 3, center); store_rgb24(out1 + 6, e);
                store_rgb24(out2, sw); store_rgb24(out2 + 3, s); store_rgb24(out2 + 6, se);
                out2 += 9;
            }
            row += 3; above += 3; below += 3;
            out0 += Factor * 3; out1 += Factor * 3;
        }
    }
}
} // namespace detail

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
    if (factor == 2) detail::scale_rgb24_rows<2>(source, width, height, destination);
    else detail::scale_rgb24_rows<3>(source, width, height, destination);
    return true;
}

} // namespace sma3
