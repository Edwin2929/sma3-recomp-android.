#include "no_mario_loss_challenge.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Qualification only: consumes public-shaped native observation CSV, never
// writes game RAM, Android progress, or a challenge checkpoint. A controlled
// prefix is explicitly synthetic and is not proof of a completed campaign.
using namespace sma3::no_mario_loss;
namespace {
struct Row { Observation o; std::uint64_t frame = 0, epoch = 0; };
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> result;
    std::stringstream input(line);
    std::string field;
    while (std::getline(input, field, ',')) result.push_back(field);
    return result;
}
std::vector<Row> read(const char* path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open native trace");
    std::string line;
    if (!std::getline(input, line)) throw std::runtime_error("empty native trace");
    const auto headings = split(line);
    std::map<std::string, std::size_t> columns;
    for (std::size_t i = 0; i < headings.size(); ++i) columns.emplace(headings[i], i);
    std::vector<Row> result;
    while (std::getline(input, line)) {
        const auto fields = split(line);
        if (fields.size() != headings.size()) throw std::runtime_error("truncated trace row");
        const auto value = [&](const char* name) {
            const auto field = columns.find(name);
            if (field == columns.end()) throw std::runtime_error(std::string("missing field: ") + name);
            std::size_t consumed = 0;
            const auto v = std::stoull(fields[field->second], &consumed);
            if (consumed != fields[field->second].size()) throw std::runtime_error("invalid trace number");
            return std::uint64_t(v);
        };
        Row r;
        r.frame = value("frame"); r.epoch = value("epoch");
        r.o.valid = value("campaign_valid"); r.o.allocated = value("campaign_allocated");
        r.o.fresh_file = value("fresh_file"); r.o.state = value("campaign_state");
        r.o.course = value("campaign_course"); r.o.slot = value("campaign_slot");
        r.o.room = value("campaign_room"); r.o.transformation = value("campaign_transformation");
        r.o.victory_substate = value("campaign_victory_substate");
        r.o.mario_flags = value("mario_flags"); r.o.mario_active = value("mario_active");
        r.o.mario_state = value("mario_state");
        result.push_back(r);
    }
    if (result.empty()) throw std::runtime_error("trace has no observations");
    return result;
}
Observation attached(unsigned course, unsigned slot) {
    Observation o;
    o.valid = o.allocated = true; o.state = 13; o.course = course; o.slot = slot;
    o.mario_flags = 0x8000; o.mario_active = 5;
    return o;
}
void controlled_prefix(Tracker& tracker, const Row& first, bool ending) {
    if (first.frame < 400) throw std::runtime_error("not enough frame range for controlled prefix");
    auto frame = first.frame - 400;
    auto o = attached(11, first.o.slot);
    o.state = 8; tracker.frame(o, frame++, first.epoch);
    o.state = 0x3a; o.fresh_file = true; tracker.frame(o, frame++, first.epoch);
    o.state = 0x3b; tracker.frame(o, frame++, first.epoch);
    o = attached(11, first.o.slot); tracker.frame(o, frame++, first.epoch);
    if (ending) {
        for (unsigned i = 0; i < 47; ++i) {
            o = attached(i / 8 * 12 + i % 8, first.o.slot);
            tracker.frame(o, frame++, first.epoch);
            o.state = 0x0e; o.victory_substate = 1;
            tracker.frame(o, frame++, first.epoch);
        }
    }
    o = attached(first.o.course, first.o.slot);
    while (frame + 1 < first.frame) tracker.frame(o, frame++, first.epoch);
    // A fixture may begin at the boss transition, or the first PPU sample may
    // already see its LevelVictory result. The preceding boss frame is then
    // controlled test setup too; it is never described as an observed fight.
    if (ending && first.o.state == 0x0e) o.state = 0x23;
    tracker.frame(o, frame, first.epoch);
    if (tracker.status() != Status::Active || tracker.summary()[3] != (ending ? 47u : 0u))
        throw std::runtime_error("invalid controlled prefix");
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error(
            "usage: replay_no_mario_loss_trace fresh|controlled_ending|controlled_star|controlled_loss trace.csv");
        const std::string mode = argv[1];
        const bool ending = mode == "controlled_ending", star = mode == "controlled_star";
        const bool loss = mode == "controlled_loss", fresh = mode == "fresh";
        if (!ending && !star && !loss && !fresh) throw std::runtime_error("unknown replay mode");
        const auto rows = read(argv[2]);
        std::size_t start = 0;
        if (!fresh) {
            while (start < rows.size() && (rows[start].epoch == 0 || !rows[start].o.valid ||
                    (rows[start].o.state != 0x0d && rows[start].o.state != 0x23 && rows[start].o.state != 0x0e))) ++start;
            if (start == rows.size()) throw std::runtime_error("no controlled fixture gameplay observation");
        }
        Tracker tracker;
        if (!fresh) controlled_prefix(tracker, rows[start], ending);
        std::uint64_t first_loss = 0, first_earned = 0;
        unsigned star_states = 0;
        for (std::size_t i = start; i < rows.size(); ++i) {
            const auto& r = rows[i];
            tracker.frame(r.o, r.frame, r.epoch);
            if (!first_loss && tracker.status() == Status::MarioLost) first_loss = r.frame;
            if (!first_earned && tracker.status() == Status::Earned) first_earned = r.frame;
            if (r.o.state == 13 && r.o.transformation == 0 && r.o.mario_flags == 0x2000 &&
                    r.o.mario_active == 8 && r.o.mario_state == 0) ++star_states;
        }
        const auto expected = ending ? Status::Earned : loss ? Status::MarioLost : Status::Active;
        if (tracker.status() != expected) throw std::runtime_error("unexpected final challenge status");
        if (star && !star_states) throw std::runtime_error("no executed Super Star return seam observed");
        std::printf("PASS %s: observations=%zu status=%llu main_courses=%llu first_loss=%llu first_earned=%llu star_return_frames=%u\n",
            mode.c_str(), rows.size() - start, static_cast<unsigned long long>(tracker.summary()[1]),
            static_cast<unsigned long long>(tracker.summary()[3]), static_cast<unsigned long long>(first_loss),
            static_cast<unsigned long long>(first_earned), star_states);
        if (!fresh) std::puts("CONTROLLED PREFIX: synthetic start/prior goals; this does not validate a full clean campaign.");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
