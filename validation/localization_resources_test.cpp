#include "localization.h"
#include "localization_data.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>

namespace {
unsigned long long checks = 0;
void require(bool ok, const char* label) {
    ++checks;
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", label);
        std::exit(1);
    }
}

template <std::size_t Size>
void check_bank(uint32_t base, const uint8_t (&bytes)[Size]) {
    for (int lang = 0; lang < 3; ++lang) {
        sma3::language.store(lang);
        for (uint32_t mirror : {0u, 0x02000000u, 0x04000000u}) {
            for (uint32_t offset = 0; offset < Size; ++offset) {
                for (uint32_t width : {1u, 2u, 4u}) {
                    uint32_t expected = 0;
                    for (uint32_t i = 0; i < width; ++i)
                        expected |= uint32_t(offset + i < Size ? bytes[offset + i] : 0xFFu) << (8 * i);
                    uint32_t value = 0;
                    require(sma3::localized_read(0, base + mirror + offset, width, 0, &value) == 1,
                            "virtual byte and cross-boundary read handled");
                    require(value == expected, "immutable bank bytes, width, padding and ROM mirrors");
                }
            }
            uint32_t value = 0x12345678;
            require(!sma3::localized_read(0, base + mirror - 1, 1, 0, &value), "before bank rejected");
            require(!sma3::localized_read(0, base + mirror + Size, 1, 0, &value), "after bank rejected");
            require(value == 0x12345678, "miss preserves output");
        }
    }
    // A packet/resource resolved earlier keeps its language when the user
    // changes the selection between reads or resumes a stored virtual pointer.
    for (uint32_t i = 0; i < Size; ++i) {
        sma3::language.store(i % 3);
        uint32_t value = 0;
        require(sma3::localized_read(0, base + i, 1, 0, &value) && value == bytes[i],
                "selection changes do not mutate an already resolved resource");
    }
}

template <class Pointer, std::size_t Size>
void check_translated_pointers(const Pointer (&pointers)[Size]) {
    for (std::size_t i = 1; i < Size; ++i)
        require(pointers[i - 1].address < pointers[i].address, "strictly sorted unique pointer slots");
    for (int lang = 0; lang < 3; ++lang) {
        sma3::language.store(lang);
        for (const auto& pointer : pointers) {
            for (uint32_t mirror : {0u, 0x02000000u, 0x04000000u}) {
                uint32_t value = 0x12345678;
                const int hit = sma3::localized_read(0, pointer.address + mirror, 4, pointer.original, &value);
                require(hit == (lang != 0), "only the requested language redirects a verified pointer");
                require(value == (lang ? (lang == 1 ? pointer.spanish : pointer.portuguese) : 0x12345678u),
                        "correct pointer destination or unchanged English output");
                if (lang) {
                    uint32_t first = 0;
                    require(sma3::localized_read(0, value, 1, 0, &first), "pointer resolves into a readable bank");
                }
                value = 0x12345678;
                require(!sma3::localized_read(0, pointer.address + mirror, 4, pointer.original ^ 4, &value),
                        "modified original pointer rejected");
                for (uint32_t width : {1u, 2u})
                    require(!sma3::localized_read(0, pointer.address + mirror, width, pointer.original, &value),
                            "partial pointer access rejected");
                require(value == 0x12345678, "pointer miss preserves output");
            }
        }
    }
}
} // namespace

int main() {
    check_bank(0x09E00000u, sma3::spanish_text);
    check_bank(0x09F00000u, sma3::portuguese_text);
    check_bank(sma3::graphics_es_base, sma3::graphics_es);
    check_bank(sma3::graphics_pt_base, sma3::graphics_pt);
    check_bank(sma3::credits_resource_base, sma3::credits_resources);
    check_bank(0x082F684Cu, sma3::portuguese_glyphs);
    check_translated_pointers(sma3::text_pointers);
    check_translated_pointers(sma3::graphics_pointers);

    for (int lang = 0; lang < 3; ++lang) {
        sma3::language.store(lang);
        for (const auto& pointer : sma3::credits_resource_pointers) {
            for (uint32_t mirror : {0u, 0x02000000u, 0x04000000u}) {
                uint32_t value = 0;
                require(sma3::localized_read(0, pointer.address + mirror, 4, pointer.original, &value) &&
                        value == pointer.target, "credits font literals resolve stably for all languages");
                require(sma3::localized_read(0, value, 1, 0, &value), "credits font target readable");
                value = 0x12345678;
                require(!sma3::localized_read(0, pointer.address + mirror, 4, pointer.original ^ 4, &value),
                        "credits font value guard");
                for (uint32_t width : {1u, 2u})
                    require(!sma3::localized_read(0, pointer.address + mirror, width, pointer.original, &value),
                            "credits font pointer width guard");
                require(value == 0x12345678, "font pointer miss preserves output");
            }
        }
    }

    for (uint32_t address : {0u, 0x03006394u, 0x07FFFFFFu, 0x0E000000u, 0xFFE00000u}) {
        uint32_t value = 0x12345678;
        require(!sma3::localized_read(0, address, 4, 0, &value), "non-ROM addresses rejected");
        require(value == 0x12345678, "non-ROM read output unchanged");
    }
    uint32_t value = 0x12345678;
    for (uint32_t width : {0u, 3u, 8u, 0xFFFFFFFFu})
        require(!sma3::localized_read(0, sma3::credits_resource_base, width, 0, &value), "invalid width rejected");
    require(!sma3::localized_read(0, sma3::graphics_es_base, 1, 0, nullptr), "null output rejected");
    require(value == 0x12345678, "invalid request leaves output unchanged");
    std::printf("PASS: %llu checks; %zu text slots, %zu graphics slots, %zu stable font slots; all bank bytes, mirrors, widths, guards and language switches\n",
                checks, std::size(sma3::text_pointers), std::size(sma3::graphics_pointers),
                std::size(sma3::credits_resource_pointers));
}
