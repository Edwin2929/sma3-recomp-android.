// Differential test of the production PPU timing code against the previous
// dot-by-dot implementation. No game data, SDL, Android, or ROM is required.
//
/* From the SMA3 repository, with the engine at ../gbarecomp:
   g++ -std=c++20 -O2 -ffunction-sections -fdata-sections \
     -I../gbarecomp/src/gba -I../gbarecomp/src/debug \
     validation/ppu_tick_test.cpp ../gbarecomp/src/gba/gba_ppu.cpp \
     -Wl,--gc-sections -o /tmp/ppu-tick-test
   /tmp/ppu-tick-test
   Optional CPU-only timing comparison: /tmp/ppu-tick-test --benchmark
*/
//
// Private exposure is confined to this harness so every legal timing phase
// can be checked without constructing or loading millions of framebuffers.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string_view>
#include <vector>
#define private public
#include "gba_ppu.h"
#undef private

using TickEvents = gba::GbaPpu::TickEvents;

struct Timing {
    static constexpr uint32_t kCyclesPerDot = gba::GbaPpu::kCyclesPerDot;
    static constexpr uint32_t kDotsVisible = gba::GbaPpu::kDotsVisible;
    static constexpr uint32_t kDotsPerScanline = gba::GbaPpu::kDotsPerScanline;
    static constexpr uint32_t kLinesVisible = gba::GbaPpu::kLinesVisible;
    static constexpr uint32_t kLinesTotal = gba::GbaPpu::kLinesTotal;
    uint32_t scanline_{}, dot_in_scanline_{}, cycle_in_dot_{};
    uint16_t vcount_{};
    uint64_t frame_count_{};

    uint32_t cycles_until_next_event() const {
        const uint32_t target_dot = dot_in_scanline_ < kDotsVisible
            ? kDotsVisible : kDotsPerScanline;
        uint32_t cycles = (target_dot - dot_in_scanline_) * kCyclesPerDot;
        if (cycles > cycle_in_dot_) cycles -= cycle_in_dot_;
        else cycles = 1;
        return cycles ? cycles : 1;
    }
    friend bool operator==(const Timing&, const Timing&) = default;
};

struct Reference : Timing {
    TickEvents tick(uint32_t cycles, uint16_t vcount_compare);
};

// Keep this historical algorithm independent from the optimized production
// function. It is the previous function body, including uint32 accumulation.
TickEvents Reference::tick(uint32_t cycles, uint16_t vcount_compare) {
    TickEvents ev{};
    cycle_in_dot_ += cycles;
    while (cycle_in_dot_ >= kCyclesPerDot) {
        cycle_in_dot_ -= kCyclesPerDot;
        bool was_visible = (dot_in_scanline_ < kDotsVisible);
        ++dot_in_scanline_;
        if (was_visible && dot_in_scanline_ == kDotsVisible) {
            ev.hblank_started = true;
        }
        if (dot_in_scanline_ >= kDotsPerScanline) {
            dot_in_scanline_ = 0;
            bool was_in_visible_window = (scanline_ < kLinesVisible);
            ++scanline_;
            if (scanline_ >= kLinesTotal) {
                scanline_ = 0;
                ++frame_count_;
                ev.frame_completed = true;
            }
            vcount_ = static_cast<uint16_t>(scanline_);
            if (was_in_visible_window && scanline_ == kLinesVisible) {
                ev.vblank_started = true;
            }
            if (vcount_ == vcount_compare) {
                ev.vcount_matched = true;
            }
        }
    }
    return ev;
}

static Timing timing(const gba::GbaPpu& p) {
    return {p.scanline_, p.dot_in_scanline_, p.cycle_in_dot_, p.vcount_, p.frame_count_};
}
static void set_timing(gba::GbaPpu& p, const Timing& t) {
    p.scanline_ = t.scanline_;
    p.dot_in_scanline_ = t.dot_in_scanline_;
    p.cycle_in_dot_ = t.cycle_in_dot_;
    p.vcount_ = t.vcount_;
    p.frame_count_ = t.frame_count_;
}
static bool same_events(TickEvents a, TickEvents b) {
    return a.vblank_started == b.vblank_started &&
        a.hblank_started == b.hblank_started &&
        a.vcount_matched == b.vcount_matched &&
        a.frame_completed == b.frame_completed;
}

static uint64_t checks;
static void verify(gba::GbaPpu& actual, const Timing& initial,
                   uint32_t cycles, uint16_t compare) {
    Reference reference;
    static_cast<Timing&>(reference) = initial;
    set_timing(actual, initial);
    const auto expected_events = reference.tick(cycles, compare);
    const auto actual_events = actual.tick(cycles, compare);
    ++checks;
    if (!same_events(expected_events, actual_events) ||
            static_cast<const Timing&>(reference) != timing(actual) ||
            reference.cycles_until_next_event() != actual.cycles_until_next_event() ||
            actual.vcount() != reference.vcount_ ||
            actual.in_vblank() != (reference.scanline_ >= Timing::kLinesVisible) ||
            actual.in_hblank() != (reference.dot_in_scanline_ >= Timing::kDotsVisible)) {
        std::fprintf(stderr,
            "FAIL line=%u dot=%u residue=%u frame=%llu cycles=%u compare=%u\n",
            initial.scanline_, initial.dot_in_scanline_, initial.cycle_in_dot_,
            static_cast<unsigned long long>(initial.frame_count_), cycles, compare);
        std::exit(1);
    }
}

static void differential() {
    gba::GbaPpu actual;
    // All 280896 valid raster phases, before/at/after the next hardware event.
    for (uint32_t phase = 0; phase < gba::GbaPpu::kCyclesPerFrame; ++phase) {
        Timing s;
        s.scanline_ = phase / 1232;
        s.dot_in_scanline_ = (phase % 1232) / 4;
        s.cycle_in_dot_ = phase % 4;
        s.vcount_ = s.scanline_;
        s.frame_count_ = 17;
        const uint32_t event = s.cycles_until_next_event();
        for (uint32_t step : {0u, 1u, 3u, 4u, event - 1, event, event + 1})
            verify(actual, s, step, static_cast<uint16_t>((s.scanline_ + 1) % 228));
    }
    // HBlank/VBlank/frame boundaries, invalid compares and frame-counter wrap.
    for (uint32_t line : {0u, 1u, 159u, 160u, 161u, 226u, 227u})
        for (uint32_t dot : {0u, 1u, 250u, 251u, 252u, 253u, 306u, 307u})
            for (uint32_t residue = 0; residue < 4; ++residue)
                for (uint32_t step : {0u, 1u, 2u, 3u, 4u, 5u, 223u, 224u, 225u,
                        1007u, 1008u, 1009u, 1231u, 1232u, 1233u,
                        280895u, 280896u, 280897u, 561793u})
                    for (uint16_t compare : {uint16_t(0), uint16_t(159), uint16_t(160),
                            uint16_t(227), uint16_t(228), uint16_t(65535)}) {
                        Timing s;
                        s.scanline_ = line; s.dot_in_scanline_ = dot;
                        s.cycle_in_dot_ = residue; s.vcount_ = line;
                        s.frame_count_ = UINT64_MAX;
                        verify(actual, s, step, compare);
                    }
    std::mt19937_64 rng(0x12001012);
    for (unsigned i = 0; i < 500000; ++i) {
        Timing s;
        s.scanline_ = rng() % 228; s.dot_in_scanline_ = rng() % 308;
        s.cycle_in_dot_ = rng() % 4; s.vcount_ = s.scanline_; s.frame_count_ = rng();
        verify(actual, s, rng() % 8192, static_cast<uint16_t>(rng() % 260));
    }
    // Large input and the historical uint32 accumulation overflow behavior.
    for (uint32_t residue = 0; residue < 4; ++residue) {
        Timing s;
        s.scanline_ = 227; s.dot_in_scanline_ = 307; s.cycle_in_dot_ = residue;
        s.vcount_ = 227; s.frame_count_ = UINT64_MAX;
        verify(actual, s, UINT32_MAX, 0);
        verify(actual, s, UINT32_MAX - 1, 160);
    }
    // The legacy snapshot reader does not validate these fields. Retain the
    // previous first-dot normalization, including UINT32_MAX dot/line wrap.
    // In particular, dot == 308 must never produce a zero-length infinite loop.
    for (uint32_t line : {228u, 229u, 65535u, UINT32_MAX})
        for (uint32_t dot : {308u, 309u, UINT32_MAX - 1, UINT32_MAX})
            for (uint32_t residue : {0u, 3u, 4u, 5u, 1235u})
                for (uint32_t step : {0u, 1u, 3u, 4u, 8u, 1232u, 280896u})
                    for (uint16_t compare : {uint16_t(0), uint16_t(160), uint16_t(65535)}) {
                        Timing s;
                        s.scanline_ = line; s.dot_in_scanline_ = dot;
                        s.cycle_in_dot_ = residue; s.vcount_ = 65535;
                        s.frame_count_ = UINT64_MAX;
                        verify(actual, s, step, compare);
                    }
    // A continuous stream with fragmented sub-dot cycles and changing compares.
    Reference reference;
    set_timing(actual, Timing{});
    for (unsigned i = 0; i < 500000; ++i) {
        const uint32_t cycles = rng() % 300;
        const uint16_t compare = rng() % 260;
        const auto expected_events = reference.tick(cycles, compare);
        const auto actual_events = actual.tick(cycles, compare);
        ++checks;
        if (!same_events(expected_events, actual_events) ||
                static_cast<const Timing&>(reference) != timing(actual) ||
                reference.cycles_until_next_event() != actual.cycles_until_next_event()) {
            std::fprintf(stderr, "FAIL fragmented stream at call %u\n", i);
            std::exit(1);
        }
    }
    std::printf("PASS production PPU tick differential checks=%llu\n",
        static_cast<unsigned long long>(checks));
}

template<class Ppu>
static double benchmark(std::string_view pattern, uint64_t& checksum) {
    constexpr uint64_t total = uint64_t(gba::GbaPpu::kCyclesPerFrame) * 600;
    Ppu p;
    uint64_t sent = 0, calls = 0, flags = 0;
    constexpr std::array<uint32_t, 16> small{1,3,4,4,6,8,12,16,4,4,8,16,32,64,224,1008};
    const auto began = std::chrono::steady_clock::now();
    while (sent < total) {
        const uint32_t cycles = pattern == "frame" ? gba::GbaPpu::kCyclesPerFrame
            : pattern == "event" ? p.cycles_until_next_event() : small[calls % small.size()];
        const auto ev = p.tick(cycles, static_cast<uint16_t>(calls % 228));
        flags += ev.vblank_started + ev.hblank_started + ev.vcount_matched + ev.frame_completed;
        sent += cycles; ++calls;
    }
    const double milliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - began).count();
    checksum += p.frame_count_ + p.vcount_ + p.dot_in_scanline_ + p.cycle_in_dot_ + flags + calls;
    return milliseconds;
}

int main(int argc, char** argv) {
    if (argc < 2 || std::string_view(argv[1]) != "--benchmark") {
        differential();
        return 0;
    }
    for (const auto pattern : {"mixed", "event", "frame"}) {
        std::array<double, 5> old{}, current{};
        uint64_t old_checksum = 0, current_checksum = 0;
        for (unsigned i = 0; i < old.size(); ++i) {
            old[i] = benchmark<Reference>(pattern, old_checksum);
            current[i] = benchmark<gba::GbaPpu>(pattern, current_checksum);
        }
        if (old_checksum != current_checksum) std::abort();
        std::sort(old.begin(), old.end()); std::sort(current.begin(), current.end());
        std::printf("PPU-only 600-frame microbenchmark pattern=%s median_ms old=%.3f current=%.3f speedup=%.2fx checksum=%llu\n",
            pattern, old[2], current[2], old[2] / current[2],
            static_cast<unsigned long long>(old_checksum));
    }
}
