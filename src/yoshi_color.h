#pragma once

#include <cstdint>

namespace sma3 {

// UI-safe request. RGB is an opaque 0xRRGGBB value; out-of-range values are
// rejected. No guest RAM, palette RAM, ROM, or save data is modified.
void set_yoshi_color(bool enabled, std::uint32_t rgb) noexcept;

// Game-thread frame-boundary publication, including after a state restore.
// Install this alongside every extended_view_frame callback, even when the
// panoramic mode is disabled. Disabled or unsupported scenes use no hook.
void yoshi_color_frame() noexcept;

}  // namespace sma3
