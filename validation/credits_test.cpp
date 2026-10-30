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

int sma3_credits_dispatch(uint32_t, int);
extern thread_local uint32_t sma3_credits_delta;
static unsigned checks = 0;
static void require(bool ok, const char* why) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s pc=%08X\n", why, g_cpu.R[15]); std::exit(1); }
}
struct Packet { unsigned x, y, scale, palette; bool visible; };
struct Result { ArmCpuState cpu; std::vector<uint8_t> iwram, ewram; uint64_t cycles; };

Result run(const std::vector<uint8_t>& rom, uint32_t base, Packet packet,
           bool thumb_return, unsigned pause_offset, bool native) {
    auto bus = std::make_unique<gba::GbaBus>();
    bus->set_rom(rom.data(), rom.size());
    gbarecomp::set_active_bus(bus.get());
    gbarecomp::set_active_ppu(nullptr);
    std::memset(bus->ewram_ptr(), 0x55, 0x10000);
    for (unsigned i = 0; i < 0x1000; ++i) bus->ewram_ptr()[0x21000 + i] = (i * 13 + i / 7) & 15;
    std::memcpy(bus->iwram_ptr() + base - 0x03000000, rom.data() + 0x33A2C, 0x17C);
    bus->write32(0x03006390, base | 1);
    // Credits graphic descriptor: source tiles, width and height. The original
    // renderer scales/clips into 02000000; a fifth stack argument selects color.
    bus->write32(0x02020000, 0x02021000);
    bus->write16(0x02020004, 8);
    bus->write16(0x02020006, 8);
    g_cpu = {};
    g_cpu.cpsr = 0xFF;
    for (unsigned i = 4; i < 13; ++i) g_cpu.R[i] = 0x12000000 + i;
    g_cpu.R[0] = 0x02020000;
    g_cpu.R[1] = packet.x;
    g_cpu.R[2] = packet.y;
    g_cpu.R[3] = packet.scale;
    g_cpu.R[13] = 0x03007E00;
    g_cpu.R[14] = 0x0203FF00 | unsigned(thumb_return);
    g_cpu.R[15] = base;
    bus->write32(g_cpu.R[13], packet.palette);
    g_runtime_cycles = 0;
    g_runtime_shadow_tick = 1;
    g_runtime_shadow_cycles = 0;
    g_runtime_resume_pc = 0;
    g_runtime_break_pc = pause_offset ? base + pause_offset : 0x0203FF00;
    g_runtime_ram_dispatch_hook = [](uint32_t pc, int thumb) {
        if (pc == 0x0203FF00) { g_cpu.R[15] = pc; return 1; }
        return sma3_credits_dispatch(pc, thumb);
    };
    unsigned dispatches = 0;
    bool paused = !pause_offset;
    while (g_cpu.R[15] != 0x0203FF00 && dispatches++ < 100000) {
        if (native) require(sma3_credits_dispatch(g_cpu.R[15], (g_cpu.cpsr >> 5) & 1) == 1, "native dispatch");
        else runtime_force_interp_step();
        if (pause_offset && g_cpu.R[15] == base + pause_offset) {
            paused = true;
            g_runtime_break_pc = 0x0203FF00;
        }
    }
    require(g_cpu.R[15] == 0x0203FF00, "normal credits return without reset");
    require(paused, "requested instruction yield reached");
    require(sma3_credits_delta == 0, "relocation restored after dispatch");
    require(g_runtime_shadow_cycles > 0, "normal cycle accounting active");
    bool rendered = false;
    for (unsigned i = 0; i < 0x10000; ++i) rendered |= bus->ewram_ptr()[i] != 0x55;
    require(rendered == packet.visible, "visible and offscreen packets render as expected");
    Result result{g_cpu, {bus->iwram_ptr(), bus->iwram_ptr() + 0x8000},
                  {bus->ewram_ptr(), bus->ewram_ptr() + 0x40000}, g_runtime_shadow_cycles};
    g_runtime_shadow_tick = 0;
    if (native) {
        require(!sma3_credits_dispatch(base, 0), "reject ARM mode");
        require(!sma3_credits_dispatch(base + 1, 1), "reject odd instruction PC");
        require(!sma3_credits_dispatch(base + 0x17C, 1), "reject PC after allocation");
        bus->iwram_ptr()[base - 0x03000000 + 0x17B] ^= 1;
        require(!sma3_credits_dispatch(base, 1), "reject changed final literal byte");
        bus->iwram_ptr()[base - 0x03000000 + 0x17B] ^= 1;
        bus->write32(0x03006390, base);
        require(!sma3_credits_dispatch(base, 1), "reject non-Thumb published pointer");
        bus->write32(0x03006390, (base + 4) | 1);
        require(!sma3_credits_dispatch(base, 1), "reject stale allocation pointer");
        bus->write32(0x03006390, 0x03006301);
        require(!sma3_credits_dispatch(0x03006300, 1), "reject allocation outside arena");
        bus->write32(0x03006390, base | 3);
        require(!sma3_credits_dispatch(base + 2, 1), "reject unaligned allocation");
        bus->write32(0x03006390, 0);
        require(!sma3_credits_dispatch(base, 1), "reject released allocation");
    }
    gbarecomp::set_active_bus(nullptr);
    return result;
}

int main(int argc, char** argv) {
    require(argc == 2, "verified ROM argument");
    std::ifstream stream(argv[1], std::ios::binary);
    const std::vector<uint8_t> rom{std::istreambuf_iterator<char>(stream), {}};
    require(rom.size() == 0x400000, "ROM size");
    constexpr Packet packets[] = {
        {64, 40, 0x80, 0x40, true}, {1, 2, 0x80, 0x80, true},
        {128, 64, 0x100, 0x20, true}, {400, 250, 0x80, 0xF0, false},
    };
    unsigned scenarios = 0;
    for (uint32_t base : {0x03002B64u, 0x03004D50u, 0x03005400u})
        for (const auto packet : packets)
            for (bool thumb_return : {false, true})
                for (unsigned pause_offset : {0u, 0xAu, 0x2Eu, 0x164u}) {
                    const auto a = run(rom, base, packet, thumb_return, pause_offset, true);
                    const auto b = run(rom, base, packet, thumb_return, pause_offset, false);
                    require(!std::memcmp(a.cpu.R, b.cpu.R, sizeof(a.cpu.R)), "registers equal Thumb interpreter");
                    require(a.cpu.cpsr == b.cpu.cpsr, "CPSR equal Thumb interpreter");
                    require(a.iwram == b.iwram && a.ewram == b.ewram, "memory equal Thumb interpreter");
                    require(a.cycles == b.cycles, "cycles equal Thumb interpreter");
                    ++scenarios;
                }
    std::printf("PASS: %u credits scenarios; %u checks; native/interpreter registers, flags, memory and cycles match\n", scenarios, checks);
}
