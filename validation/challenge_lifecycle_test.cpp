// Compile with --wrap=unlink and --wrap=fsync to inject real filesystem API
// failures. Native tracker stubs isolate the lifecycle permission to proceed;
// the independent tracker suite checks gameplay semantics.
#include "challenge_lifecycle.h"
#include "challenge_checkpoint.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
bool fail_unlink = false, fail_sync = false;
bool invalidated = false, restore_valid = true;
unsigned checks = 0, restored = 0;
std::vector<std::uint64_t> exported{1, 1, 0, 0};
void require(bool condition, const char* description) {
    ++checks;
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", description); std::exit(1); }
}
}
extern "C" int __real_unlink(const char*);
extern "C" int __wrap_unlink(const char* path) {
    if (fail_unlink) { errno = EACCES; return -1; }
    return __real_unlink(path);
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
    if (fail_sync) { errno = EIO; return -1; }
    return __real_fsync(fd);
}
namespace sma3 {
std::vector<std::uint64_t> local_achievements_challenge_checkpoint() { return exported; }
bool local_achievements_challenge_restore(const std::vector<std::uint64_t>& payload) noexcept {
    if (!restore_valid || payload != exported) return false;
    ++restored; return true;
}
void local_achievements_challenge_invalidate() noexcept { invalidated = true; }
}

int main() {
    using namespace sma3;
    char directory[] = "/tmp/sma3-challenge-lifecycle-XXXXXX";
    require(::mkdtemp(directory) != nullptr, "isolated directory");
    const auto proof = std::string(directory) + "/proof.bin";
    const auto state = std::string(directory) + "/game.state";
    std::ofstream(state) << "actual automatic state fixture";
    local_achievements_checkpoint_path(proof.c_str());
    require(local_achievements_live(), "fresh launch can execute");

    local_achievements_suspend(state.c_str(), true);
    require(std::filesystem::exists(proof) && !invalidated, "background stores eligibility");
    fail_unlink = true;
    require(!local_achievements_live() && invalidated, "live deletion failure prohibits guest execution");
    require(std::filesystem::exists(proof), "failed deletion leaves proof for no intervening gameplay");
    fail_unlink = false; invalidated = false;
    require(local_achievements_resume(state.c_str()) && restored == 1 && !invalidated,
            "recovered storage can resume only because no guest action was allowed");
    require(!std::filesystem::exists(proof), "trusted resume consumes proof");
    require(local_achievements_live(), "consumed resume may now execute");
    require(local_achievements_resume(state.c_str()) && invalidated && restored == 1,
            "repeat restore cannot restore challenge eligibility");

    invalidated = false;
    local_achievements_suspend(state.c_str(), true);
    fail_unlink = true;
    require(!local_achievements_resume(state.c_str()) && invalidated && restored == 1,
            "resume deletion failure prohibits guest execution and never imports payload");
    fail_unlink = false;
    require(local_achievements_live(), "storage recovery removes pending proof");

    invalidated = false;
    local_achievements_suspend(state.c_str(), true);
    fail_sync = true;
    require(!local_achievements_resume(state.c_str()) && invalidated && restored == 1,
            "unsynced consumption prohibits execution despite successful unlink");
    fail_sync = false;
    require(local_achievements_live(), "safe continuation after failure handling");

    invalidated = false;
    local_achievements_suspend(state.c_str(), true);
    local_achievements_untrusted_restore();
    require(invalidated && !std::filesystem::exists(proof), "manual state load invalidates attempt and proof");
    invalidated = false;
    local_achievements_suspend(state.c_str(), true);
    local_achievements_suspend(state.c_str(), false);
    require(invalidated && !std::filesystem::exists(proof), "failed state save cannot keep prior proof");

    invalidated = false;
    local_achievements_suspend(state.c_str(), true);
    restore_valid = false;
    require(local_achievements_resume(state.c_str()) && invalidated && restored == 1 &&
                !std::filesystem::exists(proof), "invalid tracker payload consumed but never imported");
    restore_valid = true;
    invalidated = false;
    local_achievements_suspend(state.c_str(), true);
    exported.clear();
    local_achievements_suspend(state.c_str(), true);
    require(invalidated && !std::filesystem::exists(proof), "invalid export cannot leave a stale checkpoint");
    local_achievements_checkpoint_path(nullptr);
    require(local_achievements_live(), "unconfigured host has no proof to consume");
    std::filesystem::remove_all(directory);
    std::printf("PASS: %u lifecycle and injected I/O-failure checks\n", checks);
}
