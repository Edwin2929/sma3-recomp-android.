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
// control drawing. Reserve enough time after the optional half slot for the
// next mandatory image too; swap/backpressure is work even with VSync disabled.
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
        // The next mandatory image is prepared before its guest deadline. Its
        // swap comes after that deadline, as in the normal one-image path.
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

// Cooperative presentation polling may arrive a little before its estimated
// preparation start. Reserve that early wait as real work in the double-rate
// budget; otherwise a near-capacity guest can be slowed by the optional image.
inline constexpr std::chrono::nanoseconds kPresentationServiceLead{600000};

// Keep one mandatory presentation/wait at each native guest deadline. In 120
// mode that image is the blend of the two completed guest frames, followed by
// an immutable copy of the new full image half a period later. The optional
// copy is serviced on the SAME SDL/guest thread while the next guest frame is
// running, so guest work can use the whole 16.74 ms period rather than having
// to finish before the old 8.37 ms midpoint. This adds half a guest period of
// visual latency to a full image, without changing guest/input/audio timing.
class PresentationPacing {
public:
    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::nanoseconds;

    // Normal polls return immediately until preparation is due. At a guest
    // boundary, force=true flushes any still-pending image BEFORE the caller
    // changes HostWindow geometry or replaces its framebuffer. Late images
    // are discarded, never rendered in a burst to inflate the FPS counter.
    template<class Pacer, class Presenter>
    bool service(Pacer* pacer, bool double_rate, bool realtime,
                 std::uint64_t epoch, Presenter presenter, bool force = false) {
        if (!pending_) return false;
        if (!pacer || !realtime || !double_rate || pacer->uncapped() ||
            epoch != epoch_ || pacer->schedule_generation() != generation_) {
            pending_ = false;
            return false;
        }
        const auto now = pacer->current_time();
        if (now < execution_started_ ||
            now - execution_started_ > std::chrono::milliseconds(100)) {
            pending_ = false;
            return false;
        }
        // Rendering the mandatory image may have raised the measured cost
        // after the original admission decision. Use those fresh observations
        // before spending guest time on its deferred image.
        if (!has_total_budget(pacer)) {
            pending_ = false;
            deferred_succeeded_ = false;
            return false;
        }
        if (!force && !pacer->midpoint_preparation_due()) return false;
        pending_ = false;  // protect against callback re-entry
        if (!pacer->midpoint_has_budget()) {
            deferred_succeeded_ = false;
            return false;
        }
        const auto began = pacer->current_time();
        deferred_succeeded_ = presenter(full_.data(), pacer, true);
        service_elapsed_ += pacer->current_time() - began;
        return deferred_succeeded_;
    }

    template<class Pacer, class Presenter>
    bool present(Pacer* pacer, const std::uint8_t* pixels, std::size_t bytes,
                 std::uint64_t frame, bool double_rate, bool realtime,
                 Clock::time_point now, std::uint64_t epoch, Presenter presenter) {
        if (!pacer || !realtime || pacer->uncapped()) {
            reset();
            presenter(pixels, static_cast<Pacer*>(nullptr), false);
            return false;
        }
        const bool discontinuity = !was_realtime_ || double_rate != was_double_ ||
            epoch != epoch_ || now < last_time_ ||
            now - last_time_ > std::chrono::milliseconds(100) ||
            frame != last_frame_ + 1 || bytes != last_bytes_ ||
            pacer->schedule_generation() != generation_;
        if (discontinuity) {
            reset();
            pacer->reset();
        } else if (execution_started_ != Clock::time_point{}) {
            // Audio/input/PPU-copy work between presentations belongs to the
            // guest budget. Subtract only our own cooperative rendering/wait.
            observe(guest_, std::chrono::duration_cast<Duration>(
                now - execution_started_ - service_elapsed_));
        }
        service(pacer, double_rate, realtime, epoch, presenter, true);
        const bool previous_full_succeeded = deferred_succeeded_;
        was_realtime_ = true;
        was_double_ = double_rate;
        last_time_ = now;
        last_frame_ = frame;
        last_bytes_ = bytes;
        epoch_ = epoch;
        const auto history_started = pacer->current_time();
        const bool can_double = double_rate && has_total_budget(pacer);
        const std::uint8_t* mandatory = pixels;
        if (double_rate) {
            // If the previous deferred slot was missed, show the current raw
            // frame rather than continuing with only blended images. Retain
            // history without doing a blend that cannot be shown.
            if (can_double && previous_full_succeeded) {
                if (blend_.prepare(pixels, bytes, frame)) mandatory = blend_.middle();
            } else {
                blend_.capture(pixels, bytes, frame);
            }
            if (can_double) full_.assign(pixels, pixels + bytes);
        } else {
            blend_.reset();
        }
        observe(history_preparation_, std::chrono::duration_cast<Duration>(
            pacer->current_time() - history_started));
        if (!presenter(mandatory, pacer, false)) {
            // A lost SDL texture/window must not accidentally uncap the guest.
            pacer->wait_for_next_frame();
        }
        execution_started_ = pacer->current_time();
        generation_ = pacer->schedule_generation();
        service_elapsed_ = Duration::zero();
        // History and mandatory rendering have now been measured. Do not arm
        // an extra image using the cheaper estimate from before that work.
        pending_ = can_double && has_total_budget(pacer);
        deferred_succeeded_ = false;
        return true;
    }

private:
    template<class Pacer>
    bool has_total_budget(const Pacer* pacer) const noexcept {
        return guest_ + history_preparation_ +
            2 * (pacer->preparation_cost() + pacer->swap_cost()) +
            kPresentationServiceLead + OptionalPresentationBudget::kPreparationMargin
                <= pacer->frame_period();
    }
    static void observe(Duration& estimate, Duration sample) noexcept {
        if (sample.count() < 0) return;
        sample = std::min(sample, OptionalPresentationBudget::kMaximumSample);
        estimate = std::max(sample, estimate - estimate / 8);
    }
    void reset() {
        blend_.reset();
        full_.clear();
        pending_ = deferred_succeeded_ = was_realtime_ = false;
        guest_ = history_preparation_ = service_elapsed_ = Duration::zero();
        execution_started_ = {};
    }
    FrameBlend blend_;
    std::vector<std::uint8_t> full_;
    bool pending_ = false;
    bool deferred_succeeded_ = false;
    bool was_realtime_ = false;
    bool was_double_ = false;
    Clock::time_point last_time_{}, execution_started_{};
    Duration guest_{}, history_preparation_{}, service_elapsed_{};
    std::size_t last_bytes_ = 0;
    std::uint64_t last_frame_ = 0;
    std::uint64_t epoch_ = 0, generation_ = 0;
};

// Counts completed presentation intervals, not the first sample as an extra
// frame. Epoch/mode changes start a fresh real measurement; ordinary slow
// frames remain in the result instead of being discarded as outliers.
class PresentationRateCounter {
public:
    double record(std::uint64_t ticks, std::uint64_t frequency,
                  std::uint64_t epoch, bool double_rate, bool optional = false,
                  std::uint64_t preparation_ns = 0, std::uint64_t swap_ns = 0) noexcept {
        if (frequency == 0) return -1.0;
        if (!started_ || epoch != epoch_ || double_rate != double_rate_ || ticks < last_) {
            started_ = true;
            epoch_ = epoch;
            double_rate_ = double_rate;
            start_ = last_ = ticks;
            intervals_ = 0;
            base_intervals_ = optional_intervals_ = 0;
            preparation_ns_ = swap_ns_ = 0;
            return -1.0;
        }
        last_ = ticks;
        ++intervals_;
        if (optional) ++optional_intervals_;
        else ++base_intervals_;
        preparation_ns_ += preparation_ns;
        swap_ns_ += swap_ns;
        const std::uint64_t elapsed = ticks - start_;
        if (elapsed < frequency / 2 || elapsed == 0) return -1.0;
        const double result = static_cast<double>(intervals_) *
            static_cast<double>(frequency) / static_cast<double>(elapsed);
        const double seconds = static_cast<double>(elapsed) / static_cast<double>(frequency);
        base_rate_ = static_cast<double>(base_intervals_) / seconds;
        extra_rate_ = static_cast<double>(optional_intervals_) / seconds;
        preparation_ms_ = static_cast<double>(preparation_ns_) / intervals_ / 1e6;
        swap_ms_ = static_cast<double>(swap_ns_) / intervals_ / 1e6;
        start_ = ticks;
        intervals_ = 0;
        base_intervals_ = optional_intervals_ = 0;
        preparation_ns_ = swap_ns_ = 0;
        return result;
    }
    void reset() noexcept { started_ = false; }
    double base_rate() const noexcept { return base_rate_; }
    double extra_rate() const noexcept { return extra_rate_; }
    double preparation_ms() const noexcept { return preparation_ms_; }
    double swap_ms() const noexcept { return swap_ms_; }

private:
    bool started_ = false;
    bool double_rate_ = false;
    std::uint64_t epoch_ = 0, start_ = 0, last_ = 0, intervals_ = 0;
    std::uint64_t base_intervals_ = 0, optional_intervals_ = 0;
    std::uint64_t preparation_ns_ = 0, swap_ns_ = 0;
    double base_rate_ = 0, extra_rate_ = 0, preparation_ms_ = 0, swap_ms_ = 0;
};

}  // namespace sma3
