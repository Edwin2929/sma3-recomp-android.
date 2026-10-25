// Real PPU lifecycle regression for the Android panorama integration.
// No ROM, BIOS, generated game code or device is required.
#include "gba_ppu.h"
#include "snapshot.h"
#include "view_config.h"
#include "../src/image_options.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {
using gba::GbaPpu;
using gbarecomp::debug::SnapshotReader;
using gbarecomp::debug::SnapshotWriter;
using Bytes = std::vector<uint8_t>;
uint64_t assertions = 0;
unsigned scenarios = 0;

void require(bool value, const std::string& label) {
    ++assertions;
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", label.c_str());
        std::exit(1);
    }
}
void put16(uint8_t* p, unsigned value) {
    p[0] = static_cast<uint8_t>(value);
    p[1] = static_cast<uint8_t>(value >> 8);
}
int tiles(int, int, int, uint16_t*) { return gba::kWsTilemapKeepWrapped; }
int rejected_window(int, uint16_t, uint16_t, uint16_t, int*) { return 0; }
unsigned window_calls = 0;
int verified_window(int, uint16_t, uint16_t a1, uint16_t, int* out_x) {
    ++window_calls;
    int x = a1 & 511;
    if (x >= 256) x -= 512;
    *out_x = x;
    return 1;
}

void reset_policy() {
    gba::g_ws_tilemap_provider = tiles;
    gba::g_ws_bg_x_provider = nullptr;
    gba::g_ws_bg_xy_provider = nullptr;
    gba::g_ws_authored_margin_layers = 0;
    gba::g_ws_pillarbox = 0;
    gba::g_ws_pillarbox_left = 0;
    gba::g_ws_pillarbox_right = 0;
    gba::g_ws_obj_x_provider = nullptr;
    gba::g_ws_obj_attr_x_provider = nullptr;
    gba::g_ws_obj_window_x_provider = nullptr;
    gba::g_ws_obj_native_clip = 0;
    gba::g_ws_obj_native_clip_provider = nullptr;
    gba::g_ws_obj_margin_provider = nullptr;
    window_calls = 0;
}

struct Fixture {
    std::array<uint8_t, 1024> io{};
    std::array<uint8_t, 98304> vram{};
    std::array<uint8_t, 1024> oam{};
    std::array<uint8_t, 1024> pal{};
    uint16_t dispcnt = 0x0100; // Mode 0, one 8bpp text background.

    Fixture() {
        for (unsigned i = 0; i < 128; ++i) put16(oam.data() + i * 8, 0x0200);
        put16(io.data() + 0x08, 0x1f80); // BG0: screenblock31, 8bpp.
        for (unsigned i = 0; i < 1024; ++i)
            put16(vram.data() + 0xf800 + i * 2, 1 + ((i * 7 + i / 32) % 15));
        for (unsigned tile = 1; tile <= 15; ++tile)
            for (unsigned y = 0; y < 8; ++y)
                for (unsigned x = 0; x < 8; ++x)
                    vram[tile * 64 + y * 8 + x] = uint8_t(1 + (tile + x * 3 + y * 5) % 15);
        palette(0);
    }
    void palette(unsigned variant) {
        put16(pal.data(), 0x7fff);
        for (unsigned i = 1; i <= 15; ++i) {
            const unsigned r = 1 + (i * 5 + variant * 11) % 30;
            const unsigned g = 1 + (i * 7 + variant * 13) % 30;
            const unsigned b = 1 + (i * 11 + variant * 17) % 30;
            put16(pal.data() + i * 2, r | (g << 5) | (b << 10));
        }
    }
    void window() {
        dispcnt |= 0x9000;
        put16(oam.data(), 0x0820); // Window, Y32, 16x16.
        put16(oam.data() + 2, 0x41f8); // X-8.
        put16(oam.data() + 4, 128);
        std::fill_n(vram.data() + 0x11000, 32, uint8_t(0x11));
        std::fill_n(vram.data() + 0x11020, 32, uint8_t(0x11));
        std::fill_n(vram.data() + 0x11400, 32, uint8_t(0x11));
        std::fill_n(vram.data() + 0x11420, 32, uint8_t(0x11));
        put16(io.data() + 0x4a, 0x0001); // Inside backdrop, outside BG0.
    }
};

Bytes snapshot(const GbaPpu& ppu) {
    SnapshotWriter writer;
    ppu.serialize(writer);
    return writer.take_buffer();
}
void restore(GbaPpu& ppu, const Bytes& bytes) {
    SnapshotReader reader(bytes.data(), bytes.size());
    ppu.deserialize(reader);
    require(reader.ok() && reader.remaining() == 0, "PPU snapshot framing");
}
Bytes render(GbaPpu& ppu, const Fixture& f) {
    Bytes rgb(ppu.render_bytes());
    ppu.render(rgb.data(), f.dispcnt, f.io.data(), f.vram.data(), f.oam.data(), f.pal.data());
    return rgb;
}
void latch(GbaPpu& ppu, const Fixture& f) {
    ppu.latch_framebuffer(f.dispcnt, f.io.data(), f.vram.data(), f.oam.data(), f.pal.data());
}
void scanline(GbaPpu& ppu, const Fixture& f, unsigned y) {
    ppu.render_scanline(y, f.dispcnt, f.io.data(), f.vram.data(), f.oam.data(), f.pal.data());
}
void center_equal(const uint8_t* wide, unsigned width, unsigned left,
                  const uint8_t* native, const std::string& label) {
    for (unsigned y = 0; y < 160; ++y)
        for (unsigned x = 0; x < 240 * 3; ++x)
            require(wide[(y * width + left) * 3 + x] == native[y * 240 * 3 + x], label);
}
void margins_black(const uint8_t* image, unsigned width, unsigned left,
                   unsigned first_y, unsigned end_y, const std::string& label) {
    for (unsigned y = first_y; y < end_y; ++y)
        for (unsigned x = 0; x < width; ++x) {
            if (x >= left && x < left + 240) continue;
            for (unsigned channel = 0; channel < 3; ++channel)
                require(image[(y * width + x) * 3 + channel] == 0, label);
        }
}
void timing_equal(const Bytes& a, const Bytes& b, const std::string& label) {
    // Fixed PPU snapshot prefix: three u32, one u16, one u64. Geometry and
    // presentation must never alter scanline, dot, sub-dot or frame counters.
    require(a.size() >= 22 && b.size() >= 22, "timing prefix size");
    require(std::equal(a.begin(), a.begin() + 22, b.begin()), label);
}

void image_mode_boundary_is_exclusive() {
    using sma3::ImageMode;
    static_assert(static_cast<int>(ImageMode::Original) == 0);
    static_assert(static_cast<int>(ImageMode::Panorama) == 1);
    static_assert(static_cast<int>(ImageMode::Stretch) == 2);
    require(sma3::image_mode.load() == ImageMode::Original,
            "new installs must default to original aspect mode");
    const std::array<std::pair<int, ImageMode>, 7> cases{{
        {0, ImageMode::Original}, {1, ImageMode::Panorama}, {2, ImageMode::Stretch},
        {-1, ImageMode::Original}, {3, ImageMode::Original},
        {std::numeric_limits<int>::min(), ImageMode::Original},
        {std::numeric_limits<int>::max(), ImageMode::Original}
    }};
    for (const auto& [value, expected] : cases) {
        const auto selected = sma3::validated_image_mode(value);
        require(selected == expected, "invalid or mismatched JNI image mode");
        sma3::image_mode.store(selected);
        require(sma3::image_mode.load() == expected, "mode publication changed selection");
        ++scenarios;
    }
    sma3::image_mode.store(ImageMode::Original);
}

void adaptive_geometry_is_bounded() {
    struct Case { int drawable_w, drawable_h; unsigned logical_w; };
    const std::array<Case, 14> cases{{
        {2400, 1080, 356}, {3120, 1440, 347}, {2340, 1080, 347},
        {1920, 1080, 284}, {1600, 1200, 240}, {1080, 2400, 240},
        {1440, 3120, 240}, {800, 800, 240}, {8000, 600, 356},
        {1, std::numeric_limits<int>::max(), 240},
        {std::numeric_limits<int>::max(), 1, 356},
        {0, 1080, 240}, {1920, 0, 240}, {-1, -1, 240}
    }};
    for (const auto& sample : cases) {
        const auto geometry = gbarecomp::resize_driven_view_geometry(
            sample.drawable_w, sample.drawable_h, 356, 160,
            GbaPpu::kMaxRenderWidth, GbaPpu::kMaxRenderHeight);
        require(geometry.width == sample.logical_w && geometry.height == 160,
                "adaptive surface geometry exceeds qualified horizontal view");
        require(geometry.extra_left + geometry.extra_right == geometry.width - 240 &&
                geometry.extra_right >= geometry.extra_left &&
                geometry.extra_right - geometry.extra_left <= 1 &&
                geometry.extra_top == 0 && geometry.extra_bottom == 0,
                "adaptive margins are not centered or introduce unauthored rows");
        const auto disabled = gbarecomp::resize_driven_view_geometry(
            sample.drawable_w, sample.drawable_h, 240, 160,
            GbaPpu::kMaxRenderWidth, GbaPpu::kMaxRenderHeight);
        require(disabled.width == 240 && disabled.height == 160,
                "native capability must reject stale wide geometry");
        ++scenarios;
    }
}

void resize_and_native_roundtrip() {
    reset_policy();
    Fixture f;
    const Fixture untouched = f;
    auto ppu = std::make_unique<GbaPpu>();
    auto native = std::make_unique<GbaPpu>();
    ppu->tick(3 * GbaPpu::kCyclesPerFrame + 43 * GbaPpu::kCyclesPerScanline + 197);
    native->tick(3 * GbaPpu::kCyclesPerFrame + 43 * GbaPpu::kCyclesPerScanline + 197);
    const auto timing = snapshot(*ppu);
    const auto original = render(*native, f);
    require(!ppu->view_expanded() && ppu->render_width() == 240 &&
            ppu->render_height() == 160, "default geometry must be native");
    const std::array<unsigned, 12> widths{240, 241, 320, 346, 355, 356, 300, 240, 356, 241, 240, 356};
    for (unsigned width : widths) {
        latch(*ppu, f);
        const unsigned left = (width - 240) / 2;
        ppu->set_view_margins(left, width - 240 - left, 0, 0);
        require(ppu->render_width() == width, "requested width and odd margin remainder");
        require(ppu->has_latched_framebuffer(),
                "geometry change must retain the complete remapped latch");
        timing_equal(snapshot(*ppu), timing, "resize changed guest PPU timing");
        for (unsigned y = 0; y < 160; ++y) scanline(*ppu, f, y);
        ppu->mark_framebuffer_latched();
        require(ppu->has_latched_framebuffer(), "fresh completed frame not latched");
        center_equal(ppu->latched_framebuffer(), width, left, original.data(), "resize center mismatch");
        const auto direct = render(*ppu, f);
        require(std::equal(direct.begin(), direct.end(), ppu->latched_framebuffer()),
                "scanline/full rendering disagree after resize");
        timing_equal(snapshot(*ppu), timing, "presentation changed guest timing");
        ++scenarios;
    }
    require(f.io == untouched.io && f.vram == untouched.vram && f.oam == untouched.oam &&
            f.pal == untouched.pal, "rendering mutated synthetic guest memory");
}

void snapshot_center_and_geometry() {
    reset_policy();
    Fixture f;
    auto native = std::make_unique<GbaPpu>();
    auto wide = std::make_unique<GbaPpu>();
    native->tick(5 * GbaPpu::kCyclesPerFrame + 81 * GbaPpu::kCyclesPerScanline + 901);
    wide->tick(5 * GbaPpu::kCyclesPerFrame + 81 * GbaPpu::kCyclesPerScanline + 901);
    wide->set_view_margins(58, 58, 0, 0);
    latch(*native, f);
    latch(*wide, f);
    const auto native_state = snapshot(*native);
    const auto wide_state = snapshot(*wide);
    require(native_state.size() == GbaPpu::kFramebufferBytes + 23, "fixed snapshot size");
    require(native_state == wide_state, "wide snapshot must preserve native payload exactly");
    auto loaded_native = std::make_unique<GbaPpu>();
    restore(*loaded_native, wide_state);
    require(!loaded_native->view_expanded() && loaded_native->has_latched_framebuffer(),
            "wide saved state changed native host geometry/latch");
    require(snapshot(*loaded_native) == native_state, "wide-to-native state roundtrip");
    ++scenarios;

    auto loaded_wide = std::make_unique<GbaPpu>();
    loaded_wide->set_view_margins(47, 48, 0, 0);
    restore(*loaded_wide, native_state);
    require(loaded_wide->render_width() == 335 && loaded_wide->has_latched_framebuffer(),
            "native load must retain host geometry and the saved native latch");
    require(gba::g_ws_pillarbox == 1, "state restore must initially fail margins closed");
    margins_black(loaded_wide->latched_framebuffer(), 335, 47, 0, 160,
                  "restored latch retained unauthored margins");
    center_equal(loaded_wide->latched_framebuffer(), 335, 47, native->latched_framebuffer(),
                 "native-to-wide restored completed latch");
    const auto fallback = render(*loaded_wide, f);
    margins_black(fallback.data(), 335, 47, 0, 160, "immediate restore margins are not black");
    center_equal(fallback.data(), 335, 47, native->latched_framebuffer(),
                 "native-to-wide restored center");
    timing_equal(snapshot(*loaded_wide), native_state, "restore failed PPU timing");
    ++scenarios;
}

void midframe_restore_does_not_reuse_old_margins() {
    reset_policy();
    Fixture old_scene;
    Fixture new_scene;
    new_scene.palette(1);
    auto source = std::make_unique<GbaPpu>();
    auto target = std::make_unique<GbaPpu>();
    source->tick(80 * GbaPpu::kCyclesPerScanline);
    latch(*source, old_scene);
    const auto saved = snapshot(*source);
    const auto source_image = render(*source, old_scene);
    const auto new_image = render(*source, new_scene);
    target->set_view_margins(58, 58, 0, 0);
    latch(*target, new_scene); // Deliberately nonblack old wide margin storage.
    restore(*target, saved);
    require(target->has_latched_framebuffer(), "restore discarded saved complete native latch");
    center_equal(target->latched_framebuffer(), 356, 58, source_image.data(),
                 "restore shows the live scene instead of the saved completed scene");
    margins_black(target->latched_framebuffer(), 356, 58, 0, 160,
                  "restored complete latch retained old margins");
    for (unsigned y = 80; y < 160; ++y) scanline(*target, new_scene, y);
    target->mark_framebuffer_latched();
    const auto* rgb = target->latched_framebuffer();
    margins_black(rgb, 356, 58, 0, 160, "mid-frame restore reused stale margins");
    for (unsigned y = 0; y < 160; ++y) {
        const auto& expected = y < 80 ? source_image : new_image;
        for (unsigned x = 0; x < 240 * 3; ++x)
            require(rgb[(y * 356 + 58) * 3 + x] == expected[y * 240 * 3 + x],
                    "partial restored frame lost seeded/native center");
    }
    ++scenarios;
}

void unsupported_window_modes_keep_native_center() {
    for (unsigned windows : {0x2000u, 0x4000u, 0x6000u}) {
        reset_policy();
        Fixture f;
        f.window();
        f.dispcnt |= uint16_t(windows);
        put16(f.io.data() + 0x40, 0x1850);
        put16(f.io.data() + 0x42, 0x4058);
        put16(f.io.data() + 0x44, 0x00a0);
        put16(f.io.data() + 0x46, 0x00a0);
        put16(f.io.data() + 0x48, 0x0000);
        gba::g_ws_obj_window_x_provider = verified_window;
        auto native = std::make_unique<GbaPpu>();
        auto wide = std::make_unique<GbaPpu>();
        wide->set_view_margins(58, 58, 0, 0);
        const auto native_image = render(*native, f);
        const auto wide_image = render(*wide, f);
        require(window_calls == 0, "rectangular windows invoked lateral OBJ window provider");
        center_equal(wide_image.data(), 356, 58, native_image.data(),
                     "unsupported window mode altered center");
        margins_black(wide_image.data(), 356, 58, 32, 48,
                      "unsupported nonuniform window row escaped conservative fallback");
        ++scenarios;
    }
    reset_policy();
    Fixture f;
    f.window();
    gba::g_ws_obj_window_x_provider = rejected_window;
    auto ppu = std::make_unique<GbaPpu>();
    ppu->set_view_margins(58, 58, 0, 0);
    const auto image = render(*ppu, f);
    margins_black(image.data(), 356, 58, 32, 48, "unknown window provenance escaped fallback");
    ++scenarios;
}
} // namespace

int main() {
    image_mode_boundary_is_exclusive();
    adaptive_geometry_is_bounded();
    resize_and_native_roundtrip();
    snapshot_center_and_geometry();
    midframe_restore_does_not_reuse_old_margins();
    unsupported_window_modes_keep_native_center();
    reset_policy();
    gba::g_ws_tilemap_provider = nullptr;
    std::printf("PASS: %u lifecycle scenarios, %llu internal assertions\n", scenarios,
                static_cast<unsigned long long>(assertions));
}
