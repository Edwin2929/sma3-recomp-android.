#include "runtime_arm.h"
#include "runtime_bus_bridge.h"
#include "gba_bus.h"
#include <cstdint>
#include <cstring>
struct DispatchEntry { uint32_t addr; uint8_t thumb; uint8_t resume; void (*fn)(void); };
extern "C" const DispatchEntry morph_kDispatchTable[];
extern "C" const unsigned morph_kDispatchTableLen;
thread_local uint32_t sma3_morph_delta = 0;
int sma3_morph_dispatch(uint32_t pc, int thumb) {
    if (!thumb || pc < 0x03002200 || pc >= 0x03006394) return 0;
    auto* bus = gbarecomp::active_bus();
    if (!bus || bus->rom_size() != 0x400000) return 0;
    // Morph bubble allocation and pointer publication: ROM 080DA1A2..080DA1CE.
    uint32_t pointer;
    std::memcpy(&pointer, bus->iwram_ptr() + 0x6394, sizeof(pointer));
    const uint32_t base = pointer & ~1u;
    constexpr uint32_t size = 0x4B4, canonical = 0x03004D50;
    if (!(pointer & 1) || (base & 3) || base < 0x03002200 ||
        base > 0x03006394 - size || pc < base || pc - base >= size) return 0;
    if (std::memcmp(bus->iwram_ptr() + base - 0x03000000,
                    bus->rom_ptr() + 0x40CE0, size)) return 0;
    const uint32_t key = canonical + pc - base;
    unsigned lo = 0, hi = morph_kDispatchTableLen;
    while (lo < hi) {
        unsigned mid = lo + (hi-lo)/2;
        if (morph_kDispatchTable[mid].addr < key ||
            (morph_kDispatchTable[mid].addr == key && !morph_kDispatchTable[mid].thumb)) lo = mid + 1;
        else hi = mid;
    }
    if (lo == morph_kDispatchTableLen || morph_kDispatchTable[lo].addr != key ||
        !morph_kDispatchTable[lo].thumb) return 0;
    const auto& entry = morph_kDispatchTable[lo];
    const uint32_t previous = sma3_morph_delta;
    sma3_morph_delta = base - canonical;
    g_runtime_resume_pc = entry.resume ? pc : 0;
    entry.fn();
    sma3_morph_delta = previous;
    return 1;
}
