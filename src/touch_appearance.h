#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>

namespace sma3 {

enum class TouchStyle : unsigned { Classic = 0, Symbols = 1 };
struct TouchAppearance {
    TouchStyle style = TouchStyle::Symbols;
    unsigned opacity_percent = 75;
};

// A single publication keeps style and opacity coherent for an entire pad.
inline std::atomic<unsigned> touch_appearance_bits{(1u << 8) | 75u};

inline void set_touch_appearance(int style, int opacity_percent) noexcept {
    const unsigned safe_style = style == 0 ? 0u : 1u;
    const unsigned safe_opacity = static_cast<unsigned>(
        std::clamp(opacity_percent, 20, 100));
    touch_appearance_bits.store((safe_style << 8) | safe_opacity,
                               std::memory_order_release);
}

inline TouchAppearance get_touch_appearance() noexcept {
    const unsigned packed = touch_appearance_bits.load(std::memory_order_acquire);
    return {static_cast<TouchStyle>((packed >> 8) & 1u), packed & 255u};
}

inline std::uint8_t touch_alpha(TouchAppearance appearance,
                                unsigned source_alpha = 255) noexcept {
    return static_cast<std::uint8_t>((std::min(source_alpha, 255u) *
        std::clamp(appearance.opacity_percent, 20u, 100u) + 50u) / 100u);
}

}  // namespace sma3
