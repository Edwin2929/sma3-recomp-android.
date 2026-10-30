#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace gbarecomp { struct ExtendedViewFrameInfo; }

namespace sma3 {

// Schema 2: version, completed-level mask, perfect-level mask, vehicle mask,
// maximum red coins, maximum flowers, maximum whole stars, event flags.
// Levels use 60 compact bits: world * 10 + course (eight main + secret/extra).
// Vehicles use bits 0..4 for game transformation IDs 2, 4, 6, 8 and 12.
// Event bit 0 means an observed, naturally completed Fuzzy effect.
// Event bit 1 means a verified entire campaign without losing Baby Mario.
using LocalAchievementsSnapshot = std::array<std::uint64_t, 8>;

// Called only by the game thread, once per guest frame. Never writes guest RAM.
void local_achievements_frame(const gbarecomp::ExtendedViewFrameInfo* frame) noexcept;

// Coherent, non-destructive snapshot for Android or host qualification.
// Android merges OR/max with its durable local progress; no historical game
// save or savestate is imported as a newly earned achievement.
LocalAchievementsSnapshot local_achievements_snapshot() noexcept;

// Game-thread-only checkpoint operations, used by the trusted suspend
// lifecycle. A user savestate/rewind invalidates the live campaign instead.
std::vector<std::uint64_t> local_achievements_challenge_checkpoint();
bool local_achievements_challenge_restore(const std::vector<std::uint64_t>& payload) noexcept;
void local_achievements_challenge_invalidate() noexcept;

// Schema, status (0 not started / 1 active / 2 lost / 3 unverifiable / 4 earned),
// save slot + 1 (zero if none), and distinct main courses completed out of 48.
std::array<std::uint64_t, 4> local_achievements_challenge_status() noexcept;

}  // namespace sma3
