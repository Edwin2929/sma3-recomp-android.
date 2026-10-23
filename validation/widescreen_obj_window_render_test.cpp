// Standalone real-PPU tests; no game ROM, BIOS, save state, or generated code.
// The texture/affine oracle is the unchanged native 240px compositor with a
// translated synthetic OBJ. Expected mask pixels are not reimplemented here.
#include "gba_ppu.h"
#include "foreign_presentation_internal.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {
using Rgb = std::array<uint8_t, 3>;
constexpr Rgb kRed{255, 0, 0}, kGreen{0, 255, 0}, kWhite{255, 255, 255};
constexpr Rgb kBlack{0, 0, 0};
constexpr int kNativeWidth = 240, kHeight = 160;
uint64_t checks = 0;
unsigned cases = 0;

[[noreturn]] void fail(const std::string& label) {
    std::fprintf(stderr, "FAIL: %s\n", label.c_str());
    std::exit(1);
}
void require(bool condition, const std::string& label) {
    ++checks;
    if (!condition) fail(label);
}
void store16(uint8_t* target, unsigned value) {
    target[0] = static_cast<uint8_t>(value);
    target[1] = static_cast<uint8_t>(value >> 8);
}
uint16_t read16(const uint8_t* source) {
    return static_cast<uint16_t>(source[0] | (unsigned(source[1]) << 8));
}

struct Placement {
    bool known = false;
    uint16_t a0 = 0, a1 = 0, a2 = 0;
    int x = 0;
    int result = 1;
};
std::array<Placement, 128> placements{};
unsigned provider_calls = 0;
int window_x(int index, uint16_t a0, uint16_t a1, uint16_t a2, int* x) {
    ++provider_calls;
    if (index < 0 || index >= 128 || !x) return 0;
    const auto& p = placements[static_cast<unsigned>(index)];
    if (!p.known || a0 != p.a0 || a1 != p.a1 || a2 != p.a2) return 0;
    *x = p.x;
    return p.result;
}
int wrapped_tiles(int, int, int, uint16_t*) {
    return gba::kWsTilemapKeepWrapped;
}
void reset_policy() {
    placements = {};
    provider_calls = 0;
    gba::foreign_presentation_internal::clear();
    gba::g_ws_tilemap_provider = wrapped_tiles;
    gba::g_ws_bg_x_provider = nullptr;
    gba::g_ws_bg_xy_provider = nullptr;
    gba::g_ws_bg_x_provider_layers = 0xf;
    gba::g_ws_bg_xy_provider_layers = 0xf;
    gba::g_ws_affine_filter_enabled = 0;
    gba::g_ws_affine_filter_provider = nullptr;
    gba::g_ws_authored_margin_layers = 0;
    gba::g_ws_pillarbox = 0;
    gba::g_ws_pillarbox_left = 0;
    gba::g_ws_pillarbox_right = 0;
    gba::g_ws_obj_x_provider = nullptr;
    gba::g_ws_obj_attr_x_provider = nullptr;
    gba::g_ws_obj_native_clip = 0;
    gba::g_ws_obj_native_clip_provider = nullptr;
    gba::g_ws_obj_margin_provider = nullptr;
    gba::g_ws_obj_window_x_provider = window_x;
}

struct Fixture {
    std::array<uint8_t, 0x400> io{};
    std::array<uint8_t, 0x18000> vram{};
    std::array<uint8_t, 0x400> oam{};
    std::array<uint8_t, 0x400> pal{};
    uint16_t dispcnt = 0x9340; // Mode 0, BG0/BG1, OBJ, OBJ-window, 1D.
    Fixture() {
        for (unsigned i = 0; i < 128; ++i) store16(oam.data() + i * 8, 0x0200);
        // Constant red BG0 and green BG1 in separate screenblocks. The maps
        // deliberately remain valid when sampled outside native columns.
        std::fill_n(vram.begin() + 64, 64, uint8_t(1));
        std::fill_n(vram.begin() + 128, 64, uint8_t(2));
        for (unsigned i = 0; i < 1024; ++i) {
            store16(vram.data() + 0x4000 + i * 2, 1);
            store16(vram.data() + 0x4800 + i * 2, 2);
        }
        store16(io.data() + 0x08, 0x0880); // BG0 priority 0, screenblock 8, 8bpp.
        store16(io.data() + 0x0a, 0x0981); // BG1 priority 1, screenblock 9, 8bpp.
        store16(io.data() + 0x4a, 0x0201); // Outside: BG0; OBJ-window: BG1.
        store16(pal.data(), 0x7fff);
        store16(pal.data() + 2, 0x001f);
        store16(pal.data() + 4, 0x03e0);
        store16(pal.data() + 6, 0x7c00);
    }
    void object(unsigned index, int x, int y, unsigned shape, unsigned size,
                bool bpp8, unsigned flags = 0, unsigned tile = 128) {
        const auto a0 = uint16_t((y & 255) | 0x0800 | (bpp8 ? 0x2000 : 0) |
                                 (shape << 14) | (flags & 0x0300));
        const auto a1 = uint16_t((x & 511) | (size << 14) | (flags >> 16));
        const auto a2 = uint16_t(tile);
        store16(oam.data() + index * 8, a0);
        store16(oam.data() + index * 8 + 2, a1);
        store16(oam.data() + index * 8 + 4, a2);
        placements[index] = {true, a0, a1, a2, x, 1};
    }
    void matrix(unsigned group, int pa, int pb, int pc, int pd) {
        for (const auto& [offset, value] :
             std::array<std::pair<unsigned, int>, 4>{{{6, pa}, {14, pb},
                                                     {22, pc}, {30, pd}}})
            store16(oam.data() + group * 32 + offset, unsigned(value));
    }
    void texture(int width, int height, bool bpp8, bool mapping1d,
                 unsigned tile = 128, bool solid = false) {
        // Encode a deliberately asymmetric source bitmap, including fully
        // transparent texels and both nibbles of every 4bpp byte.
        const unsigned bytes_per_tile = bpp8 ? 64u : 32u;
        for (int ty = 0; ty < height / 8; ++ty) {
            for (int tx = 0; tx < width / 8; ++tx) {
                const unsigned base = 0x10000 + tile * 32 +
                    (mapping1d ? unsigned(ty * (width / 8) + tx) * bytes_per_tile
                               : unsigned(ty) * 1024 + unsigned(tx) * bytes_per_tile);
                for (int py = 0; py < 8; ++py) {
                    for (int px = 0; px < 8; ++px) {
                        const int x = tx * 8 + px, y = ty * 8 + py;
                        const unsigned value = solid || ((x * 5 + y * 3) % 11 < 4)
                            ? unsigned((x + y * 2) % 15 + 1) : 0;
                        if (bpp8) vram[base + unsigned(py * 8 + px)] = uint8_t(value);
                        else {
                            auto& byte = vram[base + unsigned(py * 4 + px / 2)];
                            const unsigned shift = (px & 1) ? 4u : 0u;
                            byte = uint8_t((byte & ~(15u << shift)) | (value << shift));
                        }
                    }
                }
            }
        }
    }
};

struct Image {
    unsigned width = 0, height = 0, left = 0, top = 0;
    std::vector<uint8_t> bytes;
    Rgb pixel(int hx, int y) const {
        const auto i = (std::size_t(y + int(top)) * width + unsigned(hx + int(left))) * 3;
        return {bytes.at(i), bytes.at(i + 1), bytes.at(i + 2)};
    }
};
Image render(const Fixture& f, unsigned left = 58, unsigned right = 58,
             unsigned top = 0, unsigned bottom = 0) {
    gba::GbaPpu ppu;
    ppu.set_view_margins(left, right, top, bottom);
    Image result{ppu.render_width(), ppu.render_height(), ppu.view_extra_left(),
                 ppu.view_extra_top(), std::vector<uint8_t>(ppu.render_bytes(), 0xa5)};
    const auto io = f.io, oam = f.oam, pal = f.pal;
    const auto vram = f.vram;
    ppu.render(result.bytes.data(), f.dispcnt, f.io.data(), f.vram.data(),
               f.oam.data(), f.pal.data());
    require(io == f.io && vram == f.vram && oam == f.oam && pal == f.pal,
            "renderer changed guest inputs");
    return result;
}
void expect(const Image& image, int x, int y, Rgb color, const std::string& label) {
    const auto actual = image.pixel(x, y);
    ++checks;
    if (actual != color) {
        char detail[192];
        std::snprintf(detail, sizeof(detail), "%s at (%d,%d): got %u,%u,%u; expected %u,%u,%u",
                      label.c_str(), x, y, actual[0], actual[1], actual[2],
                      color[0], color[1], color[2]);
        fail(detail);
    }
}
void center_is_native(const Fixture& f, const Image& wide, const std::string& label) {
    const auto before = provider_calls;
    const auto native = render(f, 0, 0);
    require(provider_calls == before, "native renderer called margin provider");
    for (int y = 0; y < kHeight; ++y) {
        const auto* got = wide.bytes.data() +
            ((std::size_t(y) + wide.top) * wide.width + wide.left) * 3;
        const auto* want = native.bytes.data() + std::size_t(y) * kNativeWidth * 3;
        require(std::memcmp(got, want, kNativeWidth * 3) == 0,
                label + ": native center row " + std::to_string(y));
    }
}
Image disabled_render(const Fixture& f, unsigned left = 58, unsigned right = 58) {
    const auto provider = gba::g_ws_obj_window_x_provider;
    gba::g_ws_obj_window_x_provider = nullptr;
    auto result = render(f, left, right);
    gba::g_ws_obj_window_x_provider = provider;
    return result;
}

void geometry_against_native_oracle() {
    constexpr int dimensions[3][4][2] = {
        {{8, 8}, {16, 16}, {32, 32}, {64, 64}},
        {{16, 8}, {32, 8}, {32, 16}, {64, 32}},
        {{8, 16}, {8, 32}, {16, 32}, {32, 64}}};
    // The affine matrix lives in group 17: the OBJ index is deliberately 5,
    // so a copied/window OAM prefix must not imply matrix-index reindexing.
    constexpr unsigned group = 17;
    struct Transform { unsigned flags; int pa, pb, pc, pd; };
    constexpr Transform transforms[] = {
        {0, 0x100, 0, 0, 0x100},
        {0x10000000u, 0x100, 0, 0, 0x100},
        {0x20000000u, 0x100, 0, 0, 0x100},
        {0x30000000u, 0x100, 0, 0, 0x100},
        {0x0100u | ((group << 9) << 16), 0x100, 0, 0, 0x100},
        {0x0100u | ((group << 9) << 16), 0, -0x100, 0x100, 0},
        {0x0100u | ((group << 9) << 16), 0x80, 0x40, -0x40, 0xc0},
        {0x0300u | ((group << 9) << 16), 0x100, 0, 0, 0x100},
        {0x0300u | ((group << 9) << 16), 0x80, -0x80, 0x80, 0x80},
    };
    for (unsigned shape = 0; shape < 3; ++shape) {
        for (unsigned size = 0; size < 4; ++size) {
            for (bool bpp8 : {false, true}) for (bool mapping1d : {false, true}) {
                for (const auto& transform : transforms) {
                    reset_policy();
                    Fixture f;
                    if (!mapping1d) f.dispcnt &= uint16_t(~0x0040u);
                    const int full_x = (cases & 1) ? 268 : -32;
                    f.object(5, full_x, 24, shape, size, bpp8, transform.flags);
                    f.matrix(group, transform.pa, transform.pb, transform.pc, transform.pd);
                    f.texture(dimensions[shape][size][0], dimensions[shape][size][1],
                              bpp8, mapping1d);
                    const auto wide = render(f);
                    center_is_native(f, wide, "texture/affine");
                    Fixture oracle = f;
                    const uint16_t attr1 = read16(oracle.oam.data() + 5 * 8 + 2);
                    store16(oracle.oam.data() + 5 * 8 + 2, (attr1 & ~511u) | 80u);
                    const auto native = render(oracle, 0, 0);
                    for (int y = 0; y < kHeight; ++y) {
                        for (int hx = -58; hx < 298; ++hx) {
                            if (hx >= 0 && hx < kNativeWidth) continue;
                            const int reference_x = 80 + hx - full_x;
                            const Rgb expected = reference_x >= 0 && reference_x < 240
                                ? native.pixel(reference_x, y) : kRed;
                            expect(wide, hx, y, expected, "translated native stencil");
                        }
                    }
                    ++cases;
                }
            }
        }
    }
}

void explicit_pixels_and_union() {
    reset_policy();
    Fixture f;
    f.object(5, -4, 20, 0, 0, false);
    // Four opaque positions in an otherwise transparent 8x8 tile.
    f.vram[0x11000] = 0x01; // (0,0)
    f.vram[0x11003] = 0x20; // (7,0)
    f.vram[0x11004] = 0x03; // (0,1)
    f.vram[0x1101f] = 0x40; // (7,7)
    auto image = render(f);
    expect(image, -4, 20, kGreen, "4bpp low nibble is opaque");
    expect(image, -3, 20, kRed, "4bpp high nibble is transparent");
    expect(image, -4, 21, kGreen, "next texture row");
    expect(image, -4, 22, kRed, "transparent texture row");
    expect(image, 3, 20, kGreen, "native high nibble remains opaque");
    expect(image, 3, 27, kGreen, "native bottom row remains opaque");
    center_is_native(f, image, "explicit texels");
    ++cases;

    f.object(6, -3, 20, 0, 0, false, 0, 129);
    f.vram[0x11020] = 0x01;
    image = render(f);
    expect(image, -4, 20, kGreen, "first stencil survives transparent overlapping OBJ");
    expect(image, -3, 20, kGreen, "second stencil unions with first");
    expect(image, -2, 20, kRed, "union does not fill transparent gap");
    ++cases;

    reset_policy();
    Fixture bpp8;
    bpp8.object(5, -4, 20, 0, 0, true);
    bpp8.vram[0x11000] = 0x10;
    bpp8.vram[0x11001] = 0x80;
    bpp8.vram[0x11002] = 0x00;
    bpp8.vram[0x11003] = 0xff;
    image = render(bpp8);
    expect(image, -4, 20, kGreen, "8bpp index 0x10 is opaque");
    expect(image, -3, 20, kGreen, "8bpp index 0x80 is opaque");
    expect(image, -2, 20, kRed, "8bpp zero is transparent");
    expect(image, -1, 20, kGreen, "8bpp index 0xff is opaque");
    center_is_native(bpp8, image, "8bpp full byte");
    ++cases;
}

void provenance_and_fallback() {
    const std::array<int, 12> full_positions{-32768, -1000, -512, -268, -24, 0,
                                            240, 268, 488, 512, 1000, 32767};
    for (int full_x : full_positions) {
        reset_policy();
        Fixture f;
        f.object(5, full_x, 40, 0, 2, false);
        f.texture(32, 32, false, true, 128, true);
        const auto image = render(f, 168, 168);
        center_is_native(f, image, "full signed X");
        for (int hx = -168; hx < 408; ++hx) {
            if (hx >= 0 && hx < 240) continue;
            expect(image, hx, 40, hx >= full_x && hx < full_x + 32 ? kGreen : kRed,
                   "full signed X must not alias hardware X");
        }
        ++cases;
    }
    // Every invalid callback result returns to precisely the pre-opt-in
    // conservative presentation, including non-uniform native mask rows.
    for (unsigned invalid = 0; invalid < 9; ++invalid) {
        reset_policy();
        Fixture f;
        f.object(5, -24, 40, 0, 2, false);
        f.texture(32, 32, false, true, 128, true);
        auto& p = placements[5];
        switch (invalid) {
        case 0: p.known = false; break;
        case 1: p.result = 0; break;
        case 2: p.result = -1; break;
        case 3: p.result = 2; break;
        case 4: p.x = -32769; break;
        case 5: p.x = 32768; break;
        case 6: p.x = std::numeric_limits<int>::min(); break;
        case 7: p.x = std::numeric_limits<int>::max(); break;
        case 8: p.x = -23; break; // Low-nine-bit mismatch.
        }
        const auto baseline = disabled_render(f);
        const auto image = render(f);
        require(image.bytes == baseline.bytes, "invalid provenance escaped conservative fallback");
        expect(image, -24, 40, kBlack, "unresolved nonuniform row remains black");
        ++cases;
    }

    reset_policy();
    Fixture mixed;
    mixed.object(5, -24, 40, 0, 2, false);
    mixed.object(6, 100, 40, 0, 2, false);
    placements[6].known = false;
    mixed.texture(32, 32, false, true, 128, true);
    require(render(mixed).bytes == disabled_render(mixed).bytes,
            "partially resolved row published a partial stencil");
    ++cases;

    reset_policy();
    Fixture partial;
    partial.object(6, -24, 40, 0, 2, false);
    partial.object(5, 100, 40, 0, 2, false);
    placements[5].known = false;
    partial.texture(32, 32, false, true, 128, true);
    require(render(partial).bytes == disabled_render(partial).bytes,
            "stencil written before unresolved lower index was not discarded");
    ++cases;

    // An unresolved OBJ on a different row cannot poison a proven row.
    reset_policy();
    mixed.object(5, -24, 40, 0, 2, false);
    mixed.object(6, 100, 100, 0, 2, false);
    placements[6].known = false;
    auto image = render(mixed);
    expect(image, -24, 40, kGreen, "off-row unresolved OBJ blocked a complete row");
    expect(image, -24, 100, kBlack, "unresolved active row was not conservative");
    ++cases;
}

void gates_and_window_precedence() {
    for (unsigned window_bits : {0x2000u, 0x4000u, 0x6000u}) {
        reset_policy();
        Fixture f;
        f.object(5, -24, 40, 0, 2, false);
        f.texture(32, 32, false, true, 128, true);
        f.dispcnt |= uint16_t(window_bits);
        store16(f.io.data() + 0x40, 0x0010); // WIN0 X [0,16).
        store16(f.io.data() + 0x42, 0x0818); // WIN1 X [8,24).
        store16(f.io.data() + 0x44, 0x00a0);
        store16(f.io.data() + 0x46, 0x00a0);
        store16(f.io.data() + 0x48, 0x0100); // WIN0 backdrop; WIN1 red.
        const auto baseline = disabled_render(f);
        const auto image = render(f);
        require(image.bytes == baseline.bytes, "rectangular window mode unexpectedly widened");
        center_is_native(f, image, "rectangular windows");
        if (window_bits & 0x2000u) expect(image, 4, 40, kWhite, "WIN0 priority over OBJ window");
        if (window_bits == 0x6000u) expect(image, 12, 40, kWhite, "WIN0 priority over WIN1");
        if (window_bits & 0x4000u) expect(image, 20, 40, kRed, "WIN1 selected outside WIN0");
        ++cases;
    }

    for (unsigned gate = 0; gate < 5; ++gate) {
        reset_policy();
        Fixture f;
        f.object(5, -24, 40, 0, 2, false);
        f.texture(32, 32, false, true, 128, true);
        if (gate == 0) gba::g_ws_pillarbox = 1;
        if (gate == 1) f.dispcnt &= uint16_t(~0x8000u);
        if (gate == 2) f.dispcnt |= 0x0080u; // Forced blank.
        if (gate == 3) gba::g_ws_obj_window_x_provider = nullptr;
        if (gate == 4) f.dispcnt &= uint16_t(~0x1000u); // OBJ master disabled.
        const auto baseline = disabled_render(f);
        const auto image = render(f);
        require(image.bytes == baseline.bytes, "disabled feature gate changed presentation");
        center_is_native(f, image, "feature gates");
        ++cases;
    }

    reset_policy();
    Fixture f;
    f.object(5, -24, 40, 0, 2, false);
    f.texture(32, 32, false, true, 128, true);
    gba::g_ws_pillarbox_left = 1;
    auto image = render(f);
    expect(image, -24, 40, kBlack, "left pillarbox remains authoritative");
    expect(image, 260, 40, kRed, "independent right margin survives left pillarbox");
    gba::g_ws_pillarbox_left = 0;
    gba::g_ws_pillarbox_right = 1;
    image = render(f);
    expect(image, -24, 40, kGreen, "independent left margin survives right pillarbox");
    expect(image, 260, 40, kBlack, "right pillarbox remains authoritative");
    ++cases;

    reset_policy();
    f = Fixture();
    f.object(5, -24, 0, 0, 2, false);
    f.texture(32, 32, false, true, 128, true);
    image = render(f, 58, 58, 24, 24);
    for (int y : {-24, -1, 160, 183}) {
        for (int x : {-58, -24, 0, 120, 239, 260, 297})
            expect(image, x, y, kBlack, "OBJ stencil cannot authorize vertical margins");
    }
    expect(image, -24, 0, kGreen, "native row next to vertical margin remains valid");
    center_is_native(f, image, "vertical margin gate");
    ++cases;
}

void brightness_and_empty_rows() {
    reset_policy();
    Fixture f;
    f.object(5, -24, 40, 0, 2, false);
    f.texture(32, 32, false, true, 128, true);
    store16(f.io.data() + 0x4a, 0x2101); // Same BG0; color effect only inside.
    store16(f.io.data() + 0x50, 0x00c1); // Darken BG0.
    store16(f.io.data() + 0x54, 8);
    const auto image = render(f);
    const auto native = render(f, 0, 0);
    expect(image, -24, 40, native.pixel(0, 40), "window color-effect bit reaches margins");
    require(image.pixel(-24, 40) != kRed && image.pixel(-24, 40) != kBlack,
            "lighting mask did not darken within GBA color range");
    expect(image, -25, 40, kRed, "outside window does not enable color effect");
    expect(image, -24, 39, kRed, "empty preceding row resolves WINOUT");
    expect(image, -24, 72, kRed, "empty following row resolves WINOUT");
    center_is_native(f, image, "brightness");
    ++cases;

    // The castle uses the opposite polarity: color effects outside its
    // window and no color effects inside. Verify that polarity independently.
    store16(f.io.data() + 0x4a, 0x0121);
    const auto inverted = render(f);
    expect(inverted, -24, 40, kRed, "inside window disables color effect");
    expect(inverted, -25, 40, image.pixel(-24, 40),
           "outside window enables color effect");
    center_is_native(f, inverted, "inverted brightness");
    ++cases;

    store16(f.io.data() + 0x4a, 0x2303); // Both BGs; blend enabled only inside.
    store16(f.io.data() + 0x50, 0x0241); // BG0 first, BG1 second, alpha mode.
    store16(f.io.data() + 0x52, 0x0808); // Half red plus half green.
    const auto alpha = render(f);
    const auto alpha_native = render(f, 0, 0);
    expect(alpha, -24, 40, alpha_native.pixel(0, 40), "alpha blend reaches margin");
    const auto blend = alpha.pixel(-24, 40);
    require(blend[0] > 0 && blend[0] < 255 && blend[1] > 0 && blend[1] < 255 &&
            blend[2] == 0, "two background layers did not alpha blend");
    expect(alpha, -25, 40, kRed, "outside alpha bit preserves top layer");
    center_is_native(f, alpha, "alpha blending");
    ++cases;

    reset_policy();
    Fixture uniform;
    // An all-native-width union is uniform in the original viewport but has
    // a real hole in the margins. Trusted stencils must defeat propagation of
    // the uniform native control, not blindly copy it into each margin.
    for (unsigned i = 0; i < 4; ++i) uniform.object(i, int(i * 64), 40, 0, 3, false);
    uniform.texture(64, 64, false, true, 128, true);
    const auto new_image = render(uniform);
    const auto old_image = disabled_render(uniform);
    expect(old_image, -1, 40, kGreen, "uniform-control fallback fixture");
    expect(new_image, -1, 40, kRed, "trusted stencil preserves real outside hole");
    expect(new_image, 250, 40, kGreen, "trusted rightmost stencil continuation");
    expect(new_image, 256, 40, kRed, "trusted stencil ends at its real width");
    center_is_native(uniform, new_image, "uniform native union");
    ++cases;
}

void wrapped_y_and_ignored_entries() {
    for (int y : {-8, 158, 160}) {
        reset_policy();
        Fixture f;
        f.object(5, -8, y, 0, 1, false);
        f.texture(16, 16, false, true, 128, true);
        const auto image = render(f);
        center_is_native(f, image, "Y wrapping");
        const int hardware_y = y >= 160 ? y - 256 : y;
        for (int row = 0; row < 160; ++row)
            expect(image, -8, row,
                   row >= hardware_y && row < hardware_y + 16 ? kGreen : kRed,
                   "signed hardware Y clipping");
        ++cases;
    }

    // Disabled, prohibited-shape and non-window OBJs have no stencil and do
    // not make row completeness depend on unavailable provenance.
    for (unsigned ignored = 0; ignored < 3; ++ignored) {
        reset_policy();
        Fixture f;
        f.object(5, -24, 40, 0, 2, false);
        f.texture(32, 32, false, true, 128, true);
        uint16_t a0 = read16(f.oam.data() + 5 * 8);
        if (ignored == 0) a0 |= 0x0200u;
        if (ignored == 1) a0 |= 0xc000u;
        if (ignored == 2) a0 |= 0x0400u; // OBJ mode 3.
        store16(f.oam.data() + 5 * 8, a0);
        placements[5].known = false;
        const auto image = render(f);
        require(provider_calls == 0, "ignored OBJ requested window provenance");
        expect(image, -24, 40, kRed, "ignored OBJ changed window selection");
        center_is_native(f, image, "ignored OBJ");
        ++cases;
    }

    reset_policy();
    Fixture edge;
    edge.object(5, -32, 40, 0, 3, false, 0, 1023);
    std::fill_n(edge.vram.begin() + 0x17fe0, 32, uint8_t(0x11));
    const auto image = render(edge);
    expect(image, -32, 40, kGreen, "last valid OBJ tile");
    expect(image, -24, 40, kRed, "OBJ tile beyond VRAM is transparent");
    expect(image, -32, 48, kRed, "OBJ tile row beyond VRAM is transparent");
    center_is_native(edge, image, "VRAM boundary");
    ++cases;
}

void viewport_boundaries() {
    constexpr std::pair<unsigned, unsigned> margins[] = {
        {0, 0}, {1, 0}, {0, 1}, {1, 1}, {58, 57}, {168, 0}, {0, 168}, {168, 168}};
    for (const auto& [left, right] : margins) {
        reset_policy();
        Fixture f;
        f.object(5, -4, 20, 0, 0, false);
        f.object(6, 236, 20, 0, 0, false);
        f.texture(8, 8, false, true, 128, true);
        const auto image = render(f, left, right);
        center_is_native(f, image, "viewport boundary");
        for (int x = -int(left); x < 240 + int(right); ++x) {
            const bool inside = (x >= -4 && x < 4) || (x >= 236 && x < 244);
            expect(image, x, 20, inside ? kGreen : kRed,
                   "one-column and asymmetric viewport mask");
        }
        ++cases;
    }
}
} // namespace

int main() {
    geometry_against_native_oracle();
    explicit_pixels_and_union();
    provenance_and_fallback();
    gates_and_window_precedence();
    brightness_and_empty_rows();
    wrapped_y_and_ignored_entries();
    viewport_boundaries();
    reset_policy();
    gba::g_ws_obj_window_x_provider = nullptr;
    gba::g_ws_tilemap_provider = nullptr;
    std::printf("PASS: %u synthetic scenarios, %llu internal assertions; real PPU, native center unchanged\n",
                cases, static_cast<unsigned long long>(checks));
}
