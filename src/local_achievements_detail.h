#pragma once

#include "local_achievements.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace sma3::local_achievements_detail {

constexpr std::uint16_t read16(const std::uint8_t* p) noexcept {
    return std::uint16_t(p[0] | (unsigned(p[1]) << 8));
}
constexpr std::uint32_t read32(const std::uint8_t* p) noexcept {
    return std::uint32_t(read16(p)) | (std::uint32_t(read16(p + 2)) << 16);
}

constexpr int course_index(unsigned id) noexcept {
    return id / 12 < 6 && id % 12 < 10 ? int((id / 12) * 10 + id % 12) : -1;
}
constexpr unsigned vehicle_bit(unsigned id) noexcept {
    switch (id) {
        case 2: return 1u;
        case 4: return 2u;
        case 6: return 4u;
        case 8: return 8u;
        case 12: return 16u;
        default: return 0u;
    }
}

struct Observation {
    bool valid = false;
    unsigned state = 0, course = 0, room = 0, save_file = 0;
    unsigned red = 0, flowers = 0, stars_tenths = 0, transformation = 0;
    unsigned victory_substate = 0, score = 0;
    unsigned fuzzy_timer = 0, fuzzy_amplitude = 0, fuzzy_active = 0;
};

// These addresses belong to the verified SMA3 USA normal-game allocation.
// The score screen reuses collectible counters while counting down; their
// ranges are deliberately checked only during ordinary gameplay, not here.
inline Observation observe(const std::uint8_t* ram, std::size_t size,
                           unsigned dispcnt) noexcept {
    Observation o;
    if (!ram || size < 0x8000 || (dispcnt & 7u) != 0 || ram[0x6d64] != 0 ||
            read32(ram + 0x7010) != 0x0200000c ||
            read32(ram + 0x7240) != 0x0300220c) return o;
    o.state = ram[0x6b05];
    o.course = read16(ram + 0x6288);
    o.room = read16(ram + 0x4cb8);
    o.save_file = ram[0x6c39];
    if (course_index(o.course) < 0 || o.room > 255 || o.save_file > 2 ||
            (o.state != 0x0d && o.state != 0x0e && o.state != 0x23)) return o;
    o.red = read16(ram + 0x6ad6);
    o.flowers = read16(ram + 0x6a9a);
    o.stars_tenths = read16(ram + 0x6ace);
    o.transformation = read16(ram + 0x6db2);
    o.victory_substate = read16(ram + 0x4828);
    o.score = read16(ram + 0x6aa4);
    o.fuzzy_timer = read16(ram + 0x4bd6);
    o.fuzzy_amplitude = read16(ram + 0x4ad6);
    o.fuzzy_active = read32(ram + 0x7294);
    o.valid = true;
    return o;
}

class Tracker {
public:
    const LocalAchievementsSnapshot& progress() const noexcept { return progress_; }

    void invalidate() noexcept {
        have_clock_ = false;
        have_previous_ = false;
        reset_sequence();
    }

    // Returns true only when public monotonic progress changes. A restore
    // epoch, repeated frame, backward clock or skipped observation cannot
    // turn already-present RAM values into a newly observed action.
    bool frame(const Observation& o, std::uint64_t frame_count,
               std::uint64_t state_epoch) noexcept {
        if (have_clock_ && frame_count == frame_count_ && state_epoch == epoch_)
            return false;
        const bool continuous = have_clock_ && state_epoch == epoch_ &&
            frame_count_ != UINT64_MAX && frame_count == frame_count_ + 1 &&
            have_previous_ && previous_.valid && o.valid &&
            previous_.course == o.course && previous_.save_file == o.save_file;
        have_clock_ = true;
        frame_count_ = frame_count;
        epoch_ = state_epoch;
        if (!continuous) {
            reset_sequence();
            previous_ = o;
            have_previous_ = o.valid;
            return false;
        }
        const auto before = progress_;
        const bool normal_pair = previous_.state == 0x0d && o.state == 0x0d &&
                                 previous_.room == o.room;
        if (normal_pair) {
            if (o.red <= 20 && previous_.red <= 20 && o.red > previous_.red)
                progress_[4] = std::max(progress_[4], std::uint64_t(o.red));
            if (o.flowers <= 5 && previous_.flowers <= 5 && o.flowers > previous_.flowers)
                progress_[5] = std::max(progress_[5], std::uint64_t(o.flowers));
            if (o.stars_tenths <= 300 && previous_.stars_tenths <= 300 &&
                    o.stars_tenths > previous_.stars_tenths)
                progress_[6] = std::max(progress_[6], std::uint64_t(o.stars_tenths / 10));
            if (previous_.transformation == 0)
                progress_[3] |= vehicle_bit(o.transformation);
            update_fuzzy(previous_, o);
        } else {
            reset_fuzzy();
        }

        // Ordinary goals enter 0E from 0D. Bosses first enter the explicitly
        // documented BossVictoryTransition (23), then 0E at substate 19.
        if (previous_.state == 0x0d && o.state == 0x23) boss_victory_ = true;
        else if (o.state != 0x23 && o.state != 0x0e) boss_victory_ = false;
        const bool entered_goal = o.state == 0x0e &&
            (previous_.state == 0x0d || (previous_.state == 0x23 && boss_victory_)) &&
            o.victory_substate > 0 && o.victory_substate <= 0x19;
        if (entered_goal) {
            progress_[1] |= std::uint64_t(1) << course_index(o.course);
            victory_ = true;
        } else if (o.state != 0x0e || o.victory_substate == 0 ||
                   o.victory_substate > 0x54 ||
                   (previous_.state == 0x0e && o.victory_substate < previous_.victory_substate)) {
            victory_ = false;
        }
        // The game commits the CURRENT result after acknowledging score
        // substate 1E (stored value 3D/3E -> 3F/40). A stale high-score table
        // or a loaded score screen can never award a perfect course.
        if (victory_ && previous_.state == 0x0e && o.state == 0x0e &&
                (previous_.victory_substate == 0x3d || previous_.victory_substate == 0x3e) &&
                (o.victory_substate == 0x3f || o.victory_substate == 0x40) && o.score == 100)
            progress_[2] |= std::uint64_t(1) << course_index(o.course);
        previous_ = o;
        have_previous_ = true;
        return before != progress_;
    }

private:
    static bool normal_fuzzy(const Observation& o) noexcept {
        return o.fuzzy_timer == 0 && o.fuzzy_amplitude == 0 && o.fuzzy_active == 0;
    }
    void reset_fuzzy() noexcept {
        fuzzy_started_ = false;
        fuzzy_countdown_held_ = false;
        fuzzy_countdown_ = fuzzy_growth_ = fuzzy_fade_ = 0;
    }
    void reset_sequence() noexcept {
        boss_victory_ = false;
        victory_ = false;
        reset_fuzzy();
    }
    void update_fuzzy(const Observation& old, const Observation& now) noexcept {
        if (old.fuzzy_active > 1 || now.fuzzy_active > 1 ||
                old.fuzzy_timer > 0xffff || now.fuzzy_timer > 0xffff ||
                old.fuzzy_amplitude > 0xffff || now.fuzzy_amplitude > 0xffff ||
                old.transformation != 0 || now.transformation != 0) {
            reset_fuzzy();
            return;
        }
        if (!fuzzy_started_) {
            if (normal_fuzzy(old) && now.fuzzy_timer > 1) fuzzy_started_ = true;
            else return;
        }
        if (old.fuzzy_active == 1) {
            if (old.fuzzy_timer > 1) {
                // A frame-start sample can precede one delayed game update:
                // native Fuzzy replay shows 652,652,650 while the PPU clock
                // remains consecutive. Permit that one catch-up update only
                // after observing the held countdown; never a jump to zero.
                const unsigned allowance = fuzzy_countdown_held_ ? 2u : 1u;
                if (now.fuzzy_timer == 0 ||
                        (now.fuzzy_timer < old.fuzzy_timer &&
                         old.fuzzy_timer - now.fuzzy_timer > allowance)) {
                    reset_fuzzy();
                    return;
                }
                if (now.fuzzy_timer < old.fuzzy_timer)
                    fuzzy_countdown_ = std::min(2u, fuzzy_countdown_ +
                                               old.fuzzy_timer - now.fuzzy_timer);
                fuzzy_countdown_held_ = now.fuzzy_timer == old.fuzzy_timer;
            } else {
                fuzzy_countdown_held_ = false;
            }
            if (now.fuzzy_active == 1 && now.fuzzy_amplitude > old.fuzzy_amplitude)
                fuzzy_growth_ = 1;
            if (old.fuzzy_timer == 1 && now.fuzzy_timer <= 1) {
                const auto expected = old.fuzzy_amplitude > 0x100 ?
                    old.fuzzy_amplitude - 0x100 : 0;
                if (now.fuzzy_amplitude != old.fuzzy_amplitude && now.fuzzy_amplitude != expected) {
                    reset_fuzzy();
                    return;
                }
                if (now.fuzzy_amplitude < old.fuzzy_amplitude)
                    fuzzy_fade_ = std::min(2u, fuzzy_fade_ + 1);
            }
            if (now.fuzzy_active == 0) {
                if (old.fuzzy_timer == 1 && old.fuzzy_amplitude > 0 &&
                        old.fuzzy_amplitude <= 0x100 && normal_fuzzy(now) &&
                        fuzzy_countdown_ >= 2 && fuzzy_growth_ && fuzzy_fade_ >= 2)
                    progress_[7] |= 1;
                reset_fuzzy();
            }
        } else if (normal_fuzzy(now)) {
            // A canceled contact before the visible effect is not recovery.
            reset_fuzzy();
        }
    }

    LocalAchievementsSnapshot progress_{{1, 0, 0, 0, 0, 0, 0, 0}};
    Observation previous_;
    std::uint64_t frame_count_ = 0, epoch_ = 0;
    unsigned fuzzy_countdown_ = 0, fuzzy_growth_ = 0, fuzzy_fade_ = 0;
    bool have_clock_ = false, have_previous_ = false;
    bool boss_victory_ = false, victory_ = false, fuzzy_started_ = false;
    bool fuzzy_countdown_held_ = false;
};

}  // namespace sma3::local_achievements_detail
