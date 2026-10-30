#pragma once

#include <array>
#include <cstdint>

namespace gbarecomp { struct ExtendedViewFrameInfo; class HostOverlay; }

namespace sma3 {

// Actual normal-Yoshi OBJ texels; no copyrighted sprite sheet is bundled.
// Packed row-major texels, even index in the low nibble; zero is transparent.
// Width/height describe the cropped image. Remaining bytes and palette[0]
// are zero so identical poses have a canonical, bounded representation.
struct GhostImage {
    std::uint8_t width = 0, height = 0;
    std::array<std::uint16_t, 16> palette{};
    std::array<std::uint8_t, 2048> pixels{};
};

struct GhostVisualSample {
    std::uint16_t room = 0;
    std::int32_t world_x = 0, world_y = 0;
    bool visible = false;
    GhostImage image{};
};

// Guest-thread only. Publish frame-start context before the recorder samples.
// Presentation uses the previous frame context, matching the PPU's latched
// completed image. Duplicate resize publication never advances that history.
void ghost_visual_frame(const gbarecomp::ExtendedViewFrameInfo* frame) noexcept;
GhostVisualSample ghost_capture_visual() noexcept;

// Pure host chrome: does not touch the guest framebuffer, RAM or game input.
void ghost_draw_overlay(gbarecomp::HostOverlay* overlay) noexcept;

}  // namespace sma3
