// Real PPU regression for live viewport changes, without game assets.
// Scene A is complete; scene B has different scanline scroll and colors.
// A resize must preserve their separate displayed/work surfaces and timing.
#include "gba_ppu.h"
#include "snapshot.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {
using gba::GbaPpu;
using Bytes = std::vector<uint8_t>;
unsigned scenarios = 0;
uint64_t assertions = 0;

void require(bool value, const std::string& label) {
    ++assertions;
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", label.c_str());
        std::exit(1);
    }
}
void put16(uint8_t* p, unsigned value) {
    p[0] = uint8_t(value);
    p[1] = uint8_t(value >> 8);
}
int tiles(int, int, int, uint16_t*) { return gba::kWsTilemapKeepWrapped; }
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
}

struct Geometry {
    unsigned left, right;
    unsigned width() const { return 240 + left + right; }
    int first() const { return -int(left); }
    int end() const { return 240 + int(right); }
};
struct Fixture {
    std::array<uint8_t, 1024> io{}, oam{}, pal{};
    std::array<uint8_t, 98304> vram{};
    unsigned scene;
    explicit Fixture(unsigned variant) : scene(variant) {
        put16(io.data() + 8, 0x1f80); // BG0, 8bpp, screenblock31.
        for (unsigned i = 0; i < 128; ++i) put16(oam.data() + i * 8, 0x0200);
        for (unsigned i = 0; i < 1024; ++i)
            put16(vram.data() + 0xf800 + i * 2, 1 + (i * 7 + i / 32) % 31);
        for (unsigned tile = 1; tile <= 31; ++tile)
            for (unsigned y = 0; y < 8; ++y)
                for (unsigned x = 0; x < 8; ++x)
                    vram[tile * 64 + y * 8 + x] = uint8_t(1 + (tile + x * 3 + y * 5) % 31);
        for (unsigned i = 1; i <= 31; ++i) {
            unsigned r = 1 + (i * 3 + scene * 11) % 30;
            unsigned g = 1 + (i * 7 + scene * 13) % 30;
            unsigned b = 1 + (i * 11 + scene * 17) % 30;
            put16(pal.data() + i * 2, r | (g << 5) | (b << 10));
        }
    }
    void row(GbaPpu& ppu, unsigned y, uint16_t dispcnt = 0x0100) const {
        // Deliberately varies within one frame. A fresh render from the last
        // row's registers is not an oracle for the already composed image.
        auto regs = io;
        put16(regs.data() + 0x10, (y * (3 + scene * 2) + scene * 31) % 113);
        put16(regs.data() + 0x12, (y / 7 + scene * 19) % 53);
        ppu.render_scanline(y, dispcnt, regs.data(), vram.data(), oam.data(), pal.data());
    }
};

Bytes snapshot(const GbaPpu& ppu) {
    gbarecomp::debug::SnapshotWriter w;
    ppu.serialize(w);
    return w.take_buffer();
}
void timing_equal(const Bytes& before, const GbaPpu& after, const std::string& label) {
    const auto state = snapshot(after);
    require(before.size() >= 22 && state.size() >= 22, "timing prefix exists");
    require(std::equal(before.begin(), before.begin() + 22, state.begin()), label);
}
void set_geometry(GbaPpu& ppu, Geometry geometry) {
    ppu.set_view_margins(geometry.left, geometry.right, 0, 0);
    require(ppu.render_width() == geometry.width() && ppu.render_height() == 160,
            "viewport dimensions");
}
Bytes complete_image(const Fixture& f, Geometry geometry) {
    auto ppu = std::make_unique<GbaPpu>();
    set_geometry(*ppu, geometry);
    for (unsigned y = 0; y < 160; ++y) f.row(*ppu, y);
    ppu->mark_framebuffer_latched();
    return Bytes(ppu->latched_framebuffer(), ppu->latched_framebuffer() + ppu->render_bytes());
}
void center_equal(const uint8_t* image, Geometry geometry,
                  const Bytes& native, const std::string& label) {
    for (unsigned y = 0; y < 160; ++y) {
        const auto* row = image + (y * geometry.width() + geometry.left) * 3;
        require(std::equal(row, row + 240 * 3, native.data() + y * 240 * 3), label);
    }
}

void resize_case(const std::vector<Geometry>& path, unsigned rendered_rows,
                 bool initial_latch = true) {
    reset_policy();
    Fixture scene_a(0), scene_b(1);
    const auto native_a = complete_image(scene_a, {0, 0});
    const auto native_b = complete_image(scene_b, {0, 0});
    require(native_a != native_b, "distinct scene oracle");
    const Geometry initial = path.front();
    const auto original_a = complete_image(scene_a, initial);
    auto ppu = std::make_unique<GbaPpu>();
    set_geometry(*ppu, initial);
    if (initial_latch) {
        for (unsigned y = 0; y < 160; ++y) scene_a.row(*ppu, y);
        ppu->mark_framebuffer_latched();
    }
    ppu->tick(2 * GbaPpu::kCyclesPerFrame + rendered_rows * GbaPpu::kCyclesPerScanline + 193);
    for (unsigned y = 0; y < rendered_rows; ++y) scene_b.row(*ppu, y);
    const auto timing = snapshot(*ppu);
    bool resized = false;
    Geometry current = initial;
    for (unsigned step = 1; step < path.size(); ++step) {
        const Geometry previous = current;
        current = path[step];
        resized = resized || previous.left != current.left || previous.right != current.right;
        const std::string context = "rows=" + std::to_string(rendered_rows) +
            " resize=" + std::to_string(step) + " target=" +
            std::to_string(current.left) + "/" + std::to_string(current.right);
        set_geometry(*ppu, current);
        require(ppu->has_latched_framebuffer() == initial_latch,
                context + ": resize discarded complete latch or invented one");
        timing_equal(timing, *ppu, context + ": resize changed timing");
        if (!initial_latch) continue;
        center_equal(ppu->latched_framebuffer(), current, native_a,
                     context + ": displayed center lost complete scene A");
        // Changed geometry retains only the authentic center. Previously
        // authored margins are not authorized for a new viewport; same-size
        // calls must preserve the entire complete frame without clearing it.
        Bytes expected = resized ? Bytes(ppu->render_bytes(), 0) : original_a;
        if (resized)
            for (unsigned y = 0; y < 160; ++y)
                std::copy_n(native_a.data() + y * 240 * 3, 240 * 3,
                            expected.data() + (y * current.width() + current.left) * 3);
        require(std::equal(expected.begin(), expected.end(), ppu->latched_framebuffer()),
                context + ": displayed native center or conservative margins differ");
    }
    for (unsigned y = rendered_rows; y < 160; ++y) scene_b.row(*ppu, y);
    ppu->mark_framebuffer_latched();
    require(ppu->has_latched_framebuffer(), "completed scene B not latched");
    center_equal(ppu->latched_framebuffer(), current, native_b,
                 "resize destroyed already-rendered work rows of scene B");
    auto expected_b = complete_image(scene_b, current);
    for (unsigned y = 0; y < rendered_rows; ++y)
        for (int x = current.first(); x < current.end(); ++x)
            if (resized && (x < 0 || x >= 240))
                for (unsigned c = 0; c < 3; ++c)
                    expected_b[(y * current.width() + x + int(current.left)) * 3 + c] = 0;
    require(std::equal(expected_b.begin(), expected_b.end(), ppu->latched_framebuffer()),
            "completed scene B has stale, shifted or invented margin pixels");
    timing_equal(timing, *ppu, "finishing resized work changed guest timing");
    ++scenarios;
}

void resize_matrix() {
    const std::vector<std::vector<Geometry>> paths{
        {{0, 0}, {53, 54}}, {{0, 0}, {58, 58}},
        {{53, 54}, {58, 58}}, {{58, 58}, {53, 54}},
        {{53, 54}, {0, 0}}, {{58, 58}, {0, 0}},
        {{0, 0}, {53, 54}, {58, 58}, {0, 0}, {58, 58}},
        {{58, 58}, {53, 54}, {58, 58}},
        {{53, 54}, {54, 53}},
        {{58, 58}, {0, 116}, {116, 0}, {58, 58}},
        {{58, 58}, {58, 58}}, {{0, 0}, {0, 0}},
        {{1, 0}, {0, 1}}
    };
    for (const auto& path : paths)
        for (unsigned rows : {0u, 1u, 40u, 80u, 159u, 160u}) resize_case(path, rows);
    resize_case({{0, 0}, {53, 54}, {58, 58}}, 80, false);
}

void forced_blank_policy() {
    for (const auto& policy : std::array<std::array<int, 3>, 4>{{
             {{1, 0, 0}}, {{0, 1, 0}}, {{0, 0, 1}}, {{0, 0, 0}}
         }}) {
        reset_policy();
        Fixture fixture(0);
        auto ppu = std::make_unique<GbaPpu>();
        const Geometry geometry{53, 54};
        set_geometry(*ppu, geometry);
        gba::g_ws_pillarbox = policy[0];
        gba::g_ws_pillarbox_left = policy[1];
        gba::g_ws_pillarbox_right = policy[2];
        for (unsigned y = 0; y < 160; ++y) fixture.row(*ppu, y, 0x0080);
        ppu->mark_framebuffer_latched();
        const auto* pixels = ppu->latched_framebuffer();
        for (unsigned y = 0; y < 160; ++y)
            for (unsigned x = 0; x < geometry.width(); ++x) {
                const bool hidden = (x < geometry.left && (policy[0] || policy[1])) ||
                    (x >= geometry.left + 240 && (policy[0] || policy[2]));
                const uint8_t expected = hidden ? 0 : 255;
                for (unsigned c = 0; c < 3; ++c)
                    require(pixels[(y * geometry.width() + x) * 3 + c] == expected,
                            "forced blank escaped pillarbox or changed native white center");
            }
        ++scenarios;
    }
}

void restored_completed_frame_is_preserved() {
    for (const auto& pair : std::vector<std::array<Geometry, 2>>{
             {{{0, 0}, {53, 54}}}, {{{53, 54}, {58, 58}}}, {{{58, 58}, {0, 0}}}
         }) {
        reset_policy();
        Fixture scene_a(0), live_scene_b(1);
        const auto native_a = complete_image(scene_a, {0, 0});
        const auto native_b = complete_image(live_scene_b, {0, 0});
        require(native_a != native_b, "restore scene oracle differs from live memory");
        auto source = std::make_unique<GbaPpu>();
        set_geometry(*source, pair[0]);
        source->tick(7 * GbaPpu::kCyclesPerFrame + 80 * GbaPpu::kCyclesPerScanline + 73);
        for (unsigned y = 0; y < 160; ++y) scene_a.row(*source, y);
        source->mark_framebuffer_latched();
        const auto saved = snapshot(*source);
        auto target = std::make_unique<GbaPpu>();
        set_geometry(*target, pair[1]);
        for (unsigned y = 0; y < 160; ++y) live_scene_b.row(*target, y);
        target->mark_framebuffer_latched();
        gbarecomp::debug::SnapshotReader reader(saved.data(), saved.size());
        target->deserialize(reader);
        require(reader.ok() && reader.remaining() == 0, "restore framing");
        require(target->has_latched_framebuffer(), "restore discarded the saved complete raster frame");
        center_equal(target->latched_framebuffer(), pair[1], native_a,
                     "restore presents live scene B instead of saved complete scene A");
        Bytes expected(target->render_bytes(), 0);
        for (unsigned y = 0; y < 160; ++y)
            std::copy_n(native_a.data() + y * 240 * 3, 240 * 3,
                        expected.data() + (y * pair[1].width() + pair[1].left) * 3);
        require(std::equal(expected.begin(), expected.end(), target->latched_framebuffer()),
                "restore retained unauthored margins from prior live scene");
        timing_equal(saved, *target, "restore changed saved PPU timing");
        ++scenarios;
    }
    reset_policy();
    auto source = std::make_unique<GbaPpu>();
    const auto saved = snapshot(*source);
    auto target = std::make_unique<GbaPpu>();
    set_geometry(*target, {53, 54});
    gbarecomp::debug::SnapshotReader reader(saved.data(), saved.size());
    target->deserialize(reader);
    require(reader.ok() && !target->has_latched_framebuffer(),
            "restore fabricated a complete frame absent from the saved state");
    ++scenarios;
}
} // namespace

int main(int argc, char** argv) {
    const std::string group = argc > 1 ? argv[1] : "all";
    require(group == "all" || group == "--resize" || group == "--forced-blank" || group == "--restore",
            "unknown test group");
    if (group == "all" || group == "--resize") resize_matrix();
    if (group == "all" || group == "--forced-blank") forced_blank_policy();
    if (group == "all" || group == "--restore") restored_completed_frame_is_preserved();
    reset_policy();
    gba::g_ws_tilemap_provider = nullptr;
    std::printf("PASS: %u resize/presentation scenarios, %llu internal assertions\n",
                scenarios, static_cast<unsigned long long>(assertions));
}
