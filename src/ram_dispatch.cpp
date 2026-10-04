#include "runtime_arm.h"
#include "runtime_bus_bridge.h"
#include "gba_bus.h"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
struct DispatchEntry { uint32_t addr; uint8_t thumb; uint8_t resume; void (*fn)(void); };
extern "C" const DispatchEntry sma3ram_kDispatchTable[];
extern "C" const unsigned sma3ram_kDispatchTableLen;
int sma3_overlay_dispatch(uint32_t pc, int thumb);
int sma3_ram_dispatch(uint32_t pc, int thumb) {
    if (sma3_overlay_dispatch(pc, thumb)) return 1;
    struct Copy { uint32_t ram, rom, size; };
    constexpr Copy copies[] = {
        {0x03004110, 0x08033224, 0x3A4}, {0x03003DF8, 0x080335C8, 0x258},
        {0x03004054, 0x0802F2A4, 0xB8}, {0x03002474, 0x08000288, 0x100},
    };
    auto* bus = gbarecomp::active_bus();
    if (!bus || bus->rom_size() != 0x400000) return 0;
    bool verified = false;
    for (const auto& copy : copies) {
        if (pc < copy.ram || pc - copy.ram >= copy.size) continue;
        verified = std::memcmp(bus->iwram_ptr() + copy.ram - 0x03000000,
            bus->rom_ptr() + copy.rom - 0x08000000, copy.size) == 0;
        break;
    }
    if (!verified) {
        if (std::getenv("SMA3_RAM_DIAGNOSTICS") && !runtime_has_static_entry(pc, thumb)
            && pc >= 0x03000000 && pc <= 0x03007FC0) {
            const auto* bytes = bus->iwram_ptr() + pc - 0x03000000;
            for (uint32_t i = 0; i <= 0x400000 - 64; i += 2) {
                if (std::memcmp(bytes, bus->rom_ptr() + i, 64)) continue;
                uint32_t length = 64;
                while (pc - 0x03000000 + length < 0x8000 && i + length < 0x400000
                       && bytes[length] == bus->rom_ptr()[i + length]) ++length;
                std::fprintf(stderr, "SMA3 RAM mapping: pc=%08X thumb=%d source=%08X matching_bytes=%X\n",
                    pc, thumb, 0x08000000 + i, length);
                break;
            }
        }
        return 0;
    }
    unsigned lo = 0, hi = sma3ram_kDispatchTableLen;
    while (lo < hi) {
        unsigned mid = lo + (hi - lo) / 2;
        const auto& entry = sma3ram_kDispatchTable[mid];
        if (entry.addr < pc || (entry.addr == pc && entry.thumb < thumb)) lo = mid + 1;
        else hi = mid;
    }
    if (lo == sma3ram_kDispatchTableLen) return 0;
    const auto& entry = sma3ram_kDispatchTable[lo];
    if (entry.addr != pc || entry.thumb != thumb) return 0;
    g_runtime_resume_pc = entry.resume ? pc : 0;
    entry.fn();
    return 1;
}
