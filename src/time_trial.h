#pragma once

#include <array>
#include <cstdint>

namespace gbarecomp { struct ExtendedViewFrameInfo; }

namespace sma3 {

enum class TimeTrialStatus : std::uint64_t {
    Disabled = 0, Waiting = 1, Running = 2, Paused = 3, Finished = 4, Invalidated = 5
};
enum class TimeTrialReason : std::uint64_t {
    None = 0, Death = 1, LeftLevel = 2, StateRestored = 3,
    ClockDiscontinuity = 4, UnsupportedScene = 5, SourceChanged = 6
};

// Schema 1, 74 words:
//  0 version, 1 enabled, 2 TimeTrialStatus, 3 raw course + 1 (zero = none),
//  4 elapsed PPU-frame intervals, 5 attempt generation, 6 completion sequence,
//  7 last completed raw course + 1, 8 last completed frame intervals,
//  9 TimeTrialReason, 10 state epoch, 11 observed PPU frame,
// 12 cycles/frame (280896), 13 clock Hz (16777216),
// 14..73 minimum completed frame intervals per compact course, zero if none.
// Compact course = world * 10 + stage; stage 0..7 main, 8 Extra, 9 Secret.
// The welcome course and Mario Bros. are not timed. Session minima are merged
// into durable Android records by the UI; no guest frame performs file I/O.
using TimeTrialSnapshot = std::array<std::uint64_t, 74>;

// All three entry points are synchronized. Reapplying the same setting does
// not reset a race. Enabling mid-course waits for a new main-entrance intro.
void time_trial_set_enabled(bool enabled) noexcept;
void time_trial_frame(const gbarecomp::ExtendedViewFrameInfo* frame) noexcept;
TimeTrialSnapshot time_trial_snapshot() noexcept;

}  // namespace sma3
