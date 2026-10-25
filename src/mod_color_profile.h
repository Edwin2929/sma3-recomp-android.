#pragma once

// Immutable, presentation-only mods. The catalog worker publishes one matrix;
// each present() holds one snapshot for the complete frame, including frames
// synthesized by the 120 FPS blend. Guest pixels and savestates stay untouched.
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace sma3::mods {

struct ColorProfile {
    std::array<int, 9> matrix{100, 0, 0, 0, 100, 0, 0, 0, 100};

    bool identity() const noexcept {
        return matrix == std::array<int, 9>{100, 0, 0, 0, 100, 0, 0, 0, 100};
    }

    void map(const std::uint8_t* source, std::uint8_t* destination,
             std::size_t pixels) const noexcept {
        // Diagonal profiles map each byte independently. Small, frame-local
        // tables preserve the exact integer rounding/clipping, including
        // in-place calls, without a mutable cache or a heap allocation.
        // Tiny images and general matrices keep the original arithmetic path.
        if (pixels >= 256 && matrix[1] == 0 && matrix[2] == 0 &&
                matrix[3] == 0 && matrix[5] == 0 && matrix[6] == 0 && matrix[7] == 0) {
            std::array<std::array<std::uint8_t,256>,3> lut{};
            for(int channel=0;channel<3;++channel)
                for(int value=0;value<256;++value)
                    lut[channel][value] = static_cast<std::uint8_t>(
                        (std::clamp(matrix[channel*4]*value,0,25500)+50)/100);
            for(std::size_t pixel=0;pixel<pixels;++pixel) {
                const auto red=source[3*pixel], green=source[3*pixel+1], blue=source[3*pixel+2];
                destination[3*pixel]=lut[0][red];
                destination[3*pixel+1]=lut[1][green];
                destination[3*pixel+2]=lut[2][blue];
            }
            return;
        }
        for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
            // Read all channels first, so an in-place call is also valid.
            const int red = source[3 * pixel];
            const int green = source[3 * pixel + 1];
            const int blue = source[3 * pixel + 2];
            for (int channel = 0; channel < 3; ++channel) {
                const int sum = matrix[3 * channel] * red +
                    matrix[3 * channel + 1] * green +
                    matrix[3 * channel + 2] * blue;
                destination[3 * pixel + channel] = static_cast<std::uint8_t>(
                    (std::clamp(sum, 0, 25500) + 50) / 100);
            }
        }
    }
};

inline std::shared_ptr<const ColorProfile> active_profile;

inline void publish(std::shared_ptr<const ColorProfile> profile) noexcept {
    std::atomic_store_explicit(&active_profile, std::move(profile),
                              std::memory_order_release);
}

inline std::shared_ptr<const ColorProfile> snapshot() noexcept {
    return std::atomic_load_explicit(&active_profile, std::memory_order_acquire);
}

inline const std::uint8_t* apply(const std::uint8_t* source, int width, int height,
                               std::vector<std::uint8_t>& scratch) {
    const auto profile = snapshot();
    if (!source || !profile || profile->identity() || width <= 0 || height <= 0 ||
        width > 4096 || height > 4096)
        return source;
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    scratch.resize(pixels * 3);
    profile->map(source, scratch.data(), pixels);
    return scratch.data();
}

}  // namespace sma3::mods
