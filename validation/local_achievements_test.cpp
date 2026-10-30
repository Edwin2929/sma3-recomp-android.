#include "local_achievements_detail.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

using sma3::LocalAchievementsSnapshot;
using namespace sma3::local_achievements_detail;
namespace {
unsigned checks = 0, scenarios = 0;
void require(bool value, const char* message) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
const LocalAchievementsSnapshot empty{{1, 0, 0, 0, 0, 0, 0, 0}};
Observation normal(unsigned course = 0) {
    Observation o;
    o.valid = true; o.state = 0x0d; o.course = course; o.room = 0;
    o.stars_tenths = 100;
    return o;
}
struct Session {
    Tracker tracker;
    Observation o = normal();
    std::uint64_t frame = 0, epoch = 0;
    Session() { tracker.frame(o, frame, epoch); }
    bool step() { return tracker.frame(o, ++frame, epoch); }
    void restore() { ++epoch; tracker.frame(o, frame, epoch); }
};
void write16(std::uint8_t* p, unsigned v) { p[0] = v; p[1] = v >> 8; }
void write32(std::uint8_t* p, unsigned v) { write16(p, v); write16(p + 2, v >> 16); }

void observation_and_ids() {
    ++scenarios;
    std::array<std::uint8_t, 0x8000> ram{};
    write32(ram.data() + 0x7010, 0x0200000c);
    write32(ram.data() + 0x7240, 0x0300220c);
    ram[0x6b05] = 13; ram[0x6c39] = 2;
    write16(ram.data() + 0x6288, 69);
    write16(ram.data() + 0x4cb8, 255);
    write16(ram.data() + 0x6ad6, 19);
    write16(ram.data() + 0x6a9a, 4);
    write16(ram.data() + 0x6ace, 299);
    write16(ram.data() + 0x6db2, 12);
    write16(ram.data() + 0x4828, 0x3d);
    write16(ram.data() + 0x6aa4, 100);
    write16(ram.data() + 0x4bd6, 37);
    write16(ram.data() + 0x4ad6, 0x6000);
    write32(ram.data() + 0x7294, 1);
    const auto original = ram;
    auto o = observe(ram.data(), ram.size(), 0x1f00);
    require(o.valid && o.state == 13 && o.save_file == 2 && o.course == 69 && o.room == 255,
            "observation identity");
    require(o.red == 19 && o.flowers == 4 && o.stars_tenths == 299 && o.transformation == 12,
            "decimal star scale and counters");
    require(o.victory_substate == 0x3d && o.score == 100 && o.fuzzy_timer == 37 &&
            o.fuzzy_amplitude == 0x6000 && o.fuzzy_active == 1, "result/effect addresses");
    require(ram == original, "RAM remains byte-for-byte immutable");
    require(!observe(nullptr, ram.size(), 0).valid, "null RAM");
    for (unsigned size = 0; size < ram.size(); ++size)
        require(!observe(ram.data(), size, 0).valid, "short RAM rejected before reads");
    for (unsigned mode = 1; mode < 8; ++mode)
        require(!observe(ram.data(), ram.size(), mode).valid, "non-mode-zero graphics rejected");
    for (const auto offset : {0x7010, 0x7240}) {
        ram = original; write32(ram.data() + offset, 0);
        require(!observe(ram.data(), ram.size(), 0).valid, "unowned allocation rejected");
    }
    ram = original; ram[0x6d64] = 1;
    require(!observe(ram.data(), ram.size(), 0).valid, "other game mode rejected");
    ram = original; ram[0x6c39] = 3;
    require(!observe(ram.data(), ram.size(), 0).valid, "invalid save slot rejected");
    ram = original; write16(ram.data() + 0x4cb8, 256);
    require(!observe(ram.data(), ram.size(), 0).valid, "invalid sublevel rejected");
    for (unsigned state = 0; state < 256; ++state) {
        ram = original; ram[0x6b05] = state;
        require(observe(ram.data(), ram.size(), 0).valid ==
                (state == 13 || state == 14 || state == 35), "closed scene set");
    }
    std::uint64_t all_courses = 0;
    for (unsigned id = 0; id < 65536; ++id) {
        const int index = course_index(id);
        const bool expected = id < 72 && id % 12 < 10;
        require((index >= 0) == expected, "course mapping excludes intro/bonus/holes");
        if (expected) {
            require(index < 60 && !(all_courses & (std::uint64_t(1) << index)), "unique compact course bit");
            all_courses |= std::uint64_t(1) << index;
        }
    }
    require(all_courses == (std::uint64_t(1) << 60) - 1, "exactly 60 valid course bits");
}

void collectible_actions() {
    ++scenarios;
    Session s;
    require(s.tracker.progress() == empty, "no retrospective progress at startup");
    for (unsigned i = 1; i <= 20; ++i) {
        s.o.red = i; require(s.step(), "observed red pickup publishes progress");
        require(s.tracker.progress()[4] == i, "red maximum");
    }
    for (unsigned i = 1; i <= 5; ++i) {
        s.o.flowers = i; require(s.step(), "observed flower pickup publishes progress");
        require(s.tracker.progress()[5] == i, "flower maximum");
    }
    s.o.stars_tenths = 299; s.step();
    require(s.tracker.progress()[6] == 29, "29.9 stars do not become 30");
    s.o.stars_tenths = 300; s.step();
    require(s.tracker.progress()[6] == 30, "30 whole stars achieved");
    const auto earned = s.tracker.progress();
    for (unsigned i = 0; i < 1000; ++i) require(!s.step(), "unchanged frame does not republish");
    s.o.red = 0; s.o.flowers = 0; s.o.stars_tenths = 0; s.step();
    require(s.tracker.progress() == earned, "counter loss cannot remove earned progress");
    s.o.red = 65535; s.o.flowers = 65535; s.o.stars_tenths = 65535; s.step();
    require(s.tracker.progress() == earned, "out-of-range counters do not unlock");

    Session loaded;
    loaded.o.red = 20; loaded.o.flowers = 5; loaded.o.stars_tenths = 300;
    loaded.restore(); loaded.step();
    require(loaded.tracker.progress() == empty, "loading max counters is not an action");
    loaded.o.red = 19; loaded.o.flowers = 4; loaded.o.stars_tenths = 290; loaded.step();
    loaded.o.red = 20; loaded.o.flowers = 5; loaded.o.stars_tenths = 300; loaded.step();
    require(loaded.tracker.progress()[4] == 20 && loaded.tracker.progress()[5] == 5 &&
            loaded.tracker.progress()[6] == 30, "a new pickup after load can earn progress");
}

void continuity() {
    ++scenarios;
    for (unsigned kind = 0; kind < 9; ++kind) {
        Session s;
        s.o.red = 1; s.step();
        s.o.red = 20; s.o.flowers = 5; s.o.stars_tenths = 300;
        if (kind == 0) ++s.epoch;
        if (kind == 1) s.frame += 5;
        if (kind == 2) s.frame = 0;
        if (kind == 3) s.o.course = 1;
        if (kind == 4) s.o.save_file = 1;
        if (kind == 5) s.tracker.invalidate();
        if (kind == 6) s.o.room = 1;
        if (kind == 7) s.o.state = 14;
        if (kind == 8) s.o.valid = false;
        s.step();
        require(s.tracker.progress() == LocalAchievementsSnapshot{{1, 0, 0, 0, 1, 0, 0, 0}},
                "discontinuity/death/room change does not invent pickups or erase history");
    }
    Session s;
    s.o.red = 20;
    require(!s.tracker.frame(s.o, s.frame, s.epoch) && s.tracker.progress() == empty,
            "same-frame presentation cannot earn progress");
    Tracker wrapping;
    auto o = normal(); wrapping.frame(o, UINT64_MAX, 0);
    o.red = 20; wrapping.frame(o, 0, 0);
    require(wrapping.progress() == empty, "frame wrap is a discontinuity");
}

void transformations() {
    ++scenarios;
    unsigned expected_mask = 0;
    for (unsigned id = 0; id < 65536; ++id) {
        Session s;
        s.o.transformation = id; s.step();
        const unsigned expected = id == 2 ? 1 : id == 4 ? 2 : id == 6 ? 4 : id == 8 ? 8 : id == 12 ? 16 : 0;
        require(s.tracker.progress()[3] == expected, "only five recognized vehicle categories");
        expected_mask |= expected;
    }
    require(expected_mask == 31, "vehicle mask contract");
    Session s;
    for (const auto id : {2, 4, 6, 8, 12}) {
        s.o.transformation = 0; s.step(); s.o.transformation = id; s.step();
    }
    require(s.tracker.progress()[3] == 31, "different vehicles accumulate");
    Session loaded;
    loaded.o.transformation = 2; loaded.restore(); loaded.step();
    require(loaded.tracker.progress()[3] == 0, "starting inside vehicle is not transforming");
    loaded.o.transformation = 0; loaded.step(); loaded.o.transformation = 2; loaded.step();
    require(loaded.tracker.progress()[3] == 1, "new transformation after restore is eligible");
}

void victory() {
    ++scenarios;
    Session s;
    for (unsigned course = 0; course < 72; ++course) {
        if (course_index(course) < 0) continue;
        s.o = normal(course); s.step(); s.step();
        s.o.state = 14; s.o.victory_substate = 1; s.o.score = 100; s.step();
        const auto bit = std::uint64_t(1) << course_index(course);
        require((s.tracker.progress()[1] & bit) != 0, "real goal transition completes course");
        require((s.tracker.progress()[2] & bit) == 0, "stale 100 at goal start is not perfection");
        s.o.victory_substate = 0x3d; s.step();
        require((s.tracker.progress()[2] & bit) == 0, "perfect waits for score confirmation");
        s.o.victory_substate = 0x3f; s.step();
        require((s.tracker.progress()[2] & bit) != 0, "confirmed 100-point result");
        require((s.tracker.progress()[2] & ~s.tracker.progress()[1]) == 0, "perfect subset of completed");
        const auto saved = s.tracker.progress(); s.step();
        require(s.tracker.progress() == saved, "result is not repeatedly counted");
    }
    require(s.tracker.progress()[1] == (std::uint64_t(1) << 60) - 1, "all compact course bits");
    Session boss;
    boss.o.state = 35; boss.o.victory_substate = 0; boss.step(); boss.step();
    boss.o.state = 14; boss.o.victory_substate = 0x19; boss.step();
    require(boss.tracker.progress()[1] == 1, "observed boss victory leads to score screen");
    for (unsigned mode = 0; mode < 6; ++mode) {
        Session negative;
        if (mode == 0) { negative.o.state = 14; negative.o.victory_substate = 0x3d; negative.restore(); }
        if (mode == 1) { negative.o.state = 35; negative.restore(); negative.step(); }
        if (mode == 2) { negative.o.state = 14; negative.o.victory_substate = 0; negative.step(); }
        if (mode == 3) { negative.o.state = 14; negative.o.victory_substate = 0x55; negative.step(); }
        if (mode == 4) { negative.o.state = 14; negative.o.victory_substate = 0x3d; negative.step(); }
        if (mode == 5) { negative.o.valid = false; negative.step(); negative.o.valid = true; }
        negative.o.state = 14; negative.o.victory_substate = 0x3f; negative.o.score = 100; negative.step();
        require(negative.tracker.progress()[1] == 0 && negative.tracker.progress()[2] == 0,
                "loaded or invalid result does not award a course");
    }
    Session imperfect;
    imperfect.o.state = 14; imperfect.o.victory_substate = 1; imperfect.step();
    imperfect.o.victory_substate = 0x3d; imperfect.o.score = 99; imperfect.step();
    imperfect.o.victory_substate = 0x3f; imperfect.step();
    require(imperfect.tracker.progress()[1] == 1 && imperfect.tracker.progress()[2] == 0,
            "99-point clear is not perfect");
    Session restored;
    restored.o.state = 14; restored.o.victory_substate = 1; restored.step();
    restored.o.victory_substate = 0x3d; restored.o.score = 100; restored.restore();
    restored.o.victory_substate = 0x3f; restored.step();
    require(restored.tracker.progress()[1] == 1 && restored.tracker.progress()[2] == 0,
            "restoring during score preserves clear but cancels pending perfection");
}

void fuzzy_sequence(Session& s, unsigned fault) {
    s.o.fuzzy_timer = 5; s.step();
    s.o.fuzzy_active = 1; s.o.fuzzy_amplitude = 256; s.step();
    if (fault == 1) s.restore();
    s.o.fuzzy_timer = 4; s.o.fuzzy_amplitude = 512; s.step();
    if (fault == 2) s.o.fuzzy_timer = 0;
    else s.o.fuzzy_timer = 3;
    s.o.fuzzy_amplitude = 768; s.step();
    if (fault == 3) s.o.room = 1;
    s.o.fuzzy_timer = 2; s.step();
    if (fault == 4) { s.o.valid = false; s.step(); s.o.valid = true; }
    s.o.fuzzy_timer = 1; s.step();
    s.o.fuzzy_amplitude = fault == 5 ? 128 : 512; s.step();
    s.o.fuzzy_amplitude = 256; s.step();
    if (fault == 6) ++s.epoch;
    s.o.fuzzy_timer = 0; s.o.fuzzy_amplitude = 0; s.o.fuzzy_active = 0; s.step();
}
void fuzzy() {
    ++scenarios;
    Session s; fuzzy_sequence(s, 0);
    require(s.tracker.progress()[7] == 1, "observed contact/countdown/growth/fade earns recovery");
    const auto saved = s.tracker.progress();
    for (unsigned i = 0; i < 100; ++i) s.step();
    require(s.tracker.progress() == saved, "recovery is a durable one-time event");
    for (unsigned fault = 1; fault <= 6; ++fault) {
        Session bad; fuzzy_sequence(bad, fault);
        require(bad.tracker.progress()[7] == 0, "forced clear/load/gap/room/death is not recovery");
    }
    Session loaded;
    loaded.o.fuzzy_timer = 1; loaded.o.fuzzy_amplitude = 512; loaded.o.fuzzy_active = 1;
    loaded.restore(); loaded.o.fuzzy_amplitude = 256; loaded.step();
    loaded.o.fuzzy_timer = 0; loaded.o.fuzzy_amplitude = 0; loaded.o.fuzzy_active = 0; loaded.step();
    require(loaded.tracker.progress()[7] == 0, "loading an almost recovered effect is insufficient");
    fuzzy_sequence(loaded, 0);
    require(loaded.tracker.progress()[7] == 1, "a new full effect after load is eligible");
    for (unsigned amplitude = 0; amplitude < 1024; ++amplitude) {
        Session bad; bad.o.fuzzy_timer = 100; bad.o.fuzzy_amplitude = amplitude;
        bad.o.fuzzy_active = 1; bad.step();
        bad.o.fuzzy_timer = bad.o.fuzzy_amplitude = bad.o.fuzzy_active = 0; bad.step();
        require(bad.tracker.progress()[7] == 0, "one nonzero effect followed by zeros cannot fake recovery");
    }
}

void fuzzy_frame_start_cadence() {
    ++scenarios;
    // Real per-PPU-frame callback regression: a delayed update produced a
    // held timer followed by two native countdown updates. TCP snapshots at
    // another execution boundary see one update in each of those frames.
    for (unsigned fault = 0; fault < 7; ++fault) {
        Session s;
        s.o.fuzzy_timer = 5; s.step();
        s.o.fuzzy_active = 1; s.o.fuzzy_amplitude = 256; s.step();
        s.o.fuzzy_timer = 4; s.o.fuzzy_amplitude = 512; s.step();
        if (fault != 1) s.step();
        if (fault == 4) s.restore();
        if (fault == 5) ++s.o.room;
        if (fault == 6) s.frame += 1;
        s.o.fuzzy_timer = fault == 2 ? 1 : fault == 3 ? 0 : 2;
        s.o.fuzzy_amplitude = 768; s.step();
        s.o.fuzzy_timer = 1; s.step();
        s.o.fuzzy_amplitude = 512; s.step();
        s.o.fuzzy_amplitude = 256; s.step();
        s.o.fuzzy_amplitude = 0; s.o.fuzzy_active = 0; s.o.fuzzy_timer = 0; s.step();
        require(s.tracker.progress()[7] == (fault == 0 ? 1u : 0u),
                "Fuzzy catch-up needs one held countdown, bounded decrement and continuous scene");
    }
}
}  // namespace

int main() {
    observation_and_ids(); collectible_actions(); continuity(); transformations(); victory(); fuzzy();
    fuzzy_frame_start_cadence();
    std::printf("PASS: %u scenarios, %u checks; immutable observation, collectible actions, epochs, vehicles, goals, perfect results and natural Fuzzy recovery\n", scenarios, checks);
}
