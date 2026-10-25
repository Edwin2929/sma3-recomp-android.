#include "runtime_arm.h"
#include "runtime_bus_bridge.h"
#include "gba_bus.h"
#include <cstdint>
#include <cstring>

struct DispatchEntry { uint32_t addr; uint8_t thumb; uint8_t resume; void (*fn)(void); };
extern "C" const DispatchEntry effect_tiles_kDispatchTable[];
extern "C" const unsigned effect_tiles_kDispatchTableLen;
extern "C" const DispatchEntry effect_offsets_kDispatchTable[];
extern "C" const unsigned effect_offsets_kDispatchTableLen;
thread_local uint32_t sma3_effect_delta = 0;

int sma3_effects_dispatch(uint32_t pc, int thumb) {
    if (thumb || (pc & 3u) || pc < 0x03002200 || pc >= 0x03006394) return 0;
    auto* bus = gbarecomp::active_bus();
    if (!bus || bus->rom_size() != 0x400000) return 0;
    uint32_t active;
    std::memcpy(&active, bus->iwram_ptr() + 0x7294, sizeof(active));
    if (active != 1) return 0;
    // CodeStart.asm: the original Fuzzy vertical tile-shift effect allocates
    // these ARM routines independently and publishes these live pointers.
    // Never reuse an old allocation or accept modified bytes after compaction.
    struct Variant {
        uint32_t pointer_offset, callback_offset, source, size;
        const DispatchEntry* table;
        const unsigned* count;
    };
    const Variant variants[] = {
        {0x7264, 0x736C, 0x91C, 0xD8, effect_tiles_kDispatchTable, &effect_tiles_kDispatchTableLen},
        {0x7260, 0x7370, 0xA08, 0x78, effect_offsets_kDispatchTable, &effect_offsets_kDispatchTableLen},
    };
    constexpr uint32_t canonical = 0x03004D50;
    for (const auto& variant : variants) {
        uint32_t base, callback;
        std::memcpy(&base, bus->iwram_ptr() + variant.pointer_offset, sizeof(base));
        std::memcpy(&callback, bus->iwram_ptr() + variant.callback_offset, sizeof(callback));
        if (callback != base) continue;
        if ((base & 3u) || base < 0x03002200 || base > 0x03006394 - variant.size ||
            pc < base || pc - base >= variant.size) continue;
        if (std::memcmp(bus->iwram_ptr() + base - 0x03000000,
                        bus->rom_ptr() + variant.source, variant.size)) continue;
        const uint32_t key = canonical + pc - base;
        unsigned lo = 0, hi = *variant.count;
        while (lo < hi) {
            const unsigned mid = lo + (hi - lo) / 2;
            if (variant.table[mid].addr < key) lo = mid + 1;
            else hi = mid;
        }
        if (lo == *variant.count || variant.table[lo].addr != key || variant.table[lo].thumb)
            return 0;
        const auto& entry = variant.table[lo];
        const uint32_t previous = sma3_effect_delta;
        sma3_effect_delta = base - canonical;
        g_runtime_resume_pc = entry.resume ? pc : 0;
        entry.fn();
        sma3_effect_delta = previous;
        return 1;
    }
    return 0;
}
