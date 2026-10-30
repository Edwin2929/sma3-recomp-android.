#include "localization.h"
#include "localization_data.h"
#include <iterator>
namespace sma3 {

namespace {
template <std::size_t Size>
bool read_bank(uint32_t address, uint32_t base, const uint8_t (&bytes)[Size],
               uint32_t width, uint32_t* output) {
    if (address < base || address - base >= Size) return false;
    const uint32_t offset = address - base;
    uint32_t value = 0;
    for (uint32_t i = 0; i < width; ++i)
        value |= uint32_t(offset + i < Size ? bytes[offset + i] : 0xFFu) << (8 * i);
    *output = value;
    return true;
}

template <class Pointer, std::size_t Size>
bool translated_pointer(const Pointer (&pointers)[Size], uint32_t address,
                        uint32_t original, int lang, uint32_t* output) {
    unsigned lo = 0, hi = Size;
    while (lo < hi) {
        const unsigned mid = lo + (hi - lo) / 2;
        if (pointers[mid].address < address) lo = mid + 1;
        else hi = mid;
    }
    if (lo == Size || pointers[lo].address != address ||
        pointers[lo].original != original) return false;
    *output = lang == 1 ? pointers[lo].spanish : pointers[lo].portuguese;
    return true;
}

static_assert(graphics_es_base + sizeof(graphics_es) <= graphics_pt_base);
static_assert(graphics_pt_base + sizeof(graphics_pt) <= credits_resource_base);
static_assert(credits_resource_base + sizeof(credits_resources) <= 0x09E00000u);
static_assert(0x09E00000u + sizeof(spanish_text) <= 0x09F00000u);
static_assert(0x09F00000u + sizeof(portuguese_text) <= 0x0A000000u);
} // namespace

int localized_read(uint32_t, uint32_t address, uint32_t width, uint32_t original,uint32_t* output) {
    if(!output || (width!=1 && width!=2 && width!=4)) return 0;
    // ROM wait-state mirrors map to the same read-only localization bank.
    if(address<0x08000000u || address>=0x0E000000u) return 0;
    address=0x08000000u+(address&0x01FFFFFFu);
    // Resolved packets and graphics stay readable after a language change or
    // savestate load. Existing text bank offsets are preserved by the generator.
    if (read_bank(address, 0x09E00000u, spanish_text, width, output) ||
        read_bank(address, 0x09F00000u, portuguese_text, width, output) ||
        read_bank(address, graphics_es_base, graphics_es, width, output) ||
        read_bank(address, graphics_pt_base, graphics_pt, width, output) ||
        read_bank(address, credits_resource_base, credits_resources, width, output) ||
        // Reserved main-font glyphs 60..63 remain available to open PT packets.
        read_bank(address, 0x082F684Cu, portuguese_glyphs, width, output)) return 1;
    if (width != 4) return 0;
    // Only the credits renderer's two verified font literals use the extended
    // copy. English-used glyphs/widths are byte-identical; the original ROM
    // font addresses are never overwritten. Stable selection also lets an
    // already open translated credit retain its accented glyphs.
    for (const auto& pointer : credits_resource_pointers) {
        if (pointer.address == address && pointer.original == original) {
            *output = pointer.target;
            return 1;
        }
    }
    const int lang=language.load(std::memory_order_relaxed);
    if (!lang) return 0;
    return translated_pointer(text_pointers, address, original, lang, output) ||
           translated_pointer(graphics_pointers, address, original, lang, output);
}
}
