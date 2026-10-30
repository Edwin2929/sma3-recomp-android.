// Controlled unit tests for the read-only race observer. These are not a
// complete native level playthrough or an Android device test.
#include "time_trial_detail.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>

using sma3::TimeTrialReason;
using sma3::TimeTrialStatus;
using sma3::time_trial_detail::Observation;
using sma3::time_trial_detail::Tracker;
unsigned checks = 0;

void require(bool value, const char* description) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", description); std::exit(1); }
}
Observation observed(unsigned course = 0, unsigned state = 0x0d) {
    Observation o;
    o.valid = o.allocated = true;
    o.course = course; o.state = state;
    return o;
}
struct Run {
    Tracker tracker;
    std::uint64_t frame = 100, epoch = 7;
    Run() { tracker.set_enabled(true); }
    void put(Observation o) { tracker.frame(o, frame++, epoch); }
    void start(unsigned course = 0) {
        put(observed(course, 0x0a));
        put(observed(course, 0x0c));
        put(observed(course, 0x0c));
        put(observed(course));
        require(tracker.snapshot()[2] == std::uint64_t(TimeTrialStatus::Running), "main entrance starts");
        require(tracker.snapshot()[4] == 0, "intro duration excluded");
    }
    void finish(unsigned course = 0) {
        auto o = observed(course, 0x0e); o.victory_substate = 1;
        put(o);
    }
};
void expect_reason(const Tracker& t, TimeTrialReason r) {
    require(t.snapshot()[2] == std::uint64_t(TimeTrialStatus::Invalidated), "attempt invalidated");
    require(t.snapshot()[9] == std::uint64_t(r), "specific invalidation reason");
    require(t.snapshot()[6] == 0, "invalid attempt never completes");
    for (unsigned i = 14; i < 74; ++i) require(t.snapshot()[i] == 0, "invalid attempt has no record");
}
void write16(std::uint8_t* p, unsigned n) { p[0] = n; p[1] = n >> 8; }
void write32(std::uint8_t* p, unsigned n) { write16(p,n); write16(p+2,n>>16); }

int main() {
    Tracker empty;
    const auto initial = empty.snapshot();
    require(initial.size() == 74 && initial[0] == 1 && initial[1] == 0, "disabled schema");
    require(initial[2] == 0 && initial[12] == 280896 && initial[13] == 16777216, "timing contract");

    // A full course, two pauses and an ordinary goal have hand-calculated
    // interval totals. Neither a long pause nor double presentation counts.
    Run timed;
    timed.start();
    for (unsigned i = 0; i < 60; ++i) {
        timed.put(observed());
        timed.tracker.frame(observed(), timed.frame - 1, timed.epoch);
    }
    require(timed.tracker.snapshot()[4] == 60, "duplicates do not double the clock");
    auto paused = observed(); paused.paused = true;
    timed.put(paused);
    require(timed.tracker.snapshot()[2] == 3 && timed.tracker.snapshot()[4] == 61, "pause opening interval");
    for (unsigned i = 0; i < 100; ++i) timed.put(paused);
    require(timed.tracker.snapshot()[4] == 61, "held pause adds no time");
    timed.put(observed());
    require(timed.tracker.snapshot()[4] == 61, "pause closing interval");
    timed.put(observed());
    timed.tracker.set_enabled(true);
    timed.put(observed());
    require(timed.tracker.snapshot()[4] == 63, "reapplying enabled preserves attempt");
    timed.finish();
    require(timed.tracker.snapshot()[2] == 4 && timed.tracker.snapshot()[4] == 64, "goal freezes elapsed time");
    require(timed.tracker.snapshot()[6] == 1 && timed.tracker.snapshot()[7] == 1 &&
            timed.tracker.snapshot()[8] == 64 && timed.tracker.snapshot()[14] == 64, "completed result and minimum");
    for (unsigned i = 0; i < 100; ++i) timed.finish();
    require(timed.tracker.snapshot()[6] == 1, "score animation does not complete twice");
    require(timed.tracker.snapshot()[4] == 64, "score animation never advances clock");
    timed.start();
    timed.put(observed()); timed.finish();
    require(timed.tracker.snapshot()[5] == 2 && timed.tracker.snapshot()[6] == 2 &&
            timed.tracker.snapshot()[14] == 2, "faster full retry replaces minimum");
    timed.start();
    for (unsigned i = 0; i < 7; ++i) timed.put(observed());
    timed.finish();
    require(timed.tracker.snapshot()[8] == 8 && timed.tracker.snapshot()[14] == 2, "slower retry retained as last only");
    timed.tracker.set_enabled(false);
    require(timed.tracker.snapshot()[2] == 0 && timed.tracker.snapshot()[14] == 2, "disable preserves records");

    // Main entry, screen exit and midpoint use different guest state paths.
    Run rooms;
    rooms.start(3);
    for (unsigned state : {0x09u,0x0au,0x0bu,0x0du,0x1fu,0x20u,0x21u,0x22u,0x0au,0x0bu,0x0du}) {
        auto o = observed(3,state); o.room = 27; rooms.put(o);
    }
    require(rooms.tracker.snapshot()[4] == 11, "rooms and internal Bandit game count elapsed time");
    rooms.put(observed(3,0x23));
    rooms.put(observed(3,0x23));
    auto boss_goal = observed(3,0x0e); boss_goal.victory_substate = 0x19;
    rooms.put(boss_goal);
    require(rooms.tracker.snapshot()[6] == 1 && rooms.tracker.snapshot()[8] == 14, "boss transition completes once");

    for (bool restored : {false,true}) {
        Run partial;
        partial.put(observed());
        if (restored) ++partial.epoch;
        partial.put(observed()); partial.finish();
        require(partial.tracker.snapshot()[6] == 0, "mid-level enable/load never records partial run");
        partial.put(observed(0,0x0a)); partial.put(observed(0,0x0b));
        partial.put(observed()); partial.finish();
        require(partial.tracker.snapshot()[6] == 0, "midway or screen exit cannot arm a record");
    }
    Run loaded_intro;
    loaded_intro.put(observed(0,0x0a));
    ++loaded_intro.epoch;
    loaded_intro.put(observed(0,0x0c)); loaded_intro.put(observed()); loaded_intro.finish();
    expect_reason(loaded_intro.tracker, TimeTrialReason::StateRestored);

    for (unsigned state : {0x0fu,0x10u,0x11u,0x12u,0x13u,0x24u,0x25u,0x26u,0x27u,0x28u,0x29u,0x2au,0x2bu,0x2cu,0x2du}) {
        Run died; died.start(); died.put(observed(0,state)); died.finish();
        expect_reason(died.tracker, TimeTrialReason::Death);
    }
    for (unsigned state : {0x33u,0x35u,0x40u,0x07u}) {
        Run leave; leave.start(); leave.put(observed(0,state));
        expect_reason(leave.tracker, TimeTrialReason::LeftLevel);
    }
    for (unsigned change : {0u,1u,2u,3u,4u}) {
        Run invalid; invalid.start();
        auto o = observed();
        if (change == 0) ++invalid.epoch;
        if (change == 1) invalid.frame += 5;
        if (change == 2) invalid.frame -= 2;
        if (change == 3) o.valid = false;
        if (change == 4) o.allocated = false;
        invalid.put(o);
        expect_reason(invalid.tracker, change == 0 ? TimeTrialReason::StateRestored :
            change < 3 ? TimeTrialReason::ClockDiscontinuity : TimeTrialReason::UnsupportedScene);
    }
    Run slot; slot.start(); auto other_slot=observed(); other_slot.slot=1; slot.put(other_slot);
    expect_reason(slot.tracker,TimeTrialReason::LeftLevel);
    Run other_course; other_course.start(); other_course.put(observed(1));
    expect_reason(other_course.tracker,TimeTrialReason::LeftLevel);
    Run wrong_goal; wrong_goal.start(); wrong_goal.put(observed(0,0x0e));
    expect_reason(wrong_goal.tracker,TimeTrialReason::UnsupportedScene);
    Run wrap; wrap.frame = std::numeric_limits<std::uint64_t>::max() - 3;
    wrap.start(); wrap.put(observed());
    expect_reason(wrap.tracker,TimeTrialReason::ClockDiscontinuity);
    Run changed; changed.start(); changed.tracker.invalidate(TimeTrialReason::SourceChanged);
    expect_reason(changed.tracker,TimeTrialReason::SourceChanged);
    Run toggled; toggled.start(); toggled.tracker.set_enabled(false); toggled.tracker.set_enabled(true);
    toggled.put(observed()); toggled.finish();
    require(toggled.tracker.snapshot()[6] == 0 && toggled.tracker.snapshot()[2] == 1, "toggle cannot resume partial attempt");

    Run all;
    for (unsigned world = 0; world < 6; ++world) for (unsigned stage = 0; stage < 10; ++stage) {
        const auto course = world * 12 + stage;
        all.start(course); all.finish(course);
        require(all.tracker.snapshot()[14 + world*10 + stage] == 1, "main Extra Secret record index");
    }
    require(all.tracker.snapshot()[6] == 60, "all sixty supported courses");
    for (unsigned unsupported : {10u,11u,22u,23u,70u,71u,72u,65535u}) {
        Run unsupported_run;
        unsupported_run.put(observed(unsupported,0x0a));
        unsupported_run.put(observed(unsupported,0x0c));
        unsupported_run.put(observed(unsupported)); unsupported_run.finish(unsupported);
        require(unsupported_run.tracker.snapshot()[6] == 0, "welcome and unsupported course excluded");
    }

    // Bounded RAM decoding, independent of Tracker transition fixtures.
    std::array<std::uint8_t,0x8000> ram{};
    write32(ram.data()+0x7010,0x0200000c); write32(ram.data()+0x7240,0x0300220c);
    write16(ram.data()+0x6288,67); write16(ram.data()+0x4cb8,201);
    write16(ram.data()+0x4828,0x19); ram[0x6b05]=0x0d; ram[0x6c39]=2;
    auto decoded=sma3::time_trial_detail::observe(ram.data(),ram.size());
    require(decoded.valid && decoded.allocated && !decoded.paused && decoded.course==67 &&
            decoded.room==201 && decoded.slot==2 && decoded.victory_substate==0x19, "verified allocation observation");
    ram[0x48b5]=1;
    require(sma3::time_trial_detail::observe(ram.data(),ram.size()).paused, "inventory freezes");
    ram[0x48b5]=0; ram[0x4d46]=0x11;
    require(sma3::time_trial_detail::observe(ram.data(),ram.size()).paused, "Start low nibble freezes");
    ram[0x4d46]=0x10;
    require(!sma3::time_trial_detail::observe(ram.data(),ram.size()).paused, "unrelated Start flag does not freeze");
    ram[0x6d64]=1;
    require(!sma3::time_trial_detail::observe(ram.data(),ram.size()).valid, "Mario Bros excluded");
    ram[0x6d64]=0; ram[0x6c39]=3;
    require(!sma3::time_trial_detail::observe(ram.data(),ram.size()).valid, "invalid save slot");
    ram[0x6c39]=0; ram[0x6b05]=0x4d;
    require(!sma3::time_trial_detail::observe(ram.data(),ram.size()).valid, "invalid game state");
    require(!sma3::time_trial_detail::observe(nullptr,0x8000).valid &&
            !sma3::time_trial_detail::observe(ram.data(),0x7fff).valid, "null and truncated RAM rejected");
    std::printf("PASS %u time trial unit checks\n",checks);
}
