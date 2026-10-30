#include "ghost_visual.h"
#include "ghost_visual_detail.h"
#include "time_trial_ghost.h"
#include "controller_options.h"
#include "touch_layout.h"

#include "gba_bus.h"
#include "gba_ppu.h"
#include "host_overlay.h"
#include "runtime.h"
#include "runtime_bus_bridge.h"

namespace sma3 {
namespace {
using namespace ghost_visual_detail;
ContextLatch contexts;
Memory memory{};

GhostImage cached_image{};
std::array<Run, kMaxRuns> cached_runs{};
std::size_t cached_count = 0;
bool cached_valid = false;

bool same_image(const GhostImage& a, const GhostImage& b) noexcept {
    return a.width == b.width && a.height == b.height && a.palette == b.palette &&
        a.pixels == b.pixels;
}

gbarecomp::OverlayColor color(std::uint16_t value) noexcept {
    const unsigned r = value & 31, g = (value >> 5) & 31, b = (value >> 10) & 31;
    return {std::uint8_t((r << 3) | (r >> 2)), std::uint8_t((g << 3) | (g >> 2)),
            std::uint8_t((b << 3) | (b >> 2)), 112};
}
}  // namespace

void ghost_visual_frame(const gbarecomp::ExtendedViewFrameInfo* frame) noexcept {
    const auto* bus = gbarecomp::active_bus();
    if (!frame || !bus) {
        memory = {}; contexts.clear(); cached_valid = false;
        return;
    }
    // Line zero publishes upcoming OBJ/camera state; the runtime immediately
    // presents the image that was latched at the preceding VBlank. Advance
    // only once per real guest frame, not on resize or a second 120-Hz present.
    memory = {bus->iwram_ptr(), bus->oam_ptr(), bus->vram_ptr(),
              bus->pal_ptr(), frame->io, 0x8000, 0x400, 0x18000, 0x400, frame->io_size};
    contexts.publish(scene(memory), gba::g_ws_pillarbox != 0, bus,
                     frame->frame_count, frame->state_epoch);
}

GhostVisualSample ghost_capture_visual() noexcept {
    auto sample = capture(memory);
    if (!sample.visible || !gba::g_obj_palette_provider) return sample;
    // The color mod publishes one read-only OBJ palette for the whole frame.
    // Ask using a real eligible object, then retain only used bank-5 entries.
    for (unsigned i = 0; i < 128; ++i) {
        const auto* p = memory.oam + i * 8;
        const auto a0 = read16(p), a1 = read16(p + 2), a2 = read16(p + 4);
        if (!yoshi_color_detail::player_object(a0, a2) ||
            (!(a0 & 0x100) && (a0 & 0x200)) || (a0 & 255) == 160) continue;
        const auto* replacement = gba::g_obj_palette_provider(int(i), a0, a1, a2, memory.pal + 0x200);
        if (replacement) {
            std::array<bool,16> used{};
            for (unsigned y = 0; y < sample.image.height; ++y)
                for (unsigned x = 0; x < sample.image.width; ++x) used[pixel(sample.image, x, y)] = true;
            for (unsigned entry = 1; entry < 16; ++entry)
                if (used[entry]) sample.image.palette[entry] = read16(replacement + 0xa0 + entry * 2) & 0x7fff;
        }
        break;
    }
    return sample;
}

void ghost_draw_overlay(gbarecomp::HostOverlay* overlay) noexcept {
    const auto& displayed = contexts.displayed();
    if (!overlay || !displayed.valid || !displayed.scene.visible ||
        menu_open.load(std::memory_order_relaxed) || touch_editing.load(std::memory_order_relaxed)) return;
    GhostVisualSample sample;
    if (!time_trial_ghost_playback(sample) || !sample.visible || sample.room != displayed.scene.room) return;
    const auto rect = overlay->game_rect();
    const auto p = projection(overlay->drawable_width(), overlay->drawable_height(),
        rect.x, rect.y, rect.width, rect.height, overlay->view_width(), overlay->view_height(),
        displayed.pillarbox, image_filter.load(std::memory_order_relaxed) == ImageFilter::Cinematic);
    if (!p.valid) return;
    if (!cached_valid || !same_image(cached_image, sample.image)) {
        cached_image = sample.image;
        cached_valid = true;
        if (!runs(cached_image, cached_runs, cached_count)) return;
    }
    for (std::size_t i = 0; i < cached_count; ++i) {
        const auto& run = cached_runs[i];
        Rectangle output;
        if (!project(p, run, sample.world_x, sample.world_y,
                     displayed.scene.camera_x, displayed.scene.camera_y, output)) continue;
        overlay->fill_rect(output.x, output.y, output.width, output.height, 0.0f,
            color(sample.image.palette[run.color]), gbarecomp::OverlaySpace::Drawable);
    }
}

}  // namespace sma3
