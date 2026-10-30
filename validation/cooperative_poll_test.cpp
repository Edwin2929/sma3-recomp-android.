// Deterministic tests of the actual armed/deadline polling helper and scheduler.
// These timings are a synthetic clock, not measurements of an Android device.
#include "presentation_pacing.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using Clock = std::chrono::steady_clock;
using Ns = std::chrono::nanoseconds;
constexpr Ns period{16742706};
static unsigned checks = 0;
static void require(bool ok, const char* label) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL %s\n", label); std::exit(1); }
}

// Reference of the bridge in 0.10.13: the first sampled clock establishes the
// 1 ms grid, independent of whether PresentationPacing has an image pending.
struct LegacyPoll {
    std::uint32_t ticks = 0;
    bool started = false;
    Clock::time_point last{};
    template<class Now, class Service>
    void poll(bool doubled, Now now, Service service) {
        if ((++ticks & 0x3ffu) != 0 || !doubled) return;
        const auto current = now();
        if (!started) { started = true; last = current; }
        if (current - last >= std::chrono::milliseconds(1)) {
            last = current;
            service(current);
        }
    }
};

void helper_cases() {
    sma3::CooperativePresentationPoll poll;
    Clock::time_point now{Ns{1000000000}};
    unsigned clocks = 0, calls = 0;
    auto read = [&] { ++clocks; return now; };
    auto callback = [&](auto) { ++calls; poll.cancel(); };
    for (unsigned i = 0; i < 1000000; ++i) poll.poll(read, callback);
    require(clocks == 0 && calls == 0, "disarmed poll does no clock reads or callbacks");
    const auto due = now + Ns{3000000};
    poll.arm(due, now);
    for (unsigned i = 0; i < 1023; ++i) poll.poll(read, callback);
    require(clocks == 0, "tick divider avoids per-instruction clock reads");
    poll.poll(read, callback);
    require(clocks == 1 && calls == 0, "armed but early poll cannot invoke callback");
    now = due;
    for (unsigned i = 0; i < 1024; ++i) poll.poll(read, callback);
    require(clocks == 2 && calls == 1 && !poll.armed(), "first sampled deadline is eligible inclusively");
    for (unsigned i = 0; i < 1000000; ++i) poll.poll(read, callback);
    require(clocks == 2 && calls == 1, "completed image stops all optional clock polling");
    poll.arm(now + Ns{1000000}, now);
    now -= Ns{1};
    for (unsigned i = 0; i < 1024; ++i) poll.poll(read, callback);
    require(calls == 2 && !poll.armed(), "clock regression reaches cancellation instead of waiting forever");
    poll.arm(now - Ns{1}, now);
    for (unsigned i = 0; i < 1024; ++i) poll.poll(read, callback);
    require(calls == 3, "already due request is eligible on first sampled clock");
}

void phase_comparison() {
    std::uint64_t cases = 0, old_clocks = 0, new_clocks = 0;
    std::uint64_t old_calls = 0, new_calls = 0, old_late = 0, new_late = 0;
    Ns old_sum{}, new_sum{}, old_max{}, new_max{};
    // Sweep the SAME initial clock-check phase relative to the preparation
    // deadline, including intervals coarser than the former 1 ms filter.
    // This is not a claim of earlier callbacks in every production frame:
    // stopping inactive ticks can change phase relative to the old divider.
    // rearm_phase_cases separately checks the worst-case tick bound.
    for (int quantum_us : {100, 250, 400, 700, 1000, 1500}) {
        for (int phase_us = 0; phase_us < 2000; phase_us += 25) {
            const auto origin = Clock::time_point{Ns{1000000000}};
            const auto due = origin + period / 2 - Ns{750000} - sma3::kPresentationServiceLead;
            const auto latest = origin + period / 2 + period / 16 - Ns{750000};
            sma3::CooperativePresentationPoll poll;
            poll.arm(due, origin);
            LegacyPoll legacy;
            Clock::time_point old_at{}, new_at{};
            for (Ns elapsed{phase_us * 1000}; elapsed < Ns{20000000}; elapsed += Ns{quantum_us * 1000}) {
                const auto current = origin + elapsed;
                for (unsigned tick = 0; tick < 1024; ++tick) {
                    legacy.poll(true, [&] { ++old_clocks; return current; }, [&](auto at) {
                        ++old_calls;
                        if (at >= due && old_at == Clock::time_point{}) old_at = at;
                    });
                    poll.poll([&] { ++new_clocks; return current; }, [&](auto at) {
                        ++new_calls; new_at = at; poll.cancel();
                    });
                }
            }
            require(old_at != Clock::time_point{} && new_at != Clock::time_point{}, "both pollers reach every phase deadline");
            require(new_at <= old_at, "at the same initial sample phase the explicit deadline removes the 1 ms gate delay");
            const auto old_delay = std::chrono::duration_cast<Ns>(old_at - due);
            const auto new_delay = std::chrono::duration_cast<Ns>(new_at - due);
            old_sum += old_delay; new_sum += new_delay;
            old_max = std::max(old_max, old_delay); new_max = std::max(new_max, new_delay);
            old_late += old_at > latest; new_late += new_at > latest;
            ++cases;
        }
    }
    require(new_sum < old_sum && new_max <= old_max, "deadline policy improves phase delay aggregate without a worse maximum");
    require(new_late <= old_late, "deadline policy never loses a previously timely phase");
    require(new_calls == cases && new_clocks < old_clocks, "one callback per armed image and fewer clocks after completion");
    std::printf("{\"name\":\"poll_phase_sweep\",\"scope\":\"deterministic_synthetic_clock_not_phone\",\"cases\":%llu,\"old_mean_delay_us\":%.6f,\"new_mean_delay_us\":%.6f,\"old_max_delay_us\":%.6f,\"new_max_delay_us\":%.6f,\"old_late\":%llu,\"new_late\":%llu,\"old_clock_reads\":%llu,\"new_clock_reads\":%llu,\"old_callbacks\":%llu,\"new_callbacks\":%llu}\n",
        static_cast<unsigned long long>(cases), old_sum.count() / (1000.0 * cases), new_sum.count() / (1000.0 * cases),
        old_max.count() / 1000.0, new_max.count() / 1000.0,
        static_cast<unsigned long long>(old_late), static_cast<unsigned long long>(new_late),
        static_cast<unsigned long long>(old_clocks), static_cast<unsigned long long>(new_clocks),
        static_cast<unsigned long long>(old_calls), static_cast<unsigned long long>(new_calls));
}

void rearm_phase_cases() {
    for (unsigned phase = 0; phase < 1024; ++phase) {
        sma3::CooperativePresentationPoll poll;
        auto now = Clock::time_point{Ns{1000000000}};
        unsigned clocks = 0, calls = 0;
        auto read = [&] { ++clocks; return now; };
        auto callback = [&](auto) { ++calls; poll.cancel(); };
        poll.arm(now + Ns{1000000000}, now);
        for (unsigned i = 0; i < phase; ++i) poll.poll(read, callback);
        poll.cancel();
        const auto clocks_before = clocks, calls_before = calls;
        // Different inactive lengths deliberately break alignment with the
        // former always-running divider; no old/new time ordering is assumed.
        for (unsigned i = 0; i < (phase * 7 + 13) % 2048; ++i) {
            now += Ns{100}; poll.poll(read, callback);
        }
        require(clocks == clocks_before && calls == calls_before,
                "every inactive divider phase avoids clocks and callbacks");
        poll.arm(now, now);
        unsigned ticks = 0;
        while (poll.armed() && ticks < 1024) {
            ++ticks; now += Ns{100}; poll.poll(read, callback);
        }
        require(ticks >= 1 && ticks <= 1024 && !poll.armed(),
                "a rearmed due request is sampled within at most1024 runtime ticks");
        require(clocks == clocks_before + 1 && calls == calls_before + 1,
                "a rearmed due request needs one clock and one callback");
    }
    std::printf("{\"name\":\"poll_rearm_phase_sweep\",\"scope\":\"deterministic_tick_bound_not_phone_latency\",\"cases\":1024,\"maximum_ticks_to_due_callback\":1024,\"inactive_clock_reads\":0,\"inactive_callbacks\":0,\"reference_phase_alignment_required\":false}\n");
}

struct Pacer {
    Clock::time_point& now;
    Clock::time_point next{};
    sma3::OptionalPresentationBudget budget;
    unsigned full = 0, middle = 0, resets = 0;
    bool unlimited = false;
    explicit Pacer(Clock::time_point& time) : now(time) {}
    Clock::time_point current_time() const { return now; }
    Ns preparation_cost() const { return budget.preparation(); }
    Ns swap_cost() const { return budget.swap(); }
    Ns frame_period() const { return period; }
    void reset() { ++resets; next = now + period; }
    bool uncapped() const { return unlimited; }
    std::uint64_t schedule_generation() const { return resets; }
    Clock::time_point midpoint_preparation_deadline() const {
        return next - period / 2 - budget.preparation() - sma3::kPresentationServiceLead;
    }
    bool midpoint_preparation_due() const { return now >= midpoint_preparation_deadline(); }
    bool midpoint_has_budget() const { return !unlimited && budget.permits(now, next, period); }
    bool wait_for_midpoint() {
        if (!sma3::midpoint_is_timely(now, next, period)) return false;
        now = std::max(now, next - period / 2); ++middle; return true;
    }
    void wait_for_next_frame() {
        ++full;
        if (now > next + period) { next = now + period; return; }
        now = std::max(now, next);
        next = now > next + Ns{1500000} ? now + period : next + period;
    }
};

struct Harness {
    Clock::time_point now{Ns{1000000000}};
    Pacer pacer{now};
    sma3::PresentationPacing scheduling;
    LegacyPoll legacy;
    std::array<std::uint8_t, 3> pixels{};
    std::vector<std::uint8_t> seen;
    std::vector<Clock::time_point> mandatory;
    Ns prep{750000}, swap{250000}, quantum{250000};
    bool use_legacy = false, doubled = true, realtime = true, fail_optional = false;
    unsigned frame = 0, clocks = 0, callbacks = 0;
    std::uint64_t epoch = 0;

    auto presenter() { return [&](const std::uint8_t* data, Pacer* clock, bool optional) {
        if (!clock) { seen.push_back(data[0]); return true; }
        if (optional && fail_optional) return false;
        if (optional && !clock->midpoint_has_budget()) return false;
        now += prep;
        clock->budget.observe_preparation(prep, period);
        if (optional) {
            if (!clock->wait_for_midpoint()) return false;
        } else { clock->wait_for_next_frame(); mandatory.push_back(now); }
        seen.push_back(data[0]);
        now += swap;
        clock->budget.observe_swap(swap, period);
        return true;
    }; }
    void poll_tick() {
        auto read = [&] { ++clocks; return now; };
        auto service = [&](auto) {
            ++callbacks;
            scheduling.service(&pacer, doubled, realtime, epoch, presenter());
        };
        if (use_legacy) legacy.poll(doubled, read, service);
        else scheduling.cooperative_poll().poll(read, service);
    }
    void guest(Ns work) {
        while (work > Ns::zero()) {
            const auto chunk = std::min(work, quantum);
            now += chunk; work -= chunk;
            for (unsigned i = 0; i < 1024; ++i) poll_tick();
        }
    }
    void boundary(unsigned char pixel) {
        scheduling.service(&pacer, doubled, realtime, epoch, presenter(), true);
        pixels.fill(pixel);
        scheduling.present(&pacer, pixels.data(), pixels.size(), ++frame,
                           doubled, realtime, now, epoch, presenter());
    }
};

void cadence_cases() {
    for (bool doubled : {false, true}) for (int guest_us : {2000, 10000, 14000, 17000}) {
        Harness before, after;
        before.use_legacy = true;
        before.doubled = after.doubled = doubled;
        for (int i = 0; i < 30; ++i) {
            before.guest(Ns{guest_us * 1000}); before.boundary(i);
            after.guest(Ns{guest_us * 1000}); after.boundary(i);
        }
        before.clocks = before.callbacks = after.clocks = after.callbacks = 0;
        const auto began_before = before.now, began_after = after.now;
        const auto extra_before = before.pacer.middle, extra_after = after.pacer.middle;
        const auto full_before = before.pacer.full, full_after = after.pacer.full;
        for (int i = 0; i < 120; ++i) {
            before.guest(Ns{guest_us * 1000}); before.boundary(i % 180);
            after.guest(Ns{guest_us * 1000}); after.boundary(i % 180);
        }
        require(before.pacer.full - full_before == 120 && after.pacer.full - full_after == 120,
                "both policies retain every mandatory guest deadline");
        require(after.pacer.middle - extra_after >= before.pacer.middle - extra_before,
                "explicit deadline does not lose optional images under fitting workloads");
        require(after.now - began_after == before.now - began_before,
                "poll optimization preserves simulated elapsed time and native game speed");
        if (guest_us <= 14000)
            require(after.now - began_after == 120 * period, "fitting work remains at the original guest frequency");
        if (!doubled || guest_us == 17000)
            require(after.clocks == 0 && after.callbacks == 0, "60 mode and sustained overload have no optional polling work");
        std::printf("{\"name\":\"poll_cadence\",\"scope\":\"deterministic_synthetic_clock_not_phone\",\"mode\":%u,\"guest_us\":%d,\"mandatory\":120,\"old_extra\":%u,\"new_extra\":%u,\"old_clock_reads\":%u,\"new_clock_reads\":%u,\"old_callbacks\":%u,\"new_callbacks\":%u,\"guest_hz\":%.6f}\n",
            doubled ? 120u : 60u, guest_us, before.pacer.middle - extra_before, after.pacer.middle - extra_after,
            before.clocks, after.clocks, before.callbacks, after.callbacks,
            120.0 / std::chrono::duration<double>(after.now - began_after).count());
    }
}

void cancellation_cases() {
    for (int reason = 0; reason < 7; ++reason) {
        Harness h; h.boundary(10);
        require(h.scheduling.cooperative_poll().armed(), "new double-rate boundary arms a deferred image");
        switch (reason) {
            case 0: h.doubled = false; break;
            case 1: h.realtime = false; break;  // pause or fast-forward
            case 2: ++h.epoch; break;          // surface/lifecycle restart
            case 3: h.pacer.reset(); break;    // load/rewind/reset
            case 4: h.now += std::chrono::seconds(2); break;
            case 5: h.now -= std::chrono::seconds(2); break;
            case 6: h.pacer.unlimited = true; break;
        }
        h.guest(Ns{10000000});
        require(!h.scheduling.cooperative_poll().armed(), "cancellation disarms the poll ticket");
        require(h.pacer.middle == 0 && h.seen.size() == 1, "cancelled request never displays stale pixels");
        const auto clocks = h.clocks, callbacks = h.callbacks;
        h.guest(Ns{10000000});
        require(h.clocks == clocks && h.callbacks == callbacks, "cancelled request does not keep polling");
        h.doubled = h.realtime = true; h.pacer.unlimited = false;
        ++h.epoch; h.pacer.reset();  // real lifecycle/mode resume resets timing
        h.boundary(20); h.guest(Ns{10000000}); h.boundary(30);
        require(h.pacer.full == 3 && h.pacer.middle == 1, "resume restores optional work and one full wait per boundary");
    }
    Harness failed; failed.boundary(10); failed.fail_optional = true;
    failed.guest(Ns{10000000});
    require(!failed.scheduling.cooperative_poll().armed() && failed.pacer.middle == 0,
            "failed presenter consumes the request instead of spinning retries");
    failed.boundary(20);
    require(failed.seen.back() == 20, "failed optional path falls back to current raw frame");
    Harness immutable; immutable.boundary(10); immutable.pixels.fill(240);
    immutable.guest(Ns{10000000});
    require(immutable.seen.back() == 10 && immutable.pacer.middle == 1,
            "deadline ticket preserves the immutable pending image");
    Harness reentry; reentry.boundary(10); reentry.now += Ns{7000000};
    const auto clocks = reentry.clocks;
    reentry.scheduling.service(&reentry.pacer, true, true, 0,
        [&](const auto*, auto*, bool) {
            for (unsigned i = 0; i < 2048; ++i) reentry.poll_tick();
            return true;
        }, true);
    require(reentry.clocks == clocks && !reentry.scheduling.cooperative_poll().armed(),
            "request is disarmed before a reentrant presentation callback");
}

int main() {
    helper_cases(); phase_comparison(); rearm_phase_cases(); cadence_cases(); cancellation_cases();
    std::printf("PASS cooperative poll checks=%u\n", checks);
}
