#pragma once

#include "time_trial.h"
#include "local_achievements_detail.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace sma3::time_trial_detail {

using Status = TimeTrialStatus;
using Reason = TimeTrialReason;

struct Observation {
    bool valid = false, allocated = false, paused = false;
    unsigned state = 0, course = 0, room = 0, slot = 0, victory_substate = 0;
};

inline Observation observe(const std::uint8_t* ram, std::size_t size) noexcept {
    using local_achievements_detail::read16;
    using local_achievements_detail::read32;
    Observation o;
    // MainLoopStart selects YI with this mode byte (08002AE0/08002B0C).
    // Allocation checks precede every read from the gameplay arena.
    if (!ram || size < 0x8000 || ram[0x6d64] != 0 ||
            ram[0x6b05] > 0x4c || ram[0x6c39] > 2) return o;
    o.valid = true;
    o.state = ram[0x6b05];
    o.course = read16(ram + 0x6288);
    o.slot = ram[0x6c39];
    o.allocated = read32(ram + 0x7010) == 0x0200000c &&
                  read32(ram + 0x7240) == 0x0300220c;
    if (o.allocated) {
        o.room = read16(ram + 0x4cb8);
        o.victory_substate = read16(ram + 0x4828);
        // Inventory: arena + 26A9 (080E7F2A, 080E7FA8).
        // Start menu: low nibble of arena + 2B3A (080FB026/080FB118).
        // Both are handled before ordinary gameplay at 0802D230.
        o.paused = ram[0x48b5] != 0 || (ram[0x4d46] & 0x0f) != 0;
    }
    return o;
}

inline int course_index(unsigned course) noexcept {
    return local_achievements_detail::course_index(course);
}

class Tracker {
public:
    Tracker() noexcept {
        data_[0] = 1;
        data_[12] = 280896;
        data_[13] = 16777216;
    }
    const TimeTrialSnapshot& snapshot() const noexcept { return data_; }
    void set_enabled(bool enabled) noexcept {
        if (enabled == (data_[1] != 0)) return;
        data_[1] = enabled;
        data_[2] = std::uint64_t(enabled ? Status::Waiting : Status::Disabled);
        data_[3] = data_[4] = data_[9] = 0;
        clear_sequence();
    }
    void invalidate(Reason reason) noexcept {
        if (data_[1]) {
            data_[2] = std::uint64_t(Status::Invalidated);
            data_[9] = std::uint64_t(reason);
        }
        clear_sequence();
    }
    void frame(const Observation& o, std::uint64_t frame, std::uint64_t epoch) noexcept {
        if (have_clock_ && frame == data_[11] && epoch == data_[10]) return;
        const bool continuous = have_clock_ && epoch == data_[10] &&
            data_[11] != std::numeric_limits<std::uint64_t>::max() && frame == data_[11] + 1;
        if (have_clock_ && !continuous) {
            invalidate(epoch != data_[10] ? Reason::StateRestored : Reason::ClockDiscontinuity);
        }
        data_[10] = epoch;
        data_[11] = frame;
        if (!data_[1]) { clear_sequence(); return; }

        if (!o.valid) {
            if (active() || intro_armed_) invalidate(Reason::UnsupportedScene);
            remember(o);
            return;
        }

        // Only the main-entrance arm of NormalGameplayInit writes 0C
        // (0802CC5A/5C). Screen exits and Middle Rings write 0B (0802CE8E).
        // Require a witnessed transition INTO 0C before its 0D transition:
        // enabling or loading a state already in an intro cannot arm a race.
        if (continuous && previous_.valid && previous_.slot == o.slot &&
                o.state == 0x0c && previous_.state != 0x0c &&
                intro_predecessor(previous_.state) && gameplay_arena(o)) {
            intro_armed_ = true;
            intro_course_ = o.course;
            intro_slot_ = o.slot;
            data_[2] = std::uint64_t(Status::Waiting);
            data_[3] = o.course + 1;
            data_[4] = data_[9] = 0;
            boss_victory_ = false;
            remember(o);
            return;
        }
        if (intro_armed_) {
            if (continuous && previous_.state == 0x0c && o.state == 0x0d &&
                    o.course == intro_course_ && o.slot == intro_slot_ && gameplay_arena(o)) {
                data_[2] = std::uint64_t(o.paused ? Status::Paused : Status::Running);
                data_[3] = o.course + 1;
                data_[4] = data_[9] = 0;
                ++data_[5];
                active_slot_ = o.slot;
                intro_armed_ = false;
                remember(o);
                return;
            }
            if (!continuous || o.state != 0x0c || o.course != intro_course_ ||
                    o.slot != intro_slot_ || !gameplay_arena(o)) {
                invalidate(Reason::UnsupportedScene);
            }
            remember(o);
            return;
        }

        if (!active()) { remember(o); return; }
        if (!continuous || !previous_.valid) {
            invalidate(Reason::ClockDiscontinuity);
        } else if (o.course + 1 != data_[3] || o.slot != active_slot_) {
            invalidate(Reason::LeftLevel);
        } else if (death_state(o.state)) {
            invalidate(Reason::Death);
        } else if (!timed_state(o.state)) {
            invalidate(Reason::LeftLevel);
        } else if ((o.state == 0x0d || o.state == 0x0e || o.state == 0x23) && !gameplay_arena(o)) {
            invalidate(Reason::UnsupportedScene);
        } else {
            // An interval belongs to the state at its leading PPU sample.
            // This counts the frame that opens a menu and excludes the frame
            // that leaves it; held pauses and duplicate presentations add 0.
            if (!(previous_.state == 0x0d && previous_.paused)) {
                if (data_[4] == std::numeric_limits<std::uint64_t>::max()) {
                    invalidate(Reason::ClockDiscontinuity);
                    remember(o);
                    return;
                }
                ++data_[4];
            }
            if (previous_.state == 0x0d && o.state == 0x23) boss_victory_ = true;
            else if (o.state != 0x23 && o.state != 0x0e) boss_victory_ = false;
            if (o.state == 0x0e) {
                const bool goal = (previous_.state == 0x0d ||
                    (previous_.state == 0x23 && boss_victory_)) &&
                    o.victory_substate > 0 && o.victory_substate <= 0x19 && data_[4] > 0;
                if (goal) complete(o.course);
                else invalidate(Reason::UnsupportedScene);
            } else {
                data_[2] = std::uint64_t(o.state == 0x0d && o.paused ? Status::Paused : Status::Running);
            }
        }
        remember(o);
    }

private:
    bool active() const noexcept {
        return data_[2] == std::uint64_t(Status::Running) || data_[2] == std::uint64_t(Status::Paused);
    }
    static bool gameplay_arena(const Observation& o) noexcept {
        return o.allocated && o.room <= 255 && course_index(o.course) >= 0;
    }
    static bool intro_predecessor(unsigned state) noexcept {
        // The load can span a sampled frame (0A), or finish between samples
        // after its fade/map/retry wrapper. None are ordinary gameplay or a
        // screen-exit transition (0B).
        return state == 0x09 || state == 0x0a || state == 0x35 || state == 0x36 ||
               state == 0x3c || state == 0x27 || state == 0x2b;
    }
    static bool death_state(unsigned state) noexcept {
        return state == 0x0f || (state >= 0x10 && state <= 0x13) ||
               (state >= 0x24 && state <= 0x2d);
    }
    static bool timed_state(unsigned state) noexcept {
        // A course includes room loads and its optional Bandit mini-game.
        return (state >= 0x09 && state <= 0x0b) || state == 0x0d || state == 0x0e ||
               (state >= 0x1f && state <= 0x23);
    }
    void complete(unsigned course) noexcept {
        const auto index = 14 + course_index(course);
        if (!data_[index] || data_[4] < data_[index]) data_[index] = data_[4];
        ++data_[6];
        data_[7] = course + 1;
        data_[8] = data_[4];
        data_[2] = std::uint64_t(Status::Finished);
        data_[9] = 0;
        boss_victory_ = false;
    }
    void remember(const Observation& o) noexcept { previous_ = o; have_clock_ = true; }
    void clear_sequence() noexcept {
        have_clock_ = false;
        previous_ = {};
        intro_armed_ = false;
        boss_victory_ = false;
    }

    TimeTrialSnapshot data_{};
    Observation previous_{};
    bool have_clock_ = false, intro_armed_ = false, boss_victory_ = false;
    unsigned intro_course_ = 0, intro_slot_ = 0, active_slot_ = 0;
};

}  // namespace sma3::time_trial_detail
