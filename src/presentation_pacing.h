#pragma once

#include "frame_interpolation.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace sma3 {

// Incremented on Android lifecycle/surface restart, never every frame. Both
// the scheduler and measured-FPS window discard time spent in the background.
inline std::atomic<std::uint64_t> presentation_epoch{0};

// The same wait policy serves full and intermediate presentation slots. Sleep
// for most of the interval, then yield only through a small adaptive margin.
// Oversleep observations raise that margin; quiet intervals decay it. This is
// a bounded scheduling aid, not a real-time or device-performance guarantee.
class AdaptiveDeadlineWait {
public:
    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::nanoseconds;
    static constexpr Duration kMinimumTail{100000};
    static constexpr Duration kMaximumTail{1500000};

    template<class Now, class Sleep, class Yield>
    void wait(Clock::time_point deadline, Now now, Sleep sleep, Yield yield) {
        const auto current = now();
        const auto sleep_until = deadline - tail_;
        if (current < sleep_until) {
            sleep(sleep_until);
            const Duration overslept = std::max(Duration::zero(),
                std::chrono::duration_cast<Duration>(now() - sleep_until));
            tail_ = std::clamp(std::max(tail_ - tail_ / 64,
                                      overslept + kMinimumTail),
                               kMinimumTail, kMaximumTail);
        }
        while (now() < deadline) yield();
    }

    Duration tail() const noexcept { return tail_; }

private:
    Duration tail_{300000};
};

// A midpoint arriving over one sixteenth of a guest frame late is obsolete.
// Omitting that optional image preserves the complete frame's next slot;
// presenting it several milliseconds late creates a visible short/long pair.
inline bool midpoint_is_timely(std::chrono::steady_clock::time_point now,
                               std::chrono::steady_clock::time_point next,
                               std::chrono::nanoseconds period) noexcept {
    return period.count() > 0 && now <= next - period / 2 + period / 16;
}

// Admission control for an optional image, using observed rendering and swap
// cost. A late image is rejected BEFORE color conversion, upload, filters and
// control drawing. Reserve enough time after the midpoint for the complete
// guest image too; swap/backpressure is real work even with VSync disabled.
// A new slow sample takes effect immediately. Its conservative high-water
// estimate decays over subsequent samples so a transient stall can recover.
class OptionalPresentationBudget {
public:
    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::nanoseconds;
    static constexpr Duration kPreparationMargin{100000};
    static constexpr Duration kMaximumSample{100000000};

    void observe_preparation(Duration sample) noexcept { observe(preparation_, sample); }
    void observe_swap(Duration sample) noexcept { observe(swap_, sample); }

    bool permits(Clock::time_point now, Clock::time_point next,
                 Duration period) const noexcept {
        if (period.count() <= 0) return false;
        const Duration preparation = preparation_ + kPreparationMargin;
        const auto prepared = now + preparation;
        if (!midpoint_is_timely(prepared, next, period)) return false;
        const auto midpoint = std::max(prepared, next - period / 2);
        // The full image is prepared before its own deadline. Its swap comes
        // after that deadline, as in the normal one-image path.
        return midpoint + swap_ + preparation <= next;
    }

    Duration preparation() const noexcept { return preparation_; }
    Duration swap() const noexcept { return swap_; }

private:
    static void observe(Duration& estimate, Duration sample) noexcept {
        if (sample.count() < 0) return;
        sample = std::min(sample, kMaximumSample);
        estimate = std::max(sample, estimate - estimate / 8);
    }
    Duration preparation_{}, swap_{};
};

// Coordinates exactly one full pacing wait per guest frame. The presenter
// prepares its texture/filter/controls first and performs the supplied slot's
// wait immediately before SDL swaps the completed image. No guest step occurs
// here and interpolation remains a blend of adjacent completed frames.
class PresentationPacing {
public:
    using Clock = std::chrono::steady_clock;

    template<class Pacer, class Presenter>
    bool present(Pacer* pacer, const std::uint8_t* pixels, std::size_t bytes,
                 std::uint64_t frame, bool double_rate, bool realtime,
                 Clock::time_point now, std::uint64_t epoch, Presenter presenter) {
        if (!pacer || !realtime || pacer->uncapped()) {
            blend_.reset();
            was_realtime_ = false;
            presenter(pixels, static_cast<Pacer*>(nullptr), false);
            return false;
        }
        const bool discontinuity = !was_realtime_ || double_rate != was_double_ ||
            epoch != epoch_ || now < last_time_ ||
            now - last_time_ > std::chrono::milliseconds(100) ||
            frame != last_frame_ + 1;
        if (discontinuity) {
            blend_.reset();
            pacer->reset();
        }
        was_realtime_ = true;
        was_double_ = double_rate;
        last_time_ = now;
        last_frame_ = frame;
        epoch_ = epoch;
        if (double_rate) {
            if (pacer->midpoint_has_budget()) {
                const bool middle = blend_.prepare(pixels, bytes, frame);
                presenter(middle ? blend_.middle() : pixels, pacer, true);
            } else {
                blend_.capture(pixels, bytes, frame);
            }
        } else {
            blend_.reset();
        }
        if (!presenter(pixels, pacer, false)) {
            // A lost SDL texture/window must not accidentally uncap the guest.
            pacer->wait_for_next_frame();
        }
        return true;
    }

private:
    FrameBlend blend_;
    bool was_realtime_ = false;
    bool was_double_ = false;
    Clock::time_point last_time_{};
    std::uint64_t last_frame_ = 0;
    std::uint64_t epoch_ = 0;
};

// Counts completed presentation intervals, not the first sample as an extra
// frame. Epoch/mode changes start a fresh real measurement; ordinary slow
// frames remain in the result instead of being discarded as outliers.
class PresentationRateCounter {
public:
    double record(std::uint64_t ticks, std::uint64_t frequency,
                  std::uint64_t epoch, bool double_rate) noexcept {
        if (frequency == 0) return -1.0;
        if (!started_ || epoch != epoch_ || double_rate != double_rate_ || ticks < last_) {
            started_ = true;
            epoch_ = epoch;
            double_rate_ = double_rate;
            start_ = last_ = ticks;
            intervals_ = 0;
            return -1.0;
        }
        last_ = ticks;
        ++intervals_;
        const std::uint64_t elapsed = ticks - start_;
        if (elapsed < frequency / 2 || elapsed == 0) return -1.0;
        const double result = static_cast<double>(intervals_) *
            static_cast<double>(frequency) / static_cast<double>(elapsed);
        start_ = ticks;
        intervals_ = 0;
        return result;
    }
    void reset() noexcept { started_ = false; }

private:
    bool started_ = false;
    bool double_rate_ = false;
    std::uint64_t epoch_ = 0, start_ = 0, last_ = 0, intervals_ = 0;
};

}  // namespace sma3
