#include "challenge_lifecycle.h"
#include "challenge_checkpoint.h"
#include "local_achievements.h"

#include <string>
#include <vector>

namespace sma3 {
namespace {
std::string checkpoint_path;
}

void local_achievements_checkpoint_path(const char* path) noexcept {
    try { checkpoint_path = path ? path : ""; }
    catch (...) { checkpoint_path.clear(); }
}

void local_achievements_untrusted_restore() noexcept {
    local_achievements_challenge_invalidate();
    if (!checkpoint_path.empty()) challenge_checkpoint::discard(checkpoint_path);
}

void local_achievements_suspend(const char* state_path, bool saved) noexcept {
    if (checkpoint_path.empty()) return;
    try {
        if (!saved || !state_path || !challenge_checkpoint::save(checkpoint_path, state_path,
                local_achievements_challenge_checkpoint())) {
            local_achievements_untrusted_restore();
        }
    } catch (...) { local_achievements_untrusted_restore(); }
}

bool local_achievements_resume(const char* state_path) noexcept {
    if (checkpoint_path.empty()) { local_achievements_challenge_invalidate(); return true; }
    try {
        std::vector<std::uint64_t> payload;
        if (!state_path) { local_achievements_challenge_invalidate(); return local_achievements_live(); }
        const auto result = challenge_checkpoint::consume(checkpoint_path, state_path, payload);
        if (result != challenge_checkpoint::ReadResult::Restored ||
                !local_achievements_challenge_restore(payload))
            local_achievements_challenge_invalidate();
        return result != challenge_checkpoint::ReadResult::IoError;
    } catch (...) { local_achievements_challenge_invalidate(); return false; }
}

bool local_achievements_live() noexcept {
    // Consume the background proof before executing another game action.
    // If Android later kills this foreground process, its earlier automatic
    // state is no longer proof of an uninterrupted attempt.
    if (!checkpoint_path.empty() && !challenge_checkpoint::discard(checkpoint_path)) {
        local_achievements_challenge_invalidate();
        return false;
    }
    return true;
}

} // namespace sma3
