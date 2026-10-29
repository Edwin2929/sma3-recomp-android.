// Selective palette and real-PPU regression tests. No game assets required.
// Build with the pinned, patched gba_ppu.cpp and src/yoshi_color.cpp.
#include "../src/yoshi_color.h"
#include "../src/yoshi_color_palette.h"
#include "../src/presentation_pacing.h"
#include "gba_ppu.h"
#include "runtime_bus_bridge.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

// These tests exercise pure palette selection and the production compositor;
// a live game/ROM is deliberately not available to the public API bridge.
namespace gbarecomp { gba::GbaBus* active_bus() { return nullptr; } }

namespace {
using sma3::yoshi_color_detail::Palette;
using Bytes = std::vector<std::uint8_t>;
using Rgb = std::array<std::uint8_t, 3>;
unsigned long long checks = 0;
unsigned scenarios = 0;

void require(bool condition, const char* label) {
    ++checks;
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", label);
    std::exit(1);
}
void put16(std::uint8_t* p, unsigned value) {
    p[0] = std::uint8_t(value);
    p[1] = std::uint8_t(value >> 8);
}
void put32(std::uint8_t* p, unsigned value) {
    put16(p, value);
    put16(p + 2, value >> 16);
}
unsigned get16(const std::uint8_t* p) { return p[0] | (unsigned(p[1]) << 8); }
Rgb rgb555(unsigned value) {
    Rgb result{};
    for (unsigned channel = 0; channel < 3; ++channel) {
        const unsigned part = (value >> (channel * 5)) & 31u;
        result[channel] = std::uint8_t((part << 3) | (part >> 2));
    }
    return result;
}

constexpr std::array<unsigned, 16> green{
    0x17e5, 0x14a5, 0x16c5, 0x001f, 0x2577, 0x1f87, 0x319f, 0x5b1f,
    0x25bf, 0x37ee, 0x631f, 0x16df, 0x3bbf, 0x6bbf, 0x73ff, 0x7fff};

unsigned reference(unsigned variant, unsigned entry) {
    if (variant == 0 || (entry != 2 && entry != 5 && entry != 9)) return green[entry];
    return ((variant * 3 + entry) & 31u) |
           (((variant * 5 + entry * 2) & 31u) << 5) |
           (((variant * 7 + entry * 3) & 31u) << 10);
}
// Deliberately synthetic fade table with exact, known transfer. Production
// reads the actual game's different table; this fixture proves it uses the
// supplied step table instead of estimating brightness from the chosen color.
unsigned faded(unsigned color, unsigned step, bool white) {
    unsigned result = 0;
    for (unsigned shift : {0u, 5u, 10u}) {
        const unsigned value = (color >> shift) & 31u;
        const unsigned part = white ? 31u - ((31u - value) * (32u - step) / 32u)
                                    : value * (32u - step) / 32u;
        result |= part << shift;
    }
    return result;
}
Bytes source_data() {
    Bytes rom(0x2d301c + 8 * 32);
    for (unsigned variant = 0; variant < 8; ++variant)
        for (unsigned entry = 0; entry < 16; ++entry)
            put16(rom.data() + 0x2d301c + variant * 32 + entry * 2, reference(variant, entry));
    for (unsigned value = 0; value < 32; ++value)
        for (unsigned step = 0; step < 32; ++step)
            rom[0x1a82d8 + value * 32 + step] = std::uint8_t(
                value * (32 - step) / 32 - value * (31 - step) / 32);
    return rom;
}
void set_bank(std::uint8_t* pal, unsigned variant = 0, unsigned step = 0,
              bool white = false) {
    for (unsigned i = 0; i < 16; ++i)
        put16(pal + 0xa0 + i * 2, faded(reference(variant, i), step, white));
}

void scene_and_object_contract() {
    ++scenarios;
    std::array<std::uint8_t, 0x8000> ram{};
    put32(ram.data() + 0x7010, 0x0200000c);
    put32(ram.data() + 0x7240, 0x0300220c);
    using sma3::yoshi_color_detail::supported_scene;
    for (unsigned state = 0; state < 256; ++state) {
        ram[0x6b05] = std::uint8_t(state);
        const bool expected = (state >= 9 && state <= 16) || state == 34 || state == 35;
        require(supported_scene(ram.data(), ram.size(), 0x1140) == expected, "scene whitelist");
    }
    ram[0x6b05] = 13;
    require(!supported_scene(nullptr, ram.size(), 0x1140), "null RAM");
    require(!supported_scene(ram.data(), 0x7243, 0x1140), "short RAM");
    for (unsigned mode = 1; mode < 8; ++mode)
        require(!supported_scene(ram.data(), ram.size(), 0x1140 | mode), "non-tile scene");
    ram[0x6d64] = 1;
    require(!supported_scene(ram.data(), ram.size(), 0x1140), "alternate game mode");
    ram[0x6d64] = 0;
    for (unsigned transform : {1u, 2u, 4u, 6u, 8u, 65535u}) {
        put16(ram.data() + 0x6db2, transform);
        require(!supported_scene(ram.data(), ram.size(), 0x1140), "transformation keeps native art");
    }
    put16(ram.data() + 0x6db2, 0);
    put32(ram.data() + 0x7010, 0x02000010);
    require(!supported_scene(ram.data(), ram.size(), 0x1140), "bad EWRAM scene owner");
    put32(ram.data() + 0x7010, 0x0200000c);
    put32(ram.data() + 0x7240, 0x03002200);
    require(!supported_scene(ram.data(), ram.size(), 0x1140), "bad IWRAM scene owner");

    using sma3::yoshi_color_detail::player_object;
    for (unsigned bank = 0; bank < 16; ++bank)
        for (unsigned tile = 0; tile < 1024; ++tile)
            require(player_object(0, bank * 4096 + tile) == (bank == 5 && tile < 64),
                    "bank and dynamic tile reservation");
    require(player_object(0x0100, 0x5000), "affine player allowed");
    require(player_object(0x0400, 0x5000), "semi-transparent player allowed");
    require(!player_object(0x2000, 0x5000), "8bpp cannot alias bank 5");
    require(!player_object(0x0800, 0x5000), "OBJ-window cannot be recolored");
    require(!player_object(0x0c00, 0x5000), "prohibited OBJ mode");
}

void exact_colors_fades_and_cache() {
    ++scenarios;
    const auto rom = source_data();
    Palette palette;
    std::array<std::uint8_t, 512> input{};
    set_bank(input.data());
    require(!palette.for_object(0, 0x5000, input.data()), "uninitialized reference rejected");
    require(!palette.initialize(nullptr, rom.size()), "null reference rejected");
    require(!palette.initialize(rom.data(), rom.size() - 1), "short reference rejected");
    require(palette.initialize(rom.data(), rom.size()), "valid synthetic reference accepted");
    require(!palette.for_object(0, 0x5000, input.data()), "no custom color yet");

    struct Choice { unsigned rgb; std::array<unsigned, 3> tones; };
    const std::array<Choice, 6> choices{{
        {0xff0000, {0x0016, 0x001f, 0x253f}},
        {0x000000, {0x0000, 0x0000, 0x2529}},
        {0xffffff, {0x5ad6, 0x7fff, 0x7fff}},
        {0x00ffff, {0x5ac0, 0x7fe0, 0x7fe9}},
        {0xa855f7, {0x58ee, 0x7954, 0x7a17}},
        {0x39e739, {0x1685, 0x1f87, 0x3bae}},
    }};
    for (const auto& choice : choices) {
        palette.color(choice.rgb);
        for (unsigned variant = 0; variant < 8; ++variant) {
            for (bool white : {false, true}) for (unsigned step = 0; step <= 32; ++step) {
                set_bank(input.data(), variant, step, white);
                const auto before = input;
                const auto* mapped = palette.for_object(0, 0x5000, input.data());
                require(mapped != nullptr, "original or supported software fade recognized");
                require(input == before, "guest palette remains immutable");
                for (unsigned i = 0; i < 256; ++i) {
                    unsigned expected = get16(input.data() + i * 2);
                    if (i == 0x52) expected = faded(choice.tones[0], step, white);
                    if (i == 0x55) expected = faded(choice.tones[1], step, white);
                    if (i == 0x59) expected = faded(choice.tones[2], step, white);
                    require(get16(mapped + i * 2) == expected, "only three shaded body entries change");
                }
                require(palette.for_object(0, 0x5000, input.data()) == mapped, "repeat reuses stable cache");
            }
        }
    }
    set_bank(input.data());
    require(palette.for_object(0, 0x5000, input.data()), "restored original reference");
    input[0xa4] ^= 0x40;
    require(!palette.for_object(0, 0x5000, input.data()), "unknown palette keeps original");
    require(!palette.for_object(0, 0x5000, input.data()), "unknown result cached safely");
    set_bank(input.data());
    require(palette.for_object(0, 0x5000, input.data()), "unknown-to-original cache recovery");
    require(!palette.for_object(0, 0x50c2, input.data()), "Baby Mario same-bank tile excluded");
    require(!palette.for_object(0x2000, 0x5000, input.data()), "256-color palette excluded");
    require(!palette.for_object(0, 0x1000, input.data()), "different bank excluded");
    require(!palette.for_object(0, 0x5000, nullptr), "null palette excluded");
    auto invalid = rom;
    invalid[0x1a82d8] = 1; // value zero cannot lose one unit.
    require(!palette.initialize(invalid.data(), invalid.size()), "invalid fade cannot underflow");
    require(!palette.for_object(0, 0x5000, input.data()), "invalid reference cannot reuse old replacement");
    require(palette.initialize(rom.data(), rom.size()), "reference recovery");
    require(palette.for_object(0, 0x5000, input.data()), "reference recovery refreshes palette");
}

Palette render_palette;
const std::uint8_t* provider(int, std::uint16_t a0, std::uint16_t,
                             std::uint16_t a2, const std::uint8_t* pal) {
    return render_palette.for_object(a0, a2, pal);
}

struct Fixture {
    std::array<std::uint8_t, 0x400> io{}, pal{}, oam{};
    std::array<std::uint8_t, 0x18000> vram{};
    std::uint16_t dispcnt = 0x1140; // Mode 0, BG0 + 4bpp OBJ, 1D.
    Fixture() {
        for (unsigned i = 0; i < 128; ++i) put16(oam.data() + i * 8, 0x0200);
        set_bank(pal.data() + 0x200);
        put16(pal.data() + 2, 0x7c00); // blue BG / same native-green trap below
        put16(io.data() + 8, 0x1f00); // BG0 screen block 31, char block 0.
        std::fill(vram.begin(), vram.begin() + 32, 0x11);
        // One tile: dark, base, light, white, outline, shoe, skin, transparent.
        const std::array<unsigned, 8> indices{2, 5, 9, 15, 1, 3, 4, 0};
        for (unsigned tile : {0u, 64u, 194u}) {
            for (unsigned y = 0; y < 8; ++y)
                for (unsigned x = 0; x < 8; x += 2)
                    vram[0x10000 + tile * 32 + y * 4 + x / 2] =
                        std::uint8_t(indices[x] | (indices[x + 1] << 4));
        }
        for (unsigned i = 0; i < 5; ++i) {
            put16(oam.data() + i * 8, 40);
            put16(oam.data() + i * 8 + 2, 32 + i * 24);
            put16(oam.data() + i * 8 + 4, i == 1 ? 0x50c2 :
                i == 2 ? 0x5040 : i == 3 ? 0x4000 : 0x5000);
        }
        // Palette 4 has identical values: a global pixel replacement would
        // change this unrelated sprite; per-object selection must not.
        std::memcpy(pal.data() + 0x280, pal.data() + 0x2a0, 32);
        put16(oam.data() + 4 * 8, 0x2028); // 8bpp, low tile and bank bits5.
    }
};
Bytes draw(const Fixture& f, bool enabled, unsigned width = 240) {
    auto ppu = std::make_unique<gba::GbaPpu>();
    if (width != 240) ppu->set_view_margins((width - 240) / 2, (width - 240) / 2, 0, 0);
    gba::g_ws_pillarbox = 0;
    gba::g_ws_obj_native_clip = 0;
    gba::g_obj_palette_provider = enabled ? provider : nullptr;
    Bytes output(width * 160 * 3);
    ppu->render(output.data(), f.dispcnt, f.io.data(), f.vram.data(), f.oam.data(), f.pal.data());
    gba::g_obj_palette_provider = nullptr;
    return output;
}
Rgb pixel(const Bytes& image, unsigned width, unsigned x, unsigned y) {
    const auto offset = (y * width + x) * 3;
    return {image[offset], image[offset + 1], image[offset + 2]};
}

void real_compositor() {
    ++scenarios;
    const auto rom = source_data();
    require(render_palette.initialize(rom.data(), rom.size()), "renderer reference init");
    render_palette.color(0xff0000);
    Fixture f;
    const auto before_io = f.io, before_pal = f.pal, before_oam = f.oam;
    const auto before_vram = f.vram;
    for (unsigned width : {240u, 356u}) {
        const unsigned left = (width - 240) / 2;
        auto original = draw(f, false, width);
        const auto changed = draw(f, true, width);
        unsigned differences = 0;
        for (unsigned y = 0; y < 160; ++y) for (unsigned x = 0; x < width; ++x) {
            const bool body = y >= 40 && y < 48 && x >= left + 32 && x < left + 35;
            const auto old = pixel(original, width, x, y), now = pixel(changed, width, x, y);
            if (old != now) ++differences;
            require((old != now) == body, "only visible Yoshi body pixels changed");
        }
        require(differences == 24, "all three body shades appeared");
        require(pixel(changed, width, left + 32, 40) == rgb555(0x0016), "rendered red shadow");
        require(pixel(changed, width, left + 33, 40) == rgb555(0x001f), "rendered exact red base");
        require(pixel(changed, width, left + 34, 40) == rgb555(0x253f), "rendered red highlight");
        require(pixel(changed, width, left + 35, 40) == Rgb{255,255,255}, "white belly untouched");
        require(draw(f, false, width) == original, "disable restores exact native frame");
        // Hardware brightness/window masks apply after the new palette, so
        // there can be no full-bright Yoshi over a dark or hidden background.
        Fixture dark = f;
        put16(dark.io.data() + 0x50, 0x00d0); // darken OBJ, EVY16.
        put16(dark.io.data() + 0x54, 16);
        require(draw(dark, true, width) == draw(dark, false, width), "full hardware fade remains black");
        Fixture hidden = f;
        hidden.dispcnt |= 0x2000; // WIN0 hides every layer inside full viewport.
        put16(hidden.io.data() + 0x40, 240);
        put16(hidden.io.data() + 0x44, 160);
        put16(hidden.io.data() + 0x48, 0);
        put16(hidden.io.data() + 0x4a, 0x3f);
        require(draw(hidden, true, width) == draw(hidden, false, width), "window stencil remains authoritative");
        Fixture blank = f;
        blank.dispcnt |= 0x80;
        require(draw(blank, true, width) == draw(blank, false, width), "forced blank remains authentic");
    }
    require(f.io == before_io && f.pal == before_pal && f.oam == before_oam && f.vram == before_vram,
            "render does not write guest video memory");

    // The same per-object palette must also be used in the per-scanline path
    // consumed during ordinary gameplay, including an affine player sprite.
    Fixture affine;
    put16(affine.oam.data(), 0x0128);
    put16(affine.oam.data() + 6, 256);
    put16(affine.oam.data() + 14, 0);
    put16(affine.oam.data() + 22, 0);
    put16(affine.oam.data() + 30, 256);
    for (unsigned width : {240u, 356u}) {
        auto ppu = std::make_unique<gba::GbaPpu>();
        if (width != 240) ppu->set_view_margins(58,58,0,0);
        gba::g_obj_palette_provider = provider;
        for (unsigned y = 0; y < 160; ++y)
            ppu->render_scanline(y, affine.dispcnt, affine.io.data(), affine.vram.data(),
                                 affine.oam.data(), affine.pal.data());
        ppu->mark_framebuffer_latched();
        const auto expected = draw(affine, true, width);
        require(std::memcmp(ppu->latched_framebuffer(), expected.data(), expected.size()) == 0,
                "affine live scanline and snapshot render agree");
    }
    gba::g_obj_palette_provider = nullptr;
}

void object_color_effects() {
    // Regression found while qualifying the color mod: the old renderer
    // treated only semi-transparent OBJ as first targets, so an ordinary
    // selected OBJ ignored BLDCNT alpha and brightness. Endpoints make this
    // selection oracle independent of the renderer's channel-rounding math.
    ++scenarios;
    unsigned combinations = 0;
    for (unsigned width : {240u, 356u}) for (bool mod : {false, true})
        for (bool semi : {false, true}) for (bool first : {false, true})
            for (bool second : {false, true}) for (bool effects : {false, true})
                for (unsigned effect = 0; effect < 4; ++effect) {
        Fixture f;
        if (semi) put16(f.oam.data(), 0x0428);
        put16(f.io.data() + 0x50, (effect << 6) | (first ? 0x10 : 0) | (second ? 0x100 : 0));
        put16(f.io.data() + 0x52, 0x1000); // EVA0 / EVB16: select the lower blue BG.
        put16(f.io.data() + 0x54, 16); // Full white or black.
        if (!effects) {
            f.dispcnt |= 0x2000;
            put16(f.io.data() + 0x40, 240);
            put16(f.io.data() + 0x44, 160);
            put16(f.io.data() + 0x48, 0x1f); // Layers visible, SFX disabled in WIN0.
            put16(f.io.data() + 0x4a, 0x3f);
        }
        Rgb expected = mod ? Rgb{255,0,0} : rgb555(green[5]);
        if (effects) {
            if (second && (semi || (effect == 1 && first))) expected = {0,0,255};
            else if (first && effect == 2) expected = {255,255,255};
            else if (first && effect == 3) expected = {0,0,0};
        }
        const auto result = draw(f, mod, width);
        if (pixel(result, width, (width - 240) / 2 + 33, 40) != expected) {
            std::fprintf(stderr, "effect case width=%u mod=%d semi=%d first=%d second=%d window_effect=%d effect=%u\n",
                         width, mod, semi, first, second, effects, effect);
            require(false, "OBJ first-target and forced-alpha decision");
        }
        ++checks;
        ++combinations;
    }
    require(combinations == 256, "complete OBJ effect selection matrix");

    for (unsigned width : {240u, 356u}) {
        Fixture f;
        put16(f.io.data() + 0x50, 0x00d0); // selected ordinary OBJ, darken
        put16(f.io.data() + 0x54, 0); // EVY0 leaves all colors intact.
        auto no_effect = draw(f, true, width);
        require(pixel(no_effect, width, (width - 240) / 2 + 33, 40) == Rgb{255,0,0},
                "zero brightness coefficient preserves color");
        put16(f.io.data() + 0x54, 31); // Clamp EVY above16.
        require(pixel(draw(f, true, width), width, (width - 240) / 2 + 33, 40) == Rgb{0,0,0},
                "brightness coefficient clamps at full fade");
        // A disabled background cannot become the second target; forced-alpha
        // OBJ without a real lower target follows BLDCNT's ordinary first bit.
        f.dispcnt &= ~0x100;
        put16(f.oam.data(), 0x0428);
        put16(f.io.data() + 0x50, 0x01c0); // first OBJ disabled, second BG0 selected but off
        put16(f.io.data() + 0x52, 0x1000);
        require(pixel(draw(f, true, width), width, (width - 240) / 2 + 33, 40) == Rgb{255,0,0},
                "disabled second-target BG does not darken unselected semi OBJ");
    }
}

void atomic_request_and_epoch() {
    ++scenarios;
    sma3::set_yoshi_color(false, 0);
    const auto start = sma3::presentation_epoch.load();
    sma3::set_yoshi_color(false, 0x123456);
    require(sma3::presentation_epoch.load() == start, "disabled draft does not invalidate presentation");
    sma3::set_yoshi_color(true, 0x123456);
    require(sma3::presentation_epoch.load() == start + 1, "enable invalidates old blend history");
    sma3::set_yoshi_color(true, 0x123456);
    require(sma3::presentation_epoch.load() == start + 1, "same setting keeps epoch");
    sma3::set_yoshi_color(true, 0x1000000);
    require(sma3::presentation_epoch.load() == start + 1, "ARGB or out-of-range request rejected");
    sma3::set_yoshi_color(true, 0xffffff);
    require(sma3::presentation_epoch.load() == start + 2, "color change invalidates old blend history");
    gba::g_obj_palette_provider = provider;
    sma3::yoshi_color_frame();
    require(gba::g_obj_palette_provider == nullptr, "no bus means no palette override");
    sma3::set_yoshi_color(false, 0xffffff);
    require(sma3::presentation_epoch.load() == start + 3, "disable invalidates old blend history");
    sma3::yoshi_color_frame();
    require(gba::g_obj_palette_provider == nullptr, "disabled path leaves no callback");
}
}  // namespace

int main() {
    scene_and_object_contract();
    exact_colors_fades_and_cache();
    real_compositor();
    object_color_effects();
    atomic_request_and_epoch();
    std::printf("PASS: %u scenarios, %llu checks; selective Yoshi color, 8 references, software/hardware fades, real native/wide PPU, immutable memory, restore and request epoch\n",
                scenarios, checks);
}
