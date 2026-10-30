#pragma once

#include "local_achievements_detail.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sma3::no_mario_loss {

// This is an observed campaign, not a collection of independently clean levels.
// The record lives outside guest RAM and is never rewound by a guest savestate.
enum class Status : std::uint64_t { NotStarted, Active, MarioLost, Unverifiable, Earned };
constexpr std::uint64_t kMainLevels = (std::uint64_t(1) << 48) - 1;
constexpr int main_index(unsigned course) noexcept {
    return course < 72 && course % 12 < 8 ? int(course / 12 * 8 + course % 12) : -1;
}

struct Observation {
    bool valid = false, allocated = false, fresh_file = false;
    unsigned state = 0, course = 0, room = 0, slot = 0;
    unsigned mario_flags = 0, mario_active = 0, mario_state = 0, transformation = 0;
    unsigned victory_substate = 0;
};

inline Observation observe(const std::uint8_t* ram, std::size_t size) noexcept {
    using local_achievements_detail::read16;
    using local_achievements_detail::read32;
    Observation o;
    if (!ram || size < 0x8000 || ram[0x6d64] != 0 || ram[0x6b05] > 0x4c ||
            ram[0x6c39] > 2) return o;
    o.valid = true;
    o.state = ram[0x6b05]; o.slot = ram[0x6c39];
    o.course = read16(ram + 0x6288);
    o.allocated = read32(ram + 0x7010) == 0x0200000c &&
                  read32(ram + 0x7240) == 0x0300220c;
    if (o.allocated) {
        o.room = read16(ram + 0x4cb8);
        o.mario_flags = read16(ram + 0x6f2a);
        o.mario_active = read16(ram + 0x2484);
        o.mario_state = read16(ram + 0x24ce);
        o.transformation = read16(ram + 0x6db2);
        o.victory_substate = read16(ram + 0x4828);
    }
    // InitSaveFileWRAM (080FA098): AYE0, no existing file, no deaths or
    // cleared courses; 1-1 alone is unlocked. Only inspected in the new-file
    // title/intro transition, so its 72 words are not scanned during play.
    if (o.state == 0x3a || o.state == 0x3b) {
        o.fresh_file = read32(ram + 0x6278) == 0x30455941 && ram[0x6350] == 0 &&
                       read16(ram + 0x634c) == 0 && read16(ram + 0x62a6) == 0x8000;
        for (unsigned i = 1; o.fresh_file && i < 72; ++i)
            o.fresh_file = read16(ram + 0x62a6 + i * 2) == 0;
    }
    return o;
}

inline bool lost_mario(const Observation& o) noexcept {
    if (!o.valid || !o.allocated || o.state != 0x0d ||
            o.room > 255 || (o.course != 11 && local_achievements_detail::course_index(o.course) < 0)) return false;
    // Bit 14 is a Bandit's capture (080DFF5C). A normal knock-away at
    // 080E1DC4 leaves sprite 061 active (8), in state 0, without bit 15.
    // States 3 and 9..14 are the resulting bubble/capture/recovery paths.
    // Vehicle bubbles (state 6), Superstar and scripted transfers are not
    // damage. In particular a vehicle's last frame can have transform=0
    // while Mario is still in state 6: checking the flag alone is incorrect.
    if (o.mario_flags & 0x4000) return true;
    if (o.transformation != 0 || (o.mario_flags & 0x8000)) return false;
    // SuperBabyEgg_Main reconstructs Mario after Super Star with flags 2000
    // and state 0. Generic sprite init later sets active=8 before he reaches
    // state 1 and reattaches. This executed scripted return is not damage.
    // Normal reattachment (080E049A..04A6) clears 2000 before setting 8000,
    // so a subsequent real knock-away remains detectable. Capture and the
    // other damage/bubble states are deliberately not exempted by this bit.
    if ((o.mario_flags & 0x2000) && o.mario_state == 0) return false;
    return o.mario_active == 8 &&
           (o.mario_state == 0 || o.mario_state == 3 ||
            (o.mario_state >= 9 && o.mario_state <= 14));
}

class Tracker {
public:
    Status status() const noexcept { return status_; }
    std::array<std::uint64_t, 4> summary() const noexcept {
        unsigned count = 0;
        for (auto bits = completed_; bits; bits &= bits - 1) ++count;
        return {{1, std::uint64_t(status_), slot_ < 3 ? slot_ + 1 : 0, count}};
    }
    void invalidate() noexcept {
        if (status_ == Status::Active) status_ = Status::Unverifiable;
        have_clock_ = false; previous_ = {}; trusted_resume_ = false;
        boss_victory_ = false;
    }
    // Called at an engine checkpoint boundary too: a hit after the most
    // recent PPU-frame observation must not be omitted from a suspend record.
    void observe_loss(const Observation& o) noexcept {
        if (status_ == Status::Active && o.slot == slot_ && lost_mario(o))
            status_ = Status::MarioLost;
    }
    bool frame(const Observation& o, std::uint64_t frame, std::uint64_t epoch) noexcept {
        const auto before = summary();
        const bool resume = trusted_resume_;
        trusted_resume_ = false;
        if (have_clock_ && frame == frame_ && (epoch == epoch_ || resume)) {
            epoch_ = epoch;
            observe_loss(o);
            return before != summary();
        }
        const bool continuous = have_clock_ && (epoch == epoch_ || resume) &&
            frame_ != UINT64_MAX && frame == frame_ + 1;
        if (have_clock_ && !continuous && status_ == Status::Active)
            status_ = Status::Unverifiable;
        // Observe the title launching a genuinely empty file. A loaded intro,
        // imported 1-1 save, repeated first level or a new zeroed tracker can
        // never arm an attempt simply by displaying pristine game RAM.
        const bool new_file = continuous && previous_.valid && previous_.state == 8 &&
            o.valid && (o.state == 0x3a || o.state == 0x3b) && o.fresh_file;
        if (new_file && status_ != Status::Earned) {
            status_ = Status::Active; slot_ = o.slot; completed_ = 0;
            saw_intro_ = false; saw_final_course_ = false; boss_victory_ = false;
        }
        if (status_ == Status::Active) {
            if (!o.valid) status_ = Status::Unverifiable;
            else if (o.state == 0x0d && (o.slot != slot_ || !o.allocated || o.room > 255 ||
                     (o.course != 11 && local_achievements_detail::course_index(o.course) < 0)))
                status_ = Status::Unverifiable;
            else {
                observe_loss(o);
                if (o.state == 0x0d && o.course == 11) saw_intro_ = true;
                if (o.state == 0x0d && o.course == 67) saw_final_course_ = true;
                const bool pair = continuous && previous_.valid && previous_.allocated &&
                    o.allocated && previous_.slot == slot_ && o.slot == slot_ &&
                    previous_.course == o.course;
                if (pair && previous_.state == 0x0d && o.state == 0x23) boss_victory_ = true;
                else if (o.state != 0x23 && o.state != 0x0e) boss_victory_ = false;
                if (pair && o.state == 0x0e &&
                        (previous_.state == 0x0d || (previous_.state == 0x23 && boss_victory_)) &&
                        o.victory_substate > 0 && o.victory_substate <= 0x19) {
                    const int bit = main_index(o.course);
                    if (bit >= 0) completed_ |= std::uint64_t(1) << bit;
                }
                // Final Bowser first uses 0D -> 23 -> 0E, just like the
                // other bosses (08107026, 080E5D86). Afterwards the 6-8
                // castle/title wrapper and fade use 16 -> 17 -> 18. The
                // world-map sequence can also enter credits from state 39,
                // or 4B when an all-scores unlock scene runs (0800C4BA,
                // 0800DDEE, 0800CB94). No allocation guard is
                // used here: the map deliberately frees its arena before
                // CreditsInit. The observed 48 goals and continuous final
                // course transition are required even if credits appear.
                if (status_ == Status::Active && continuous && saw_intro_ && saw_final_course_ &&
                        previous_.valid && previous_.slot == slot_ && o.slot == slot_ &&
                        previous_.course == 67 && o.course == 67 &&
                        (previous_.state == 0x17 || previous_.state == 0x39 || previous_.state == 0x4b) &&
                        (o.state == 0x18 || o.state == 0x19) &&
                        completed_ == kMainLevels) status_ = Status::Earned;
            }
        }
        frame_ = frame; epoch_ = epoch; have_clock_ = true; previous_ = o;
        return before != summary();
    }

    // Opaque, bounded, explicitly versioned payload. No pointer, padding,
    // guest data, native mutex or clock-dependent struct bytes are serialized.
    std::vector<std::uint64_t> checkpoint() const {
        return {1, std::uint64_t(status_), slot_, completed_, saw_intro_, saw_final_course_,
            boss_victory_, have_clock_, frame_, epoch_, previous_.valid, previous_.allocated,
            previous_.fresh_file, previous_.state, previous_.course, previous_.room,
            previous_.slot, previous_.mario_flags, previous_.mario_active, previous_.mario_state,
            previous_.transformation, previous_.victory_substate};
    }
    bool restore(const std::vector<std::uint64_t>& p) noexcept {
        if (p.size() != 22 || p[0] != 1 || p[1] > 4 || p[2] > 3 ||
                (p[3] & ~kMainLevels) || p[4] > 1 || p[5] > 1 || p[6] > 1 || p[7] > 1 ||
                p[10] > 1 || p[11] > 1 || p[12] > 1 || p[13] > 0x4c ||
                p[14] > 65535 || p[15] > 65535 || p[16] > 2 || p[17] > 65535 ||
                p[18] > 65535 || p[19] > 65535 || p[20] > 65535 || p[21] > 65535 ||
                (p[1] == 0 && (p[2] != 3 || p[3] || p[4] || p[5] || p[6])) ||
                (p[1] != 0 && p[2] == 3) ||
                (p[1] == 4 && (p[3] != kMainLevels || !p[4] || !p[5])) ||
                (p[1] == 1 && !p[7])) return false;
        status_ = Status(p[1]); slot_ = unsigned(p[2]); completed_ = p[3];
        saw_intro_ = p[4]; saw_final_course_ = p[5]; boss_victory_ = p[6];
        have_clock_ = p[7]; frame_ = p[8]; epoch_ = p[9];
        previous_ = {bool(p[10]), bool(p[11]), bool(p[12]), unsigned(p[13]), unsigned(p[14]),
            unsigned(p[15]), unsigned(p[16]), unsigned(p[17]), unsigned(p[18]), unsigned(p[19]),
            unsigned(p[20]), unsigned(p[21])};
        trusted_resume_ = true;
        return true;
    }
private:
    Status status_ = Status::NotStarted;
    unsigned slot_ = 3;
    std::uint64_t completed_ = 0, frame_ = 0, epoch_ = 0;
    bool saw_intro_ = false, saw_final_course_ = false, boss_victory_ = false;
    bool have_clock_ = false, trusted_resume_ = false;
    Observation previous_;
};

}  // namespace sma3::no_mario_loss
