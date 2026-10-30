#include "local_achievements.h"
#include "local_achievements_detail.h"
#include "no_mario_loss_challenge.h"

#include "gba_bus.h"
#include "runtime.h"
#include "runtime_bus_bridge.h"

#include <mutex>

#ifndef __ANDROID__
#include <cstdio>
#include <cstdlib>
#endif

namespace sma3 {
namespace {
local_achievements_detail::Tracker tracker;
no_mario_loss::Tracker challenge;
LocalAchievementsSnapshot published{{2, 0, 0, 0, 0, 0, 0, 0}};
std::array<std::uint64_t, 4> published_challenge{{1, 0, 0, 0}};
std::mutex published_mutex;
const gba::GbaBus* previous_bus = nullptr;

void publish() noexcept {
    if (challenge.status() == no_mario_loss::Status::Earned) tracker.earned_no_mario_loss();
    const std::lock_guard<std::mutex> lock(published_mutex);
    published = tracker.progress();
    published_challenge = challenge.summary();
}

#ifndef __ANDROID__
// Private host qualification only: compare the actual per-PPU-frame
// observations with TCP snapshots taken at a different execution boundary.
// Android does not contain this path or perform any per-frame file I/O.
void trace_observation(const gbarecomp::ExtendedViewFrameInfo& frame,
                       const local_achievements_detail::Observation& o,
                       const no_mario_loss::Observation& campaign,
                       const LocalAchievementsSnapshot& progress) noexcept {
    static std::FILE* output = [] {
        const auto* path = std::getenv("SMA3_LOCAL_ACHIEVEMENTS_TRACE");
        if (!path || !*path) return static_cast<std::FILE*>(nullptr);
        auto* file = std::fopen(path, "w");
        if (file) std::fprintf(file,
            "frame,epoch,valid,state,course,room,save_file,red,flowers,stars_tenths,"
            "transformation,victory_substate,score,fuzzy_timer,fuzzy_amplitude,"
            "fuzzy_active,completed,perfect,vehicles,max_red,max_flowers,max_stars,flags,"
            "campaign_valid,campaign_state,campaign_course,campaign_slot,fresh_file,"
            "mario_flags,mario_active,mario_state,campaign_status,campaign_completed,"
            "campaign_allocated,campaign_room,campaign_transformation,campaign_victory_substate\n");
        return file;
    }();
    if (!output) return;
    std::fprintf(output,
        "%llu,%llu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,"
        "%llu,%llu,%llu,%llu,%llu,%llu,%llu,%u,%u,%u,%u,%u,%u,%u,%u,%llu,%llu,%u,%u,%u,%u\n",
        static_cast<unsigned long long>(frame.frame_count),
        static_cast<unsigned long long>(frame.state_epoch), unsigned(o.valid),
        o.state, o.course, o.room, o.save_file, o.red, o.flowers, o.stars_tenths,
        o.transformation, o.victory_substate, o.score,
        o.fuzzy_timer, o.fuzzy_amplitude, o.fuzzy_active,
        static_cast<unsigned long long>(progress[1]),
        static_cast<unsigned long long>(progress[2]),
        static_cast<unsigned long long>(progress[3]),
        static_cast<unsigned long long>(progress[4]),
        static_cast<unsigned long long>(progress[5]),
        static_cast<unsigned long long>(progress[6]),
        static_cast<unsigned long long>(progress[7]),
        unsigned(campaign.valid), campaign.state, campaign.course, campaign.slot,
        unsigned(campaign.fresh_file), campaign.mario_flags, campaign.mario_active, campaign.mario_state,
        static_cast<unsigned long long>(challenge.summary()[1]),
        static_cast<unsigned long long>(challenge.summary()[3]), unsigned(campaign.allocated),
        campaign.room, campaign.transformation, campaign.victory_substate);
    std::fflush(output);
}
#endif
}  // namespace

void local_achievements_frame(const gbarecomp::ExtendedViewFrameInfo* frame) noexcept {
    const auto* bus = gbarecomp::active_bus();
    if (!frame || !bus) {
        tracker.invalidate();
        challenge.invalidate();
        publish();
        previous_bus = nullptr;
        return;
    }
    if (previous_bus != bus) {
        tracker.invalidate();
        challenge.invalidate();
        previous_bus = bus;
    }
    const auto observation = local_achievements_detail::observe(bus->iwram_ptr(), 0x8000,
        local_achievements_detail::read16(bus->io().raw()));
    const auto campaign = no_mario_loss::observe(bus->iwram_ptr(), 0x8000);
    const bool challenge_changed = challenge.frame(campaign, frame->frame_count, frame->state_epoch);
    if (tracker.frame(observation, frame->frame_count, frame->state_epoch) || challenge_changed) {
        // No lock or allocation on unchanged frames. The brief copy is also
        // synchronized with readers, so perfect/completed masks cannot tear.
        publish();
    }
#ifndef __ANDROID__
    trace_observation(*frame, observation, campaign, tracker.progress());
#endif
}

LocalAchievementsSnapshot local_achievements_snapshot() noexcept {
    const std::lock_guard<std::mutex> lock(published_mutex);
    return published;
}

std::array<std::uint64_t, 4> local_achievements_challenge_status() noexcept {
    const std::lock_guard<std::mutex> lock(published_mutex);
    return published_challenge;
}

std::vector<std::uint64_t> local_achievements_challenge_checkpoint() {
    if (const auto* bus = gbarecomp::active_bus())
        challenge.observe_loss(no_mario_loss::observe(bus->iwram_ptr(), 0x8000));
    publish();
    auto payload = challenge.checkpoint();
    const auto& progress = tracker.progress();
    payload.insert(payload.end(), progress.begin(), progress.end());
    return payload;
}

bool local_achievements_challenge_restore(const std::vector<std::uint64_t>& payload) noexcept {
    if (payload.size() != 30) return false;
    LocalAchievementsSnapshot progress;
    std::copy(payload.begin() + 22, payload.end(), progress.begin());
    constexpr auto levels = (std::uint64_t(1) << 60) - 1;
    if (progress[0] != 2 || (progress[1] & ~levels) || (progress[2] & ~progress[1]) ||
            (progress[3] & ~std::uint64_t(31)) || progress[4] > 20 || progress[5] > 5 ||
            progress[6] > 30 || (progress[7] & ~std::uint64_t(3)) ||
            bool(progress[7] & 2) != (payload[1] == std::uint64_t(no_mario_loss::Status::Earned))) return false;
    try {
        const std::vector<std::uint64_t> campaign(payload.begin(), payload.begin() + 22);
        if (!challenge.restore(campaign)) return false;
    } catch (...) { return false; }
    tracker.invalidate();
    tracker.merge_restored_progress(progress);
    previous_bus = gbarecomp::active_bus();
    publish();
    return true;
}

void local_achievements_challenge_invalidate() noexcept {
    challenge.invalidate();
    publish();
}

}  // namespace sma3
