#include "yoshi_color.h"
#include "yoshi_color_palette.h"
#include "presentation_pacing.h"

#include "gba_bus.h"
#include "gba_ppu.h"
#include "runtime_bus_bridge.h"

#include <atomic>

namespace sma3 {
namespace {
constexpr std::uint32_t kEnabled = 0x01000000u;
std::atomic<std::uint32_t> requested{0};
yoshi_color_detail::Palette palette;
const std::uint8_t* reference_rom = nullptr;
std::size_t reference_size = 0;
bool reference_ready = false;

const std::uint8_t* object_palette(int, std::uint16_t attr0, std::uint16_t,
                                   std::uint16_t attr2, const std::uint8_t* original) {
    return palette.for_object(attr0, attr2, original);
}
}  // namespace

void set_yoshi_color(bool enabled, std::uint32_t rgb) noexcept {
    if (rgb > 0xffffffu) return;
    const auto next = rgb | (enabled ? kEnabled : 0u);
    const auto previous = requested.exchange(next, std::memory_order_acq_rel);
    if (previous != next && ((previous | next) & kEnabled))
        presentation_epoch.fetch_add(1, std::memory_order_relaxed);
}

void yoshi_color_frame() noexcept {
    // The UI publishes one packed value; rendering sees one complete choice
    // for the entire guest frame, including its native and panoramic images.
    gba::g_obj_palette_provider = nullptr;
    const auto config = requested.load(std::memory_order_acquire);
    if (!(config & kEnabled)) return;
    const auto* bus = gbarecomp::active_bus();
    if (!bus || !yoshi_color_detail::supported_scene(bus->iwram_ptr(), 0x8000,
            yoshi_color_detail::read16(bus->io().raw()))) return;
    const auto* rom = bus->rom_ptr();
    const auto size = bus->rom_size();
    if (reference_rom != rom || reference_size != size) {
        reference_rom = rom;
        reference_size = size;
        reference_ready = palette.initialize(rom, size);
    }
    if (!reference_ready) return;
    palette.color(config & 0xffffffu);
    gba::g_obj_palette_provider = object_palette;
}

}  // namespace sma3
