#pragma once

// The sprite renderer samples a private copy of Yoshi's palette. Selection is
// by scene, OBJ format, palette bank and the player's dynamic tile reservation,
// never by matching green pixels in the final image.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sma3::yoshi_color_detail {

constexpr unsigned kPaletteOffset = 5 * 32;
constexpr std::array<unsigned, 3> kBodyEntries{2, 5, 9};

constexpr std::uint16_t read16(const std::uint8_t* bytes) noexcept {
    return std::uint16_t(bytes[0] | (unsigned(bytes[1]) << 8));
}
constexpr std::uint32_t read32(const std::uint8_t* bytes) noexcept {
    return std::uint32_t(read16(bytes)) | (std::uint32_t(read16(bytes + 2)) << 16);
}
inline void write16(std::uint8_t* bytes, std::uint16_t value) noexcept {
    bytes[0] = std::uint8_t(value);
    bytes[1] = std::uint8_t(value >> 8);
}

// The ordinary playable Yoshi owns OBJ tiles 0..63 and palette bank 5.
// Baby Mario shares bank 5 but has tiles 0xC2/0xC4, so a bank-wide override
// would wrongly recolor him. Transformations, map and cutscene art keep their
// original colors until their separate tile layouts have been qualified.
inline bool supported_scene(const std::uint8_t* iwram, std::size_t size,
                            unsigned dispcnt) noexcept {
    if (!iwram || size < 0x7244 || (dispcnt & 7u) != 0) return false;
    if (iwram[0x6d64] != 0 || read32(iwram + 0x7010) != 0x0200000c ||
            read32(iwram + 0x7240) != 0x0300220c ||
            read16(iwram + 0x6db2) != 0) return false;
    const unsigned state = iwram[0x6b05];
    return (state >= 0x09 && state <= 0x10) || state == 0x22 || state == 0x23;
}

constexpr bool player_object(unsigned attr0, unsigned attr2) noexcept {
    return (attr0 & 0x2000u) == 0 && ((attr0 >> 10) & 3u) < 2 &&
           (attr2 >> 12) == 5 && (attr2 & 0x3ffu) < 64;
}

// Matches YoshiColorModel.java. Round RGB888 to the nearest RGB555 value,
// then derive readable body shadows and highlights in the same 5-bit space.
constexpr unsigned channel5(unsigned value) noexcept {
    return (value * 31u + 127u) / 255u;
}
constexpr std::uint16_t body_shade(std::uint32_t rgb, unsigned shade) noexcept {
    std::uint16_t output = 0;
    for (unsigned channel = 0; channel < 3; ++channel) {
        unsigned value = channel5((rgb >> (16 - channel * 8)) & 255u);
        if (shade == 0) value = (value * 18u + 12u) / 25u;
        else if (shade == 2) value += ((31u - value) * 7u + 12u) / 25u;
        output |= std::uint16_t(value << (channel * 5));
    }
    return output;
}

class Palette {
public:
    // The verified USA ROM supplies its existing eight Yoshi palettes and
    // software-fade step table. No game art or table is embedded in this mod.
    bool initialize(const std::uint8_t* rom, std::size_t size) noexcept {
        constexpr std::size_t palette_address = 0x2d301c;
        constexpr std::size_t fade_address = 0x1a82d8;
        initialized_ = false;
        cache_valid_ = false;
        if (!rom || size < palette_address + 8 * 32 ||
                size < fade_address + 32 * 32) return false;
        for (unsigned variant = 0; variant < 8; ++variant) {
            for (unsigned entry = 0; entry < 16; ++entry) {
                const auto color = read16(rom + palette_address + variant * 32 + entry * 2);
                if (color & 0x8000u) return false;
                originals_[variant][entry] = color;
            }
            if (originals_[variant][15] != 0x7fffu) return false;
        }
        for (unsigned value = 0; value < 32; ++value) {
            unsigned remaining = value;
            fade_[0][value] = std::uint8_t(value);
            for (unsigned step = 0; step < 32; ++step) {
                const unsigned decrement = rom[fade_address + value * 32 + step];
                if (decrement > remaining) return false;
                remaining -= decrement;
                fade_[step + 1][value] = std::uint8_t(remaining);
            }
            if (remaining != 0) return false;
        }
        initialized_ = true;
        return true;
    }

    void color(std::uint32_t rgb) noexcept {
        rgb &= 0xffffffu;
        if (rgb == rgb_ && color_ready_) return;
        rgb_ = rgb;
        color_ready_ = true;
        for (unsigned i = 0; i < custom_.size(); ++i) custom_[i] = body_shade(rgb, i);
        cache_valid_ = false;
    }

    // Returns a 512-byte private OBJ palette, or null for the authentic one.
    // Only the three body colors change. Cached palette comparisons happen
    // once per eligible OBJ; all color work stays outside the pixel loop.
    const std::uint8_t* for_object(unsigned attr0, unsigned attr2,
                                  const std::uint8_t* object_palette) noexcept {
        if (!initialized_ || !color_ready_ || !object_palette ||
                !player_object(attr0, attr2)) return nullptr;
        const auto* bank = object_palette + kPaletteOffset;
        if (!cache_valid_ || std::memcmp(bank, cached_.data(), cached_.size()) != 0) {
            std::memcpy(cached_.data(), bank, cached_.size());
            cache_valid_ = true;
            recognized_ = false;
            unsigned selected_step = 0;
            bool selected_white = false;
            for (unsigned variant = 0; variant < 8 && !recognized_; ++variant) {
                for (unsigned step = 0; step <= 32 && !recognized_; ++step) {
                    for (unsigned white = 0; white <= (step != 0); ++white) {
                        bool match = true;
                        for (unsigned entry = 0; entry < 16; ++entry) {
                            if (read16(bank + entry * 2) !=
                                    faded(originals_[variant][entry], step, white != 0)) {
                                match = false;
                                break;
                            }
                        }
                        if (!match) continue;
                        recognized_ = true;
                        selected_step = step;
                        selected_white = white != 0;
                        break;
                    }
                }
            }
            if (recognized_) {
                std::memcpy(replacement_.data(), object_palette, replacement_.size());
                for (unsigned i = 0; i < kBodyEntries.size(); ++i)
                    write16(replacement_.data() + kPaletteOffset + kBodyEntries[i] * 2,
                            faded(custom_[i], selected_step, selected_white));
            }
        }
        return recognized_ ? replacement_.data() : nullptr;
    }

private:
    std::uint16_t faded(std::uint16_t color, unsigned step, bool white) const noexcept {
        std::uint16_t output = 0;
        for (unsigned channel = 0; channel < 3; ++channel) {
            const unsigned value = (color >> (channel * 5)) & 31u;
            const unsigned result = white ? 31u - fade_[step][31u - value] : fade_[step][value];
            output |= std::uint16_t(result << (channel * 5));
        }
        return output;
    }

    std::array<std::array<std::uint16_t, 16>, 8> originals_{};
    std::array<std::array<std::uint8_t, 32>, 33> fade_{};
    std::array<std::uint16_t, 3> custom_{};
    std::array<std::uint8_t, 32> cached_{};
    std::array<std::uint8_t, 512> replacement_{};
    std::uint32_t rgb_ = 0;
    bool initialized_ = false;
    bool color_ready_ = false;
    bool cache_valid_ = false;
    bool recognized_ = false;
};

}  // namespace sma3::yoshi_color_detail
