#include "time_trial.h"
#include "time_trial_detail.h"

#include "gba_bus.h"
#include "runtime.h"
#include "runtime_bus_bridge.h"

#include <mutex>

namespace sma3 {
namespace {
time_trial_detail::Tracker tracker;
std::mutex tracker_mutex;
const gba::GbaBus* previous_bus = nullptr;
}  // namespace

void time_trial_set_enabled(bool enabled) noexcept {
    const std::lock_guard<std::mutex> lock(tracker_mutex);
    tracker.set_enabled(enabled);
}

void time_trial_frame(const gbarecomp::ExtendedViewFrameInfo* frame) noexcept {
    const auto* bus = gbarecomp::active_bus();
    const auto observed = bus ? time_trial_detail::observe(bus->iwram_ptr(), 0x8000)
                              : time_trial_detail::Observation{};
    // No allocation, guest writes, timers based on presentation, or disk I/O.
    const std::lock_guard<std::mutex> lock(tracker_mutex);
    if (!frame || !bus) {
        tracker.invalidate(TimeTrialReason::SourceChanged);
        previous_bus = nullptr;
        return;
    }
    if (previous_bus && previous_bus != bus) tracker.invalidate(TimeTrialReason::SourceChanged);
    previous_bus = bus;
    tracker.frame(observed, frame->frame_count, frame->state_epoch);
}

TimeTrialSnapshot time_trial_snapshot() noexcept {
    const std::lock_guard<std::mutex> lock(tracker_mutex);
    return tracker.snapshot();
}

}  // namespace sma3
