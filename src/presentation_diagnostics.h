#pragma once

// Opt-in desktop instrumentation. No allocation or logging runs inside a
// frame; one bounded aggregate is printed at process exit. These measurements
// describe the host run, not an Android device or its display pipeline.
#if defined(__ANDROID__) || defined(ANDROID)
#error "Presentation diagnostics must not be packaged in Android"
#endif

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>

namespace sma3::presentation_diagnostics {

using Clock = std::chrono::steady_clock;
using Duration = std::chrono::nanoseconds;

struct Histogram {
    // Non-cumulative bounds in microseconds; the last bucket is unbounded.
    static constexpr std::array<std::uint64_t, 19> limits_us{
        50, 100, 250, 500, 750, 1000, 1250, 1400, 1500, 1600,
        1750, 2000, 2500, 3000, 4000, 8000, 16750, 33500, 100000};
    std::array<std::uint64_t, limits_us.size() + 1> buckets{};
    std::uint64_t count = 0, sum_ns = 0, maximum_ns = 0;

    void add(Duration elapsed) noexcept {
        if (elapsed.count() < 0) return;
        const auto ns = static_cast<std::uint64_t>(elapsed.count());
        ++count;
        sum_ns += ns;
        if (ns > maximum_ns) maximum_ns = ns;
        std::size_t bucket = 0;
        while (bucket < limits_us.size() && ns > limits_us[bucket] * 1000) ++bucket;
        ++buckets[bucket];
    }

    void write(const char* name) const noexcept {
        std::fprintf(stderr, "\"%s\":{\"count\":%llu,\"mean_us\":%.3f,\"max_us\":%.3f,\"buckets\":[",
            name, static_cast<unsigned long long>(count),
            count ? static_cast<double>(sum_ns) / count / 1000.0 : 0.0,
            maximum_ns / 1000.0);
        for (std::size_t i = 0; i < buckets.size(); ++i)
            std::fprintf(stderr, "%s%llu", i ? "," : "", static_cast<unsigned long long>(buckets[i]));
        std::fputs("]}", stderr);
    }
};

struct Collector {
    Histogram clock_check_gap, callback_gap, callback_cost;
    Histogram first_pending_poll, pending_poll_gap, forced_pending_poll;
    Histogram late_preparation, guest_cost, history_cost, preparation_estimate, swap_estimate;
    std::uint64_t armed = 0, early_polls = 0, total_budget_cancellations = 0;
    std::uint64_t optional_success = 0, optional_failure = 0;
    std::uint64_t admission_checks = 0, admission_late = 0, admission_full_budget = 0;
    Clock::time_point last_check{}, last_callback{}, last_pending_poll{};
    bool have_check = false, have_callback = false, first_poll = false;

    void clock_check(Clock::time_point now) noexcept {
        if (have_check) clock_check_gap.add(now - last_check);
        last_check = now;
        have_check = true;
    }
    void callback_started(Clock::time_point now) noexcept {
        if (have_callback) callback_gap.add(now - last_callback);
        last_callback = now;
        have_callback = true;
    }
    void callback_finished(Clock::time_point began, Clock::time_point ended) noexcept {
        callback_cost.add(ended - began);
    }
    void pending_armed(Clock::time_point now, bool pending) noexcept {
        if (pending) ++armed;
        last_pending_poll = now;
        first_poll = true;
    }
    void pending_polled(Clock::time_point now, bool force) noexcept {
        if (force) {
            // A frame-boundary flush is not a regular runtime polling sample.
            forced_pending_poll.add(now - last_pending_poll);
            return;
        }
        (first_poll ? first_pending_poll : pending_poll_gap).add(now - last_pending_poll);
        last_pending_poll = now;
        first_poll = false;
    }
    void admission(Clock::time_point prepared, Clock::time_point next,
                   Duration period, bool timely, bool fits) noexcept {
        ++admission_checks;
        if (!timely) {
            ++admission_late;
            late_preparation.add(prepared - (next - period / 2 + period / 16));
        } else if (!fits) {
            ++admission_full_budget;
        }
    }
    void frame_costs(Duration guest, Duration history, Duration preparation, Duration swap) noexcept {
        guest_cost.add(guest);
        history_cost.add(history);
        preparation_estimate.add(preparation);
        swap_estimate.add(swap);
    }

    ~Collector() {
        if (!clock_check_gap.count && !armed && !admission_checks) return;
        std::fputs("[presentation-diagnostics] {\"type\":\"host_only_aggregate_not_phone_fps\",\"bounds_us\":[", stderr);
        for (std::size_t i = 0; i < Histogram::limits_us.size(); ++i)
            std::fprintf(stderr, "%s%llu", i ? "," : "", static_cast<unsigned long long>(Histogram::limits_us[i]));
        std::fprintf(stderr,
            "],\"armed\":%llu,\"early_polls\":%llu,\"total_budget_cancellations\":%llu,"
            "\"optional_success\":%llu,\"optional_failure\":%llu,\"admission_checks\":%llu,"
            "\"admission_late\":%llu,\"admission_full_budget\":%llu,",
            static_cast<unsigned long long>(armed), static_cast<unsigned long long>(early_polls),
            static_cast<unsigned long long>(total_budget_cancellations),
            static_cast<unsigned long long>(optional_success), static_cast<unsigned long long>(optional_failure),
            static_cast<unsigned long long>(admission_checks), static_cast<unsigned long long>(admission_late),
            static_cast<unsigned long long>(admission_full_budget));
        clock_check_gap.write("clock_check_gap"); std::fputc(',', stderr);
        callback_gap.write("callback_gap_includes_deliberate_waits"); std::fputc(',', stderr);
        callback_cost.write("callback_cost"); std::fputc(',', stderr);
        first_pending_poll.write("first_pending_poll"); std::fputc(',', stderr);
        pending_poll_gap.write("pending_poll_gap"); std::fputc(',', stderr);
        forced_pending_poll.write("forced_pending_poll"); std::fputc(',', stderr);
        late_preparation.write("late_preparation_excess"); std::fputc(',', stderr);
        guest_cost.write("guest_estimate"); std::fputc(',', stderr);
        history_cost.write("history_estimate"); std::fputc(',', stderr);
        preparation_estimate.write("preparation_estimate"); std::fputc(',', stderr);
        swap_estimate.write("swap_estimate");
        std::fputs("}\n", stderr);
    }
};

inline Collector& collector() noexcept {
    static Collector instance;
    return instance;
}

}  // namespace sma3::presentation_diagnostics
