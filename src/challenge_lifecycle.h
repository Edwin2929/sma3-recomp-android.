#pragma once

namespace sma3 {

// Game-thread lifecycle, called only at a stopped dispatch boundary. The
// checkpoint belongs to an automatic suspend, never to a player save slot.
void local_achievements_checkpoint_path(const char* path) noexcept;
void local_achievements_suspend(const char* state_path, bool saved) noexcept;
// False means the previous proof could not be consumed durably. The caller
// must stop before any further guest instruction, so an I/O failure cannot
// make a later launch reuse a proof after unrecorded gameplay.
bool local_achievements_resume(const char* state_path) noexcept;
bool local_achievements_live() noexcept;
void local_achievements_untrusted_restore() noexcept;

} // namespace sma3
