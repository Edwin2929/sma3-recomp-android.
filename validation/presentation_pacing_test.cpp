#include "presentation_pacing.h"
#include "host_platform.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using Clock = std::chrono::steady_clock;
using Ns = std::chrono::nanoseconds;
constexpr Ns period{16742706};
static unsigned checks = 0;
static void require(bool ok, const char* message) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

struct FakePacer {
    Clock::time_point& now;
    Clock::time_point next{};
    unsigned resets = 0, full = 0, middle = 0;
    bool unlimited = false;
    sma3::OptionalPresentationBudget budget;
    explicit FakePacer(Clock::time_point& time) : now(time) {}
    void reset() { ++resets; next = now + period; }
    bool uncapped() const { return unlimited; }
    Clock::time_point current_time() const { return now; }
    Ns frame_period() const { return period; }
    Ns preparation_cost() const { return budget.preparation(); }
    Ns swap_cost() const { return budget.swap(); }
    std::uint64_t schedule_generation() const { return resets; }
    bool midpoint_preparation_due() const {
        return now >= next - period / 2 - budget.preparation() - sma3::kPresentationServiceLead;
    }
    bool midpoint_has_budget() const { return !unlimited && budget.permits(now, next, period); }
    bool wait_for_midpoint() {
        if (unlimited || !sma3::midpoint_is_timely(now, next, period)) return false;
        now = std::max(now, next - period / 2); ++middle; return true;
    }
    void wait_for_next_frame() {
        ++full;
        now = std::max(now, next);
        next = now > next + Ns{1500000} ? now + period : next + period;
    }
};

void optional_budget_cases() {
    sma3::OptionalPresentationBudget budget;
    Clock::time_point now{};
    const auto next = now + period;
    require(budget.permits(now, next, period), "unloaded midpoint has budget");
    require(!budget.permits(next, next, period), "already late midpoint rejected before preparation");
    budget.observe_preparation(Ns{2000000});
    budget.observe_swap(Ns{1000000});
    require(budget.permits(now, next, period), "measured preparation and swap fit midpoint plus full image");
    require(!budget.permits(now + Ns{8000000}, next, period), "preflight reserves measured preparation cost");
    budget.observe_preparation(Ns{5000000});
    budget.observe_swap(Ns{4000000});
    require(!budget.permits(now, next, period), "swap blocking plus full preparation can reject otherwise timely midpoint");
    const auto observed = budget.preparation();
    budget.observe_preparation(Ns{-1});
    require(budget.preparation() == observed, "negative sample does not poison budget");
    budget.observe_preparation(std::chrono::seconds(30));
    require(budget.preparation() == budget.kMaximumSample, "stall estimate is bounded");
    for (unsigned i = 0; i < 80; ++i) {
        budget.observe_preparation(Ns{1000000});
        budget.observe_swap(Ns{100000});
    }
    require(budget.preparation() == Ns{1000000} && budget.swap() == Ns{100000}, "budget recovers after transient stall");
    require(budget.permits(now, next, period), "midpoint recovers with available budget");
    require(!budget.permits(now, next, Ns{0}), "invalid period denied");

    sma3::FrameBlend expected, skipping;
    std::vector<std::uint8_t> pixels(240u * 160u * 3u);
    for (unsigned frame = 1; frame <= 90; ++frame) {
        if (frame == 40) pixels.resize(356u * 160u * 3u);
        for (std::size_t i = 0; i < pixels.size(); ++i)
            pixels[i] = static_cast<std::uint8_t>((frame < 20 ? frame : frame + 130) + i % 13);
        const auto guest_frame = frame < 60 ? frame : frame + 100;
        const bool wanted = expected.prepare(pixels.data(), pixels.size(), guest_frame);
        if (frame % 4 == 1 || frame % 4 == 2) {
            skipping.capture(pixels.data(), pixels.size(), guest_frame);
        } else {
            const bool got = skipping.prepare(pixels.data(), pixels.size(), guest_frame);
            require(got == wanted, "capture preserves scene-cut, geometry and consecutive-frame decisions");
            if (got) require(std::equal(expected.middle(), expected.middle() + pixels.size(), skipping.middle()),
                             "accepted midpoint after skipped slot is pixel-identical");
        }
    }
}

void adaptive_wait_cases() {
    sma3::AdaptiveDeadlineWait waiter;
    Clock::time_point now{};
    unsigned slept = 0, yielded = 0;
    auto read = [&] { return now; };
    auto yield = [&] { now += Ns{1000}; ++yielded; };
    for (int i = 0; i < 1200; ++i) {
        auto deadline = now + period / 2;
        Ns overshoot = (i % 71 == 0) ? Ns{2300000} : (i % 17 == 0) ? Ns{400000} : Ns{20000};
        waiter.wait(deadline, read, [&](Clock::time_point target) {
            require(target > now, "sleep target must be in future");
            ++slept; now = target + overshoot;
        }, yield);
        require(now >= deadline, "wait must never return early");
        require(waiter.tail() >= waiter.kMinimumTail && waiter.tail() <= waiter.kMaximumTail,
                "adaptive tail remains bounded");
    }
    require(slept == 1200 && yielded > 0, "wait uses sleep plus bounded yield tail");
    const auto before = now;
    waiter.wait(now - Ns{1}, read, [&](auto) { require(false, "late deadline must not sleep"); },
                [&] { require(false, "late deadline must not yield"); });
    require(before == now, "already late wait is immediate");
    for (int i = 0; i < 1000; ++i) waiter.wait(now + period, read,
        [&](auto target) { now = target; }, yield);
    require(waiter.tail() == waiter.kMinimumTail, "quiet intervals decay to minimum CPU tail");
    const auto next = now + period;
    const auto edge = next - period / 2 + period / 16;
    require(sma3::midpoint_is_timely(edge, next, period), "midpoint tolerance endpoint");
    require(!sma3::midpoint_is_timely(edge + Ns{1}, next, period), "late midpoint rejected");
    require(!sma3::midpoint_is_timely(now, next, Ns{0}), "invalid period rejected");
}

void presentation_cases() {
    for (bool doubled : {false, true}) {
        sma3::PresentationPacing schedule;
        Clock::time_point now{};
        FakePacer pacer(now);
        std::vector<Clock::time_point> full_times, middle_times;
        std::array<std::uint8_t, 12> pixels{};
        auto presenter = [&](const std::uint8_t*, FakePacer* clock, bool midpoint) {
            now += Ns{300000};
            clock->budget.observe_preparation(Ns{300000});
            if (midpoint) {
                if (!clock->wait_for_midpoint()) return false;
                middle_times.push_back(now);
            } else {
                clock->wait_for_next_frame(); full_times.push_back(now);
            }
            return true;
        };
        for (unsigned frame = 1; frame <= 180; ++frame) {
            now += Ns{frame % 2 ? 400000 : 2700000}; // variable guest preparation
            schedule.service(&pacer, doubled, true, 0, presenter, true);
            pixels.fill(static_cast<std::uint8_t>(frame / 3));
            const auto input = pixels;
            const auto called = now;
            require(schedule.present(&pacer, pixels.data(), pixels.size(), frame, doubled, true,
                called, 0, presenter), "normal presentation is fully paced");
            require(pixels == input, "presentation never changes guest framebuffer");
        }
        schedule.service(&pacer, doubled, true, 0, presenter, true);
        require(pacer.full == 180 && full_times.size() == 180, "one full wait per guest frame");
        require(pacer.resets == 1, "steady workload does not reset frame phase");
        require(pacer.middle == (doubled ? 180u : 0u), "selected midpoint cadence");
        for (unsigned i = 1; i < full_times.size(); ++i)
            require(full_times[i] - full_times[i - 1] == period,
                    "variable CPU/render work stays inside full-frame budget");
        for (unsigned i = 0; i < middle_times.size(); ++i)
            require(middle_times[i] - full_times[i] == period - period / 2,
                    "mandatory/deferred spacing");
        // Under the previous 60-FPS ordering, rendering finished before the wait.
        // These same prep costs made its swap intervals differ by milliseconds.
        const auto old_interval_a = period + Ns{2700000 + 300000 - 400000 - 1800000};
        const auto old_interval_b = period + Ns{400000 + 1800000 - 2700000 - 300000};
        require(old_interval_a != old_interval_b, "old present-before-wait exposes varying work");
    }
    sma3::PresentationPacing schedule;
    Clock::time_point now{};
    FakePacer pacer(now);
    std::array<std::uint8_t, 3> pixels{17, 83, 129};
    auto presenter = [&](const auto*, FakePacer* clock, bool midpoint) {
        if (!clock) return true;
        if (midpoint) { now += period; return clock->wait_for_midpoint(); }
        clock->wait_for_next_frame(); return true;
    };
    require(schedule.present(&pacer, pixels.data(), 3, 1, true, true, now, 0, presenter), "late frame still paced");
    require(pacer.middle == 0 && pacer.full == 1, "late optional midpoint omitted, full frame retained");
    require(schedule.present(&pacer, pixels.data(), 3, 2, false, true, now, 0, presenter), "mode switch remains paced");
    require(pacer.resets == 2, "mode switch resets phase");
    now += std::chrono::seconds(20);
    require(schedule.present(&pacer, pixels.data(), 3, 3, false, true, now, 1, presenter), "resumed frame paced");
    require(pacer.resets == 3, "resume resets old deadline");
    require(schedule.present(&pacer, pixels.data(), 3, 100, false, true, now, 1, presenter), "frame discontinuity paced");
    require(pacer.resets == 4, "save/load discontinuity resets phase");
    unsigned full = pacer.full;
    require(!schedule.present(&pacer, pixels.data(), 3, 101, false, false, now, 1, presenter), "fast forward bypasses presentation waits");
    require(pacer.full == full, "outer loop remains sole fast-forward pacer");
    require(schedule.present(&pacer, pixels.data(), 3, 102, false, true, now, 1,
        [](const auto*, auto*, bool) { return false; }), "lost renderer remains capped");
    require(pacer.full == full + 1, "lost texture fallback still waits once");
}

void counter_cases() {
    sma3::PresentationRateCounter counter;
    constexpr std::uint64_t hz = 1000000000;
    require(counter.record(0, hz, 0, false) < 0, "first sample is not a complete interval");
    double measured = -1;
    for (unsigned i = 1; i <= 30; ++i) measured = counter.record(i * period.count(), hz, 0, false);
    require(std::abs(measured - 1e9 / period.count()) < 0.00001, "60 label measures native cadence accurately");
    const auto resumed = 20 * hz;
    require(counter.record(resumed, hz, 1, false) < 0, "resume discards background time");
    for (unsigned i = 1; i <= 30; ++i) measured = counter.record(resumed + i * period.count(), hz, 1, false);
    require(std::abs(measured - 1e9 / period.count()) < 0.00001, "new resume window has no stale low sample");
    auto start = resumed + 30 * period.count();
    require(counter.record(start + 100000000, hz, 1, false) < 0, "short slowdown remains in open window");
    for (unsigned i = 1; i <= 25; ++i) {
        auto sample = counter.record(start + 100000000 + i * period.count(), hz, 1, false);
        if (sample >= 0) measured = sample;
    }
    require(measured < 55.0, "real in-game slowdown is measured, not hidden");
    start += hz;
    require(counter.record(start, hz, 1, true) < 0, "mode change starts fresh measured window");
    for (unsigned i = 1; i <= 60; ++i) measured = counter.record(start + i * (period / 2).count(), hz, 1, true);
    require(std::abs(measured - 2e9 / period.count()) < 0.0001, "120 label measures native doubled cadence");
    require(counter.record(1, hz, 1, true) < 0, "clock regression resets safely");
    require(counter.record(2, 0, 1, true) < 0, "zero frequency rejected");
}

void real_pacer_smoke() {
    gbarecomp::FramePacer pacer;
    auto started = Clock::now();
    unsigned midpoints = 0;
    for (unsigned i = 0; i < 20; ++i) {
        if (pacer.wait_for_midpoint()) ++midpoints;
        pacer.wait_for_next_frame();
    }
    const double seconds = std::chrono::duration<double>(Clock::now() - started).count();
    require(seconds >= .32 && seconds < 1.2, "real FramePacer advances twenty guest periods");
    require(midpoints > 0, "real FramePacer has intermediate slots");
    pacer.set_uncapped(true);
    require(!pacer.wait_for_midpoint(), "uncapped mode has no midpoint wait");
    std::printf("Host real-clock smoke:20 guest periods %.6fs; midpoint slots=%u (not Android FPS)\n", seconds, midpoints);
}

int main() {
    adaptive_wait_cases(); presentation_cases(); counter_cases(); optional_budget_cases(); real_pacer_smoke();
    std::printf("PASS presentation pacing checks=%u simulated_guest_frames=360\n", checks);
}
