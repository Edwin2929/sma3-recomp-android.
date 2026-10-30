#pragma once

#include <array>
#include <cstdint>

namespace gbarecomp { struct ExtendedViewFrameInfo; }

namespace sma3 {

// Schema 1: version, completed-level mask, perfect-level mask, vehicle mask,
// maximum red coins, maximum flowers, maximum whole stars, event flags.
// Levels use 60 compact bits: world * 10 + course (eight main + secret/extra).
// Vehicles use bits 0..4 for game transformation IDs 2, 4, 6, 8 and 12.
// Event bit 0 means an observed, naturally completed Fuzzy effect.
using LocalAchievementsSnapshot = std::array<std::uint64_t, 8>;

// Called only by the game thread, once per guest frame. Never writes guest RAM.
void local_achievements_frame(const gbarecomp::ExtendedViewFrameInfo* frame) noexcept;

// Coherent, non-destructive snapshot for Android or host qualification.
// Android merges OR/max with its durable local progress; no historical game
// save or savestate is imported as a newly earned achievement.
LocalAchievementsSnapshot local_achievements_snapshot() noexcept;

}  // namespace sma3
