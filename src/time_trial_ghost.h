#pragma once

#include "ghost_visual.h"
#include "time_trial.h"

#include <array>
#include <cstdint>

namespace sma3 {

enum class GhostStorage : std::uint64_t {
    NotConfigured = 0, Loading = 1, Ready = 2, Saving = 3,
    ReadError = 4, WriteError = 5
};
enum class GhostReason : std::uint64_t {
    None = 0, AwaitingBestSync = 1, AwaitingFullStart = 2,
    MissingGhostForBest = 3, Limit = 4, CaptureUnavailable = 5,
    Cancelled = 6, BufferUnavailable = 7, InvalidBestSync = 8
};

// Schema 1, 72 words (independent of TimeTrialSnapshot's unchanged 74 words):
// 0 schema, 1 enabled, 2 bests ready, 3 recording, 4 playing,
// 5 GhostStorage, 6 GhostReason, 7 current raw course+1 (0=none),
// 8 recorded samples, 9 selected playback frames, 10 pending saves,
// 11 loaded raw course+1 (0=none), 12..71 eligible saved ghost durations.
// Eligibility requires an exact, nonzero match with Java's durable/current
// personal best. Historical records without motion never invent a replay.
using TimeTrialGhostSnapshot = std::array<std::uint64_t, 72>;
using TimeTrialGhostBests = std::array<std::uint64_t, 60>;

// Configure the protected directory once; repeated calls for the same path
// are idempotent. All disk work is performed by the serial worker.
bool time_trial_ghost_initialize(const char* directory) noexcept;
bool time_trial_ghost_enabled() noexcept;
void time_trial_ghost_set_enabled(bool enabled) noexcept;
// Zero means no record. Once synchronized, nonzero values may only improve;
// zero may become a record but an existing record cannot be erased here.
bool time_trial_ghost_set_bests(const TimeTrialGhostBests& bests) noexcept;
void time_trial_ghost_frame(const TimeTrialSnapshot& timer,
                          const GhostVisualSample& sample) noexcept;
bool time_trial_ghost_playback(GhostVisualSample& output) noexcept;
TimeTrialGhostSnapshot time_trial_ghost_snapshot() noexcept;
// Call after the guest loop has stopped, never from Android's UI callbacks.
void time_trial_ghost_shutdown() noexcept;

}  // namespace sma3
