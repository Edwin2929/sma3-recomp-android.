#include "runtime_arm.h"
#include "runtime_bus_bridge.h"
#include "gba_bus.h"
#include <cstdint>
#include <cstring>

struct DispatchEntry { uint32_t addr; uint8_t thumb; uint8_t resume; void (*fn)(void); };
extern "C" const DispatchEntry credits_kDispatchTable[];
extern "C" const unsigned credits_kDispatchTableLen;
thread_local uint32_t sma3_credits_delta = 0;

int sma3_credits_dispatch(uint32_t pc, int thumb) {
    if (!thumb || (pc & 1) || pc < 0x03002200 || pc >= 0x03006390) return 0;
    auto* bus = gbarecomp::active_bus();
    if (!bus || bus->rom_size() != 0x400000) return 0;
    // CreditsInit copies this exact routine and publishes base|1 at 03006390.
    // A pointer or PC alone cannot authorize executing stale/other RAM bytes.
    uint32_t pointer;
    std::memcpy(&pointer, bus->iwram_ptr() + 0x6390, sizeof(pointer));
    const uint32_t base = pointer & ~1u;
    constexpr uint32_t size = 0x17C, canonical = 0x03004D50;
    if (!(pointer & 1) || (base & 3) || base < 0x03002200 ||
        base > 0x03006390 - size || pc < base || pc - base >= size) return 0;
    if (std::memcmp(bus->iwram_ptr() + base - 0x03000000,
                    bus->rom_ptr() + 0x33A2C, size)) return 0;
    const uint32_t key = canonical + pc - base;
    unsigned lo = 0, hi = credits_kDispatchTableLen;
    while (lo < hi) {
        const unsigned mid = lo + (hi - lo) / 2;
        if (credits_kDispatchTable[mid].addr < key ||
            (credits_kDispatchTable[mid].addr == key && !credits_kDispatchTable[mid].thumb)) lo = mid + 1;
        else hi = mid;
    }
    if (lo == credits_kDispatchTableLen || credits_kDispatchTable[lo].addr != key ||
        !credits_kDispatchTable[lo].thumb) return 0;
    const auto& entry = credits_kDispatchTable[lo];
    const uint32_t previous = sma3_credits_delta;
    sma3_credits_delta = base - canonical;
    g_runtime_resume_pc = entry.resume ? pc : 0;
    entry.fn();
    sma3_credits_delta = previous;
    return 1;
}
