#include "runtime_arm.h"
#include "runtime_bus_bridge.h"
#include "gba_bus.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

int sma3_effects_dispatch(uint32_t, int);
extern thread_local uint32_t sma3_effect_delta;
static unsigned checks = 0;
static void require(bool ok, const char* why) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s pc=%08X\n", why, g_cpu.R[15]); std::exit(1); }
}
struct Variant { unsigned source, size, pointer, callback; };
constexpr Variant variants[] = {{0x91C, 0xD8, 0x7264, 0x736C}, {0xA08, 0x78, 0x7260, 0x7370}};
struct Result { ArmCpuState cpu; std::vector<uint8_t> iwram, ewram; uint64_t cycles; };

Result run(const std::vector<uint8_t>& rom, Variant variant, uint32_t base,
           unsigned scroll, bool thumb_return, unsigned pause_offset, bool native) {
    auto bus = std::make_unique<gba::GbaBus>();
    bus->set_rom(rom.data(), rom.size());
    gbarecomp::set_active_bus(bus.get());
    gbarecomp::set_active_ppu(nullptr);
    for (unsigned i = 0; i < 0x40000; ++i) bus->ewram_ptr()[i] = (i * 13 + 7) & 255;
    std::memcpy(bus->iwram_ptr() + base - 0x03000000, rom.data() + variant.source, variant.size);
    bus->write32(0x03000000 + variant.pointer, base);
    bus->write32(0x03000000 + variant.callback, base);
    bus->write32(0x03007294, 1);
    // A 64-column tilemap and independent destinations exercise both banks,
    // wrapped rows and all 31 columns processed by the original ARM routine.
    bus->write32(0x02008000, 0x02002000);
    bus->write32(0x02008004, 0x02004000);
    bus->write32(0x02008008, 0x02006000);
    bus->write32(0x0200800C, 0x02001000);
    for (unsigned i = 0; i < 64; ++i) {
        bus->ewram_ptr()[0x1000 + i] = (i * 3 + scroll) & 255;
        bus->iwram_ptr()[0x72A0 + i] = (i * 5 + scroll) & 255;
    }
    bus->write16(0x03007302, scroll * 8);
    bus->write8(0x03007304, scroll + 9);
    g_cpu = {};
    g_cpu.cpsr = 0xDF;
    for (unsigned i = 2; i < 13; ++i) g_cpu.R[i] = 0x12000000 + i;
    g_cpu.R[0] = variant.source == 0x91C ? 0x02008000 : 0x02001000;
    g_cpu.R[1] = scroll;
    g_cpu.R[13] = 0x03007E00;
    g_cpu.R[14] = 0x0203FF00 | unsigned(thumb_return);
    g_cpu.R[15] = base;
    g_runtime_cycles = 0;
    // The unit test has no PPU. Count normal instruction costs through the
    // runtime's shadow clock without advancing devices or delivering IRQs.
    g_runtime_shadow_tick = 1;
    g_runtime_shadow_cycles = 0;
    g_runtime_resume_pc = 0;
    g_runtime_break_pc = pause_offset ? base + pause_offset : 0x0203FF00;
    g_runtime_ram_dispatch_hook = [](uint32_t pc, int thumb) {
        if (pc == 0x0203FF00) { g_cpu.R[15] = pc; return 1; }
        return sma3_effects_dispatch(pc, thumb);
    };
    unsigned dispatches = 0;
    bool paused = !pause_offset;
    while (g_cpu.R[15] != 0x0203FF00 && dispatches++ < 100000) {
        if (native) require(sma3_effects_dispatch(g_cpu.R[15], (g_cpu.cpsr >> 5) & 1) == 1, "native dispatch");
        else runtime_force_interp_step();
        if (pause_offset && g_cpu.R[15] == base + pause_offset) {
            paused = true;
            g_runtime_break_pc = 0x0203FF00;
        }
    }
    require(g_cpu.R[15] == 0x0203FF00, "normal return, no reset");
    require(paused, "requested instruction yield reached");
    require(sma3_effect_delta == 0, "relocation restored after dispatch");
    require(g_runtime_shadow_cycles > 0, "normal cycle accounting is active");
    Result result{g_cpu, {bus->iwram_ptr(), bus->iwram_ptr() + 0x8000},
                  {bus->ewram_ptr(), bus->ewram_ptr() + 0x40000}, g_runtime_shadow_cycles};
    g_runtime_shadow_tick = 0;
    if (native) {
        require(!sma3_effects_dispatch(base, 1), "reject Thumb mode");
        require(!sma3_effects_dispatch(base + 2, 0), "reject unaligned ARM PC");
        require(!sma3_effects_dispatch(base + variant.size, 0), "reject PC after allocation");
        bus->iwram_ptr()[base - 0x03000000 + variant.size - 1] ^= 1;
        require(!sma3_effects_dispatch(base, 0), "reject changed final code byte");
        bus->iwram_ptr()[base - 0x03000000 + variant.size - 1] ^= 1;
        bus->write32(0x03007294, 0);
        require(!sma3_effects_dispatch(base, 0), "reject completed effect with stale allocation bytes");
        bus->write32(0x03007294, 1);
        bus->write32(0x03000000 + variant.callback, base + 4);
        require(!sma3_effects_dispatch(base, 0), "reject stale published callback");
        bus->write32(0x03000000 + variant.callback, base);
        bus->write32(0x03000000 + variant.pointer, base + 4);
        require(!sma3_effects_dispatch(base, 0), "reject stale allocation pointer");
        bus->write32(0x03000000 + variant.pointer, 0x03006390);
        bus->write32(0x03000000 + variant.callback, 0x03006390);
        require(!sma3_effects_dispatch(0x03006390, 0), "reject allocation outside arena");
        bus->write32(0x03000000 + variant.pointer, base | 1);
        bus->write32(0x03000000 + variant.callback, base | 1);
        require(!sma3_effects_dispatch(base, 0), "reject odd published pointer");
        bus->write32(0x03000000 + variant.pointer, 0);
        bus->write32(0x03000000 + variant.callback, 0);
        require(!sma3_effects_dispatch(base, 0), "reject released allocation");
    }
    gbarecomp::set_active_bus(nullptr);
    return result;
}

int main(int argc, char** argv) {
    require(argc == 2, "verified ROM argument");
    std::ifstream stream(argv[1], std::ios::binary);
    const std::vector<uint8_t> rom{std::istreambuf_iterator<char>(stream), {}};
    require(rom.size() == 0x400000, "ROM size");
    unsigned scenarios = 0;
    for (const auto variant : variants)
        for (uint32_t base : {0x03002400u, 0x03004D50u, 0x03005400u})
            for (unsigned scroll : {0u, 31u, 63u})
                for (bool thumb_return : {false, true})
                    for (unsigned pause_offset : {0u, 4u, 0x1Cu, 0x50u}) {
                        const auto a = run(rom, variant, base, scroll, thumb_return, pause_offset, true);
                        const auto b = run(rom, variant, base, scroll, thumb_return, pause_offset, false);
                        require(!std::memcmp(a.cpu.R, b.cpu.R, sizeof(a.cpu.R)), "registers equal ARM interpreter");
                        require(a.cpu.cpsr == b.cpu.cpsr, "CPSR equal ARM interpreter");
                        require(a.iwram == b.iwram && a.ewram == b.ewram, "memory equal ARM interpreter");
                        require(a.cycles == b.cycles, "cycles equal ARM interpreter");
                        ++scenarios;
                    }
    std::printf("PASS: %u ARM effect scenarios; %u checks; native/interpreter registers, flags, memory and cycles match\n", scenarios, checks);
}
