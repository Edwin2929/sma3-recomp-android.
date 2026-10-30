#include "no_mario_loss_challenge.h"

#include <cstdio>
#include <cstdlib>

using namespace sma3::no_mario_loss;
namespace {
unsigned checks = 0, groups = 0;
void require(bool value, const char* message) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
void w16(std::uint8_t* p, unsigned v) { p[0] = v; p[1] = v >> 8; }
void w32(std::uint8_t* p, unsigned v) { w16(p, v); w16(p + 2, v >> 16); }
Observation game(unsigned course = 0) {
    Observation o;
    o.valid = o.allocated = true; o.state = 13; o.course = course;
    o.mario_flags = 0x8000; o.mario_active = 5;
    return o;
}
struct Session {
    Tracker tracker;
    Observation o = game();
    std::uint64_t frame = 0, epoch = 0;
    Session() { o.state = 8; tracker.frame(o, frame, epoch); }
    void tick() { tracker.frame(o, ++frame, epoch); }
    void begin() {
        o.state = 8; tick(); o.state = 0x3a; o.fresh_file = true; tick();
        o.state = 0x3b; tick(); o = game(11); tick();
    }
    void clear(unsigned index, bool boss = false) {
        o = game(index / 8 * 12 + index % 8); tick(); tick();
        if (boss) { o.state = 0x23; tick(); tick(); }
        o.state = 14; o.victory_substate = boss ? 0x19 : 1; tick();
        o.victory_substate = 0x40; tick(); o.state = 0x35; tick();
    }
    void credits(unsigned map_state = 0x39, unsigned credits_state = 0x18) {
        o.state = map_state; tick(); o.allocated = false;
        o.state = credits_state; tick();
    }
    void all() { for (unsigned i = 0; i < 48; ++i) clear(i, i % 8 == 3 || i % 8 == 7); }
};

void observation_guards() {
    ++groups;
    std::array<std::uint8_t, 0x8000> ram{};
    ram[0x6b05] = 0x3a;
    w32(ram.data() + 0x6278, 0x30455941); w16(ram.data() + 0x62a6, 0x8000);
    auto o = observe(ram.data(), ram.size());
    require(o.valid && o.fresh_file && !o.allocated, "new-file identity without gameplay allocation");
    auto original = ram;
    for (unsigned i = 1; i < 72; ++i) {
        ram = original; w16(ram.data() + 0x62a6 + 2 * i, 1);
        require(!observe(ram.data(), ram.size()).fresh_file, "any prior course data disallows fresh start");
    }
    for (auto offset : {0x6278, 0x6350, 0x634c}) {
        ram = original; ram[offset] ^= 1;
        require(!observe(ram.data(), ram.size()).fresh_file, "signature/existing file/death guard");
    }
    ram = original; ram[0x6b05] = 13;
    w32(ram.data() + 0x7010, 0x0200000c); w32(ram.data() + 0x7240, 0x0300220c);
    w16(ram.data() + 0x6f2a, 0x4000); w16(ram.data() + 0x2484, 8); w16(ram.data() + 0x24ce, 9);
    o = observe(ram.data(), ram.size()); original = ram;
    require(o.allocated && o.mario_flags == 0x4000 && o.mario_state == 9 && lost_mario(o), "real capture field layout");
    require(ram == original, "observer never mutates guest bytes");
    require(!observe(nullptr, 0x8000).valid, "null memory is invalid");
    require(!observe(ram.data(), 0x7fff).valid, "short memory is invalid");
    ram[0x6d64] = 1; require(!observe(ram.data(), ram.size()).valid, "Mario Bros is not a YI campaign");
    ram = original; ram[0x6c39] = 3;
    require(!observe(ram.data(), ram.size()).valid, "invalid cartridge file slot");
    std::uint64_t seen = 0;
    for (unsigned course = 0; course < 72; ++course) {
        const int bit = main_index(course);
        require((bit >= 0) == (course % 12 < 8), "only eight main levels per world count");
        if (bit >= 0) { require(!(seen & (std::uint64_t(1) << bit)), "unique campaign level"); seen |= std::uint64_t(1) << bit; }
    }
    require(seen == kMainLevels, "all 48 campaign bits covered");
}

void genuine_begin_required() {
    ++groups;
    for (unsigned state : {0x0du, 0x3au, 0x3bu, 0x18u, 0x19u}) {
        Session s; s.o.state = state; s.o.fresh_file = false; s.tick(); s.all(); s.credits();
        require(s.tracker.status() == Status::NotStarted, "advanced saves cannot begin verified campaign");
    }
    Session intro;
    intro.o.state = 0x3a; intro.o.fresh_file = true;
    ++intro.epoch; intro.tick(); intro.tick();
    require(intro.tracker.status() == Status::NotStarted, "restored pristine intro is not new-game evidence");
    Session clean; clean.begin();
    require(clean.tracker.status() == Status::Active, "actual new-file launch begins challenge");
    clean.o = game(); clean.tick(); clean.clear(0); clean.clear(0);
    require(clean.tracker.summary()[3] == 1, "replaying the same course does not duplicate progress");
    clean.o = game(8); clean.tick(); clean.o.state = 14; clean.o.victory_substate = 1; clean.tick();
    require(clean.tracker.summary()[3] == 1, "secret levels are monitored but do not replace main courses");
    clean.o.state = 15; clean.tick(); clean.o = game(); clean.tick();
    require(clean.tracker.status() == Status::Active, "death without Mario separation does not invent extra requirement");
}

void loss_and_scripted_states() {
    ++groups;
    for (unsigned course = 0; course < 72; ++course) {
        if (course != 11 && sma3::local_achievements_detail::course_index(course) < 0) continue;
        for (unsigned mario : {0u, 3u, 9u, 10u, 11u, 12u, 13u, 14u}) {
            Session s; s.begin(); s.o = game(course); s.tick();
            s.o.mario_flags = 0; s.o.mario_active = 8; s.o.mario_state = mario; s.tick();
            require(s.tracker.status() == Status::MarioLost, "a knock-away in any normal/secret/intro map fails the entire run");
            s.o.mario_flags = 0x8000; s.o.mario_active = 5; s.tick(); s.clear(0);
            require(s.tracker.status() == Status::MarioLost, "catching Mario or retrying level cannot undo a loss");
        }
    }
    for (unsigned morph : {2u, 4u, 6u, 8u, 10u, 12u, 14u}) {
        Session s; s.begin(); s.o = game(); s.tick();
        s.o.mario_flags = 0; s.o.mario_active = 8; s.o.mario_state = 6; s.o.transformation = morph; s.tick();
        s.o.transformation = 0; s.tick(); s.o = game(); s.tick();
        require(s.tracker.status() == Status::Active, "morph bubble and its last untransformed frame are allowed");
    }
    for (unsigned state : {0x0bu, 0x0eu, 0x16u, 0x18u, 0x1du, 0x3bu}) {
        Session s; s.begin(); s.o.state = state; s.o.mario_flags = 0; s.o.mario_active = 8; s.tick();
        require(s.tracker.status() == Status::Active, "cinematic and non-play transfers are not knock-aways");
    }
    Session captured; captured.begin(); captured.o = game(); captured.o.mario_flags = 0x4000; captured.tick();
    require(captured.tracker.status() == Status::MarioLost, "bandit capture fails regardless of sprite substate");
    captured.begin(); captured.o = game(); captured.o.mario_flags = 0xc000;
    captured.o.transformation = 2; captured.tick();
    require(captured.tracker.status() == Status::MarioLost,
            "explicit enemy capture evidence takes priority over transformation or attachment flags");
    for (unsigned mario : {1u, 2u, 4u, 5u, 6u, 7u, 8u}) {
        Session s; s.begin(); s.o = game(); s.o.mario_flags = 0;
        s.o.mario_active = 8; s.o.mario_state = mario; s.tick();
        require(s.tracker.status() == Status::Active, "scripted Mario state is not treated as knock-away evidence");
    }
    Session superstar; superstar.begin(); superstar.o = game(); superstar.o.transformation = 0x10;
    superstar.o.mario_flags = 0x2000; superstar.o.mario_active = 1; superstar.tick();
    superstar.o.transformation = 0; superstar.tick();
    superstar.o.mario_active = 8; superstar.tick(); superstar.tick();
    superstar.o.mario_state = 1; superstar.tick(); superstar.o = game(); superstar.tick();
    require(superstar.tracker.status() == Status::Active,
            "executed Super Baby Egg return (active1 -> active8/state0/flags2000 -> state1 -> attached) is not a loss");
    superstar.o.mario_flags = 0; superstar.o.mario_active = 8; superstar.tick();
    require(superstar.tracker.status() == Status::MarioLost,
            "normal hit after Super Star reattachment still fails the attempt");
    for (unsigned mario : {3u, 9u, 10u, 11u, 12u, 13u, 14u}) {
        Session s; s.begin(); s.o = game(); s.o.mario_flags = 0x2000;
        s.o.mario_active = 8; s.o.mario_state = mario; s.tick();
        require(s.tracker.status() == Status::MarioLost, "scripted-return bit does not exempt other damage states");
    }
    Session return_capture; return_capture.begin(); return_capture.o = game();
    return_capture.o.mario_flags = 0x6000; return_capture.o.mario_active = 8; return_capture.tick();
    require(return_capture.tracker.status() == Status::MarioLost,
            "explicit enemy capture while returning from Super Star still fails");
    Session edge; edge.begin(); edge.o = game(); edge.tick();
    auto hit = edge.o; hit.mario_flags = 0; hit.mario_active = 8;
    edge.tracker.observe_loss(hit);
    require(edge.tracker.status() == Status::MarioLost, "suspend-boundary fresh check catches hit after PPU sample");
}

void completion_is_whole_campaign() {
    ++groups;
    Session good; good.begin(); good.all();
    require(good.tracker.summary()[3] == 48 && good.tracker.status() == Status::Active,
            "all main clears still wait for real ending");
    good.credits();
    require(good.tracker.status() == Status::Earned, "complete uninterrupted clean campaign reaches credits");
    good.tracker.invalidate(); good.o.mario_flags = 0; good.o.mario_active = 8; good.o.state = 13; good.tick();
    require(good.tracker.status() == Status::Earned, "earned achievement is permanent");
    for (unsigned map_state : {0x17u, 0x39u, 0x4bu}) for (unsigned credits_state : {0x18u, 0x19u}) {
        Session s; s.begin();
        for (unsigned i = 0; i < 47; ++i) s.clear(i);
        s.o = game(67); s.tick(); s.o.state = 0x23; s.tick(); s.tick();
        require(s.tracker.summary()[3] == 47, "final boss victory alone waits for score transition");
        s.o.state = 0x0e; s.o.victory_substate = 0x19; s.tick();
        require(s.tracker.summary()[3] == 48 && s.tracker.status() == Status::Active,
                "real final Bowser 23-to-0E adds the 48th main course");
        s.credits(map_state, credits_state);
        require(s.tracker.status() == Status::Earned,
                "castle fade and both final map branches can enter credits after arena deallocation");
    }
    for (unsigned false_origin : {0x0du, 0x0eu, 0x16u, 0x35u, 0x38u}) {
        Session s; s.begin(); s.all(); s.credits(false_origin);
        require(s.tracker.status() == Status::Active,
                "title/fade/menu or gameplay is not a final-castle credits transition");
    }
    Session wrong_course; wrong_course.begin(); wrong_course.all(); wrong_course.o.course = 0;
    wrong_course.credits();
    require(wrong_course.tracker.status() == Status::Active, "credits must belong to the final course");
    for (unsigned skipped = 0; skipped < 48; ++skipped) {
        Session s; s.begin();
        for (unsigned i = 0; i < 48; ++i) if (i != skipped) s.clear(i);
        s.credits(); require(s.tracker.status() == Status::Active, "a missing main course cannot be replaced by final cutscene");
    }
    Session failed; failed.begin(); failed.clear(0);
    failed.o = game(12); failed.o.mario_flags = 0; failed.o.mario_active = 8; failed.tick();
    failed.all(); failed.credits();
    require(failed.tracker.status() == Status::MarioLost, "full later completion does not erase earlier loss");
    failed.begin(); require(failed.tracker.status() == Status::Active && failed.tracker.summary()[3] == 0,
            "a genuinely new file starts another attempt");
}

void continuity_and_checkpoint() {
    ++groups;
    for (unsigned discontinuity = 0; discontinuity < 5; ++discontinuity) {
        Session s; s.begin();
        if (discontinuity == 0) ++s.epoch;
        if (discontinuity == 1) s.frame += 2;
        if (discontinuity == 2) s.frame = 0;
        if (discontinuity == 3) s.o.slot = 1;
        if (discontinuity == 4) s.o.allocated = false;
        s.tick(); s.all(); s.credits();
        require(s.tracker.status() == Status::Unverifiable, "untrusted discontinuity invalidates entire attempt");
    }
    for (unsigned n = 0; n < 48; ++n) {
        Session s; s.begin(); for (unsigned i = 0; i < n; ++i) s.clear(i);
        auto checkpoint = s.tracker.checkpoint();
        Tracker restored;
        require(restored.restore(checkpoint), "trusted checkpoint parses");
        require(restored.summary() == s.tracker.summary(), "trusted restart preserves run identity and level mask");
        s.tracker = restored; ++s.epoch; s.tick();
        for (unsigned i = n; i < 48; ++i) s.clear(i);
        s.credits(); require(s.tracker.status() == Status::Earned, "one-use verified resume continues whole campaign");
    }
    Session current; current.begin(); current.clear(0);
    const auto checkpoint = current.tracker.checkpoint();
    for (unsigned field : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 20u, 21u}) {
        auto bad = checkpoint; bad[field] = UINT64_MAX;
        const auto before = current.tracker.checkpoint();
        require(!current.tracker.restore(bad) && current.tracker.checkpoint() == before,
                "malformed checkpoint cannot partially mutate live attempt");
    }
    auto incomplete = checkpoint; incomplete.pop_back();
    require(!current.tracker.restore(incomplete), "truncated checkpoint rejected");
    auto false_earned = checkpoint; false_earned[1] = 4;
    require(!current.tracker.restore(false_earned), "earned marker requires completed campaign evidence");
    Session lost; lost.begin(); lost.o = game(); lost.o.mario_flags = 0; lost.o.mario_active = 8; lost.tick();
    Tracker recreated; require(recreated.restore(lost.tracker.checkpoint()), "failed checkpoint retained");
    require(recreated.status() == Status::MarioLost, "process restart cannot erase failed attempt");
    Session restored_hit; restored_hit.begin(); require(recreated.restore(restored_hit.tracker.checkpoint()), "active checkpoint ready");
    restored_hit.o.mario_flags = 0; restored_hit.o.mario_active = 8;
    recreated.frame(restored_hit.o, restored_hit.frame, restored_hit.epoch + 1);
    require(recreated.status() == Status::MarioLost, "first restored duplicate-frame observation still checks present loss");
}
}

int main() {
    observation_guards(); genuine_begin_required(); loss_and_scripted_states();
    completion_is_whole_campaign(); continuity_and_checkpoint();
    std::printf("Yo shi pude native model: %u groups, %u checks PASS\n", groups, checks);
}
