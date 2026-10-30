#include "local_achievements.h"
#include "local_achievements_detail.h"

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
LocalAchievementsSnapshot published{{1, 0, 0, 0, 0, 0, 0, 0}};
std::mutex published_mutex;
const gba::GbaBus* previous_bus = nullptr;

#ifndef __ANDROID__
// Private host qualification only: compare the actual per-PPU-frame
// observations with TCP snapshots taken at a different execution boundary.
// Android does not contain this path or perform any per-frame file I/O.
void trace_observation(const gbarecomp::ExtendedViewFrameInfo& frame,
                       const local_achievements_detail::Observation& o,
                       const LocalAchievementsSnapshot& progress) noexcept {
    static std::FILE* output = [] {
        const auto* path = std::getenv("SMA3_LOCAL_ACHIEVEMENTS_TRACE");
        if (!path || !*path) return static_cast<std::FILE*>(nullptr);
        auto* file = std::fopen(path, "w");
        if (file) std::fprintf(file,
            "frame,epoch,valid,state,course,room,save_file,red,flowers,stars_tenths,"
            "transformation,victory_substate,score,fuzzy_timer,fuzzy_amplitude,"
            "fuzzy_active,completed,perfect,vehicles,max_red,max_flowers,max_stars,flags\n");
        return file;
    }();
    if (!output) return;
    std::fprintf(output,
        "%llu,%llu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,"
        "%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",
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
        static_cast<unsigned long long>(progress[7]));
    std::fflush(output);
}
#endif
}  // namespace

void local_achievements_frame(const gbarecomp::ExtendedViewFrameInfo* frame) noexcept {
    const auto* bus = gbarecomp::active_bus();
    if (!frame || !bus) {
        tracker.invalidate();
        previous_bus = nullptr;
        return;
    }
    if (previous_bus != bus) {
        tracker.invalidate();
        previous_bus = bus;
    }
    const auto observation = local_achievements_detail::observe(bus->iwram_ptr(), 0x8000,
        local_achievements_detail::read16(bus->io().raw()));
    if (tracker.frame(observation, frame->frame_count, frame->state_epoch)) {
        // No lock or allocation on unchanged frames. The brief copy is also
        // synchronized with readers, so perfect/completed masks cannot tear.
        const std::lock_guard<std::mutex> lock(published_mutex);
        published = tracker.progress();
    }
#ifndef __ANDROID__
    trace_observation(*frame, observation, tracker.progress());
#endif
}

LocalAchievementsSnapshot local_achievements_snapshot() noexcept {
    const std::lock_guard<std::mutex> lock(published_mutex);
    return published;
}

}  // namespace sma3
