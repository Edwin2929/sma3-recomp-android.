// Focused component qualification of the ORIGINAL recompiled credit renderer.
// Controlled descriptors invoke CreditsText_ProcessMsg through static dispatch.
// An offline Thumb interpreter is used only as a reference in this test binary.
// This is not a natural ending sequence, PPU frame, or Android GPU test.
#include "runtime_arm.h"
#include "runtime_bus_bridge.h"
#include "gba_bus.h"
#include "sha1.h"
#include "self_heal.h"
#include "localization.h"
#include "localization_data.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <vector>

namespace {
constexpr uint32_t kRenderer = 0x08033E94;
constexpr uint32_t kReturn = 0x0203FF00;
constexpr uint32_t kOwner = 0x02020000;
constexpr uint32_t kInput = 0x02021000;
constexpr uint32_t kPixels = 0x02011000;
constexpr unsigned kBufferBytes = 0x800;
unsigned checks = 0, native_runs = 0, reference_runs = 0;
uint64_t reference_instructions = 0;
std::string native_coverage;
struct Reads { uint64_t font_literal = 0, width_literal = 0, font = 0, widths = 0, entries = 0; } reads;

void require(bool ok, const char* message) {
    ++checks;
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s pc=%08x\n", message, g_cpu.R[15]);
        std::exit(1);
    }
}

int observed_read(uint32_t pc, uint32_t address, uint32_t width,
                  uint32_t original, uint32_t* output) {
    int changed = sma3::localized_read(pc, address, width, original, output);
    if (changed) {
        if (address == 0x08033D80) { ++reads.font_literal; require(*output == sma3::credits_resource_base, "native font literal target"); }
        if (address == 0x08033E58) { ++reads.width_literal; require(*output == sma3::credits_resource_base + 1536, "native width literal target"); }
        if (address >= sma3::credits_resource_base && address < sma3::credits_resource_base + 1536) ++reads.font;
        if (address >= sma3::credits_resource_base + 1536 && address < sma3::credits_resource_base + 1664) ++reads.widths;
    }
    return changed;
}

struct Result {
    ArmCpuState cpu;
    std::vector<uint8_t> ewram, iwram;
    std::array<uint8_t, kBufferBytes> pixels;
    uint64_t cycles;
    Reads observed;
};

Result run(const std::vector<uint8_t>& rom, uint32_t pointer_slot,
           unsigned font_mode, int language, bool localized, bool native,
           bool change_after_latch = false, const std::vector<uint8_t>& input = {}) {
    auto bus = std::make_unique<gba::GbaBus>();
    bus->set_rom(rom.data(), rom.size());
    gbarecomp::set_active_bus(bus.get());
    gbarecomp::set_active_ppu(nullptr);
    std::memset(bus->ewram_ptr(), 0xA5, 0x40000);
    std::memset(bus->iwram_ptr(), 0xA5, 0x8000);
    std::memset(bus->ewram_ptr() + kPixels - 0x02000000, 0x44, kBufferBytes);
    std::memset(bus->ewram_ptr() + kOwner - 0x02000000, 0, 0xA00);
    if (!input.empty()) std::copy(input.begin(), input.end(), bus->ewram_ptr() + kInput - 0x02000000);
    g_cpu = {};
    g_cpu.cpsr = 0xFF;  // Thumb, system mode, IRQ/FIQ masked.
    g_cpu.R[0] = kOwner;
    for (unsigned i = 4; i < 13; ++i) g_cpu.R[i] = 0x12000000 + i;
    g_cpu.R[13] = 0x03007E00;
    g_cpu.R[14] = kReturn | 1;
    g_cpu.R[15] = kRenderer;
    g_force_interp = 0;
    g_runtime_force_interp_hook = nullptr;
    g_runtime_bus_read_override = localized ? observed_read : nullptr;
    g_runtime_fn_entry_hook = native ? +[](uint32_t pc) {
        ++reads.entries;
        if (reads.entries > 200000) {
            std::fprintf(stderr, "FAIL: native component entry budget pc=%08x\n", pc);
            std::exit(1);
        }
    } : nullptr;
    sma3::language.store(language);
    reads = {};
    // The controlled caller obtains the same verified pointer through the
    // actual runtime bridge, then latches it in the guest descriptor.
    const uint32_t packet = input.empty() ? bus_read_u32(pointer_slot) : kInput;
    bus->write32(kOwner, packet);
    bus->write16(kOwner + 0x950, font_mode);
    if (change_after_latch) sma3::language.store(0);
    const std::vector<uint8_t> before_ewram(bus->ewram_ptr(), bus->ewram_ptr() + 0x40000);
    const std::vector<uint8_t> before_iwram(bus->iwram_ptr(), bus->iwram_ptr() + 0x8000);
    runtime_call_stack_restore(nullptr, 0);
    g_runtime_cycles = 0;
    g_runtime_shadow_tick = 1;
    g_runtime_shadow_cycles = 0;
    g_runtime_resume_pc = 0;
    g_runtime_break_pc = kReturn;
    g_runtime_ram_dispatch_hook = [](uint32_t pc, int) {
        if (pc != kReturn) return 0;
        g_cpu.R[15] = pc;
        return 1;
    };
    gbarecomp::self_heal_reset();
    uint64_t steps = 0;
    while (g_cpu.R[15] != kReturn && steps++ < 2000000) {
        if (native) {
            require(runtime_has_static_entry(g_cpu.R[15], (g_cpu.cpsr >> 5) & 1), "static renderer entry");
            runtime_dispatch(g_cpu.R[15]);
        } else runtime_force_interp_step();
    }
    require(g_cpu.R[15] == kReturn, "renderer returns normally");
    require(g_cpu.R[13] == 0x03007E00, "stack restored");
    require(runtime_call_stack_depth() == 0, "host return stack balanced");
    require(!gbarecomp::self_heal_any_misses() && !gbarecomp::self_heal_distinct_misses()
            && !gbarecomp::self_heal_interpreted_insns(), "no missing native dispatch or interpreter bridge");
    for (const auto bounds : {std::pair{0u, 0x11000u}, std::pair{0x11800u, 0x20000u},
                              std::pair{0x20A00u, 0x40000u}})
        require(!std::memcmp(bus->ewram_ptr() + bounds.first, before_ewram.data() + bounds.first,
                             bounds.second - bounds.first), "EWRAM buffer canary");
    for (const auto bounds : {std::pair{0u, 0x7C00u}, std::pair{0x7E00u, 0x8000u}})
        require(!std::memcmp(bus->iwram_ptr() + bounds.first, before_iwram.data() + bounds.first,
                             bounds.second - bounds.first), "IWRAM stack canary");
    Result result{g_cpu, {bus->ewram_ptr(), bus->ewram_ptr() + 0x40000},
                  {bus->iwram_ptr(), bus->iwram_ptr() + 0x8000}, {}, g_runtime_shadow_cycles, reads};
    std::copy_n(bus->ewram_ptr() + 0x11000, kBufferBytes, result.pixels.begin());
    if (native) {
        ++native_runs;
        require(reads.entries != 0, "generated functions executed");
        native_coverage = gbarecomp::self_heal_misses_json();
    }
    else { ++reference_runs; reference_instructions += steps; }
    g_runtime_shadow_tick = 0;
    g_runtime_bus_read_override = nullptr;
    g_runtime_fn_entry_hook = nullptr;
    g_runtime_ram_dispatch_hook = nullptr;
    gbarecomp::set_active_bus(nullptr);
    return result;
}

void same_reference(const Result& native, const Result& reference) {
    require(!std::memcmp(native.cpu.R, reference.cpu.R, sizeof(native.cpu.R)), "native/reference registers");
    require(native.cpu.cpsr == reference.cpu.cpsr, "native/reference flags");
    require(native.ewram == reference.ewram && native.iwram == reference.iwram, "native/reference complete RAM");
    require(native.cycles == reference.cycles, "native/reference cycles");
}

std::vector<uint8_t> one_glyph(uint8_t code, uint8_t x, uint8_t row) {
    return {255,1,0,255,2,row,255,3,x,code,255,1,1,255,2,8,255,3,120,255,255};
}

std::array<uint8_t, kBufferBytes> expected_glyph(uint8_t code, unsigned x, unsigned line) {
    std::array<uint8_t, kBufferBytes> result;
    result.fill(0x44);
    const uint8_t* glyph = sma3::credits_resources + (code - 0x80) * 12;
    // Independent tile-plane layout of the renderer's 0x800-byte strip:
    // the two credit lines occupy distinct palette bits of the same tiles.
    for (unsigned y = 0; y < 12; ++y) for (unsigned dx = 0; dx < 8; ++dx) {
        if (!(glyph[y] & (0x80 >> dx))) continue;
        const unsigned px = x + dx;
        const unsigned offset = (y / 8) * 1024 + (y % 8) * 4 + (px / 8) * 32 + (px % 8) / 2;
        require(offset < result.size(), "expected tile write bounded");
        result[offset] |= uint8_t((line ? 2 : 1) << ((px & 1) ? 4 : 0));
    }
    return result;
}
} // namespace

int main(int argc, char** argv) {
    require(argc == 3, "ROM and coverage-output arguments");
    setenv("GBARECOMP_STRICT_STATIC", "1", 1);
    std::ifstream stream(argv[1], std::ios::binary);
    const std::vector<uint8_t> rom{std::istreambuf_iterator<char>(stream), {}};
    require(gba::sha1(rom.data(), rom.size()).hex() == "7352d2bd064d9ebaec579e264228aa21c7345b80", "verified ROM hash");
    require(runtime_has_static_entry(kRenderer, 1), "original renderer is in static archive");
    unsigned english_packets = 0, translated_packets = 0, accent_cases = 0;
    uint64_t font_reads = 0, width_reads = 0;
    for (unsigned index = 0; index < 39; ++index) {
        uint32_t slot = 0x0816DB0C + index * 4;
        if (index == 36) slot = 0x08034910;
        if (index == 37) slot = 0x0803502C;
        if (index == 38) slot = 0x08035160;
        const unsigned mode = index >= 36;
        const Result original = run(rom, slot, mode, 0, false, true);
        const Result extended = run(rom, slot, mode, 0, true, true);
        require(original.pixels == extended.pixels, "English native credit pixels unchanged");
        ++english_packets;
    }
    for (const auto& pointer : sma3::text_pointers) {
        if (pointer.original < 0x0816CEE8 || pointer.original >= 0x0816D489) continue;
        const unsigned mode = pointer.original >= 0x0816D3F6;
        for (int language : {1, 2}) {
            const Result native = run(rom, pointer.address, mode, language, true, true);
            const Result reference = run(rom, pointer.address, mode, language, true, false);
            same_reference(native, reference);
            const Result switched = run(rom, pointer.address, mode, language, true, true, true);
            require(native.pixels == switched.pixels && native.ewram == switched.ewram,
                    "latched native packet survives language switch");
            if (!mode) require(native.observed.font_literal && native.observed.width_literal
                               && native.observed.font && native.observed.widths, "native renderer reads virtual font and widths");
            font_reads += native.observed.font;
            width_reads += native.observed.widths;
            ++translated_packets;
        }
    }
    for (uint8_t code = 0x80; code <= 0x86; ++code) for (uint8_t x : {uint8_t(20), uint8_t(21)})
        for (uint8_t row : {uint8_t(0), uint8_t(8)}) {
            const auto input = one_glyph(code, x, row);
            const Result native = run(rom, 0, 0, 1, true, true, false, input);
            const Result reference = run(rom, 0, 0, 1, true, false, false, input);
            same_reference(native, reference);
            require(native.pixels == expected_glyph(code, x, row != 0), "actual native accent pixels and tile planes");
            require(native.observed.font_literal && native.observed.width_literal
                    && native.observed.font == 12 && native.observed.widths == 1, "single glyph uses exact virtual font cell");
            ++accent_cases;
        }
    require(english_packets == 39 && translated_packets == 40 && accent_cases == 28, "complete component matrix");
    std::ofstream coverage(argv[2]);
    coverage << native_coverage << '\n';
    require(bool(coverage), "write native dispatch coverage");
    std::printf("{\"result\":\"PASS\",\"scope\":\"controlled original recompiled credits renderer, not natural ending or Android\","
        "\"english_packets_identical\":%u,\"translated_packets_native_reference\":%u,\"accent_cases\":%u,"
        "\"native_executions\":%u,\"offline_reference_executions\":%u,\"offline_reference_instructions\":%llu,"
        "\"virtual_font_reads_in_translated_packets\":%llu,\"virtual_width_reads_in_translated_packets\":%llu,\"checks\":%u}\n",
        english_packets, translated_packets, accent_cases, native_runs, reference_runs,
        (unsigned long long)reference_instructions, (unsigned long long)font_reads,
        (unsigned long long)width_reads, checks);
}
