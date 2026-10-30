#pragma once

#include <chrono>
#include <cstdint>

namespace sma3 {

// Owned, armed and polled by the SDL/guest thread only. Lifecycle changes are
// still checked by PresentationPacing before it renders. No clock or callback
// is needed while there is no optional image, and an armed image is serviced
// at its actual preparation deadline rather than on an unrelated 1 ms grid.
class CooperativePresentationPoll {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr std::uint32_t kTickDivider = 1024;

    void arm(Clock::time_point deadline, Clock::time_point observed_at) noexcept {
        deadline_ = deadline;
        observed_at_ = observed_at;
        armed_ = true;
    }
    void cancel() noexcept { armed_ = false; }
    bool armed() const noexcept { return armed_; }
    Clock::time_point deadline() const noexcept { return deadline_; }

    template<class Now, class Service>
    bool poll(Now now, Service service) {
        if (!armed_) return false;
        if ((++ticks_ & (kTickDivider - 1)) != 0) return false;
        const auto current = now();
        // A regressed clock must reach the scheduler too, so it can cancel
        // the old image instead of keeping a future-dated request alive.
        if (current >= observed_at_ && current < deadline_) return false;
        service(current);
        return true;
    }

private:
    Clock::time_point deadline_{}, observed_at_{};
    std::uint32_t ticks_ = 0;
    bool armed_ = false;
};

}  // namespace sma3
