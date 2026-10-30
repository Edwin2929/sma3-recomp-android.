#include "challenge_checkpoint.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace sma3::challenge_checkpoint;
using namespace std::string_literals;
namespace fs = std::filesystem;
namespace {
unsigned checks = 0;
void require(bool condition, const char* description) {
    ++checks;
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", description); std::exit(1); }
}
void write(const fs::path& path, const std::string& value) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
    require(static_cast<bool>(output), "write test fixture");
}
std::string read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), {});
}
}

int main() {
    char directory[] = "/tmp/sma3-challenge-checkpoint-XXXXXX";
    require(::mkdtemp(directory) != nullptr, "create isolated test directory");
    const fs::path root(directory);
    const auto proof = (root / "achievements/run.bin").string();
    const auto state = (root / "game.suspend.state").string();
    const auto other = (root / "older.state").string();
    const std::vector<std::uint64_t> expected{1, 1, 2, 0x123456789abcdef0ull, 17};
    std::vector<std::uint64_t> actual{99};
    write(state, "exact suspended gameplay image\0frame 500"s);
    write(other, "same slot, older gameplay image");
    require(consume(proof, state, actual) == ReadResult::Missing && actual.empty(), "missing proof cannot restore");
    require(save(proof, state, expected), "save proof for exact image");
    const auto original = read(proof);
    require(!original.empty(), "proof committed");
    require(consume(proof, state, actual) == ReadResult::Restored && actual == expected, "exact resume restores all opaque state");
    require(!fs::exists(proof), "successful resume consumes proof before returning");
    require(consume(proof, state, actual) == ReadResult::Missing && actual.empty(), "same suspend cannot be replayed");

    require(save(proof, state, expected), "prepare mismatched image");
    require(consume(proof, other, actual) == ReadResult::Invalid && actual.empty(), "older state cannot use current proof");
    require(consume(proof, state, actual) == ReadResult::Missing, "mismatch cannot be retried after swapping files");
    for (std::size_t offset = 0; offset < original.size(); ++offset) {
        auto corrupted = original;
        corrupted[offset] ^= 1;
        write(proof, corrupted);
        require(consume(proof, state, actual) == ReadResult::Invalid && actual.empty(), "corrupt proof fails closed");
        require(!fs::exists(proof), "corrupt proof consumed");
    }
    for (std::size_t length = 0; length < original.size(); ++length) {
        write(proof, original.substr(0, length));
        require(consume(proof, state, actual) == ReadResult::Invalid && actual.empty(), "truncated proof fails closed");
    }
    write(proof, original + "extra");
    require(consume(proof, state, actual) == ReadResult::Invalid, "trailing data rejected");
    write(proof, original);
    write(state, "changed suspended gameplay image");
    require(consume(proof, state, actual) == ReadResult::Invalid, "modified suspend image rejected");

    require(save(proof, state, expected), "save before live continuation");
    require(discard(proof), "live foreground discards proof");
    require(discard(proof), "discard is idempotent");
    require(consume(proof, state, actual) == ReadResult::Missing, "foreground continuation cannot later roll back");
    require(save(proof, state, expected), "save before failed subsequent save");
    require(!save(proof, (root / "missing.state").string(), expected), "missing new suspend fails save");
    require(!fs::exists(proof), "failed suspend cannot leave previous eligibility");
    require(!save(proof, state, {}), "empty opaque payload rejected");
    require(!save(proof, state, std::vector<std::uint64_t>(65)), "oversized opaque payload rejected");
    write(root / "blocked", "regular file, not a directory");
    require(!save((root / "blocked/run.bin").string(), state, expected), "storage failure returned to caller");
    fs::create_symlink(other, proof);
    require(consume(proof, state, actual) == ReadResult::Invalid, "symlink proof rejected");
    require(read(other) == "same slot, older gameplay image", "invalid proof does not touch target");
    fs::remove_all(root);
    std::printf("PASS: %u checkpoint durability and rollback checks\n", checks);
}
