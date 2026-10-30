#include "challenge_checkpoint.h"

#include "sha1.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace sma3::challenge_checkpoint {
namespace {
constexpr std::array<std::uint8_t, 8> magic{{'S', 'M', 'A', '3', 'Y', 'S', 'P', 1}};
constexpr std::size_t max_words = 64;
constexpr std::size_t max_state_bytes = 32 * 1024 * 1024;
constexpr std::size_t digest_bytes = 20;

struct File {
    int fd = -1;
    explicit File(int value) : fd(value) {}
    ~File() { if (fd >= 0) ::close(fd); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
};

bool read_file(const std::string& path, std::size_t limit,
               std::vector<std::uint8_t>& bytes) {
    File input(::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
    if (input.fd < 0) return false;
    struct stat st{};
    if (::fstat(input.fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 ||
            static_cast<std::uint64_t>(st.st_size) > limit) return false;
    bytes.resize(static_cast<std::size_t>(st.st_size));
    std::size_t at = 0;
    while (at < bytes.size()) {
        const auto count = ::read(input.fd, bytes.data() + at, bytes.size() - at);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        at += static_cast<std::size_t>(count);
    }
    std::uint8_t trailing;
    ssize_t extra;
    do { extra = ::read(input.fd, &trailing, 1); } while (extra < 0 && errno == EINTR);
    return extra == 0;
}

bool sync_directory(const std::string& directory) {
    File dir(::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    return dir.fd >= 0 && ::fsync(dir.fd) == 0;
}

std::string parent(const std::string& path) {
    const auto directory = std::filesystem::path(path).parent_path();
    return directory.empty() ? "." : directory.string();
}

bool remove_proof(const std::string& path) {
    if (::unlink(path.c_str()) != 0) return errno == ENOENT;
    return sync_directory(parent(path));
}

void append64(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint64_t read64(const std::uint8_t* bytes) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= std::uint64_t(bytes[i]) << (8 * i);
    return value;
}

bool state_digest(const std::string& path, std::array<std::uint8_t, digest_bytes>& digest) {
    std::vector<std::uint8_t> state;
    if (!read_file(path, max_state_bytes, state)) return false;
    digest = gba::sha1(state.data(), state.size()).bytes;
    return true;
}
} // namespace

bool discard(const std::string& path) noexcept {
    try { return !path.empty() && remove_proof(path); }
    catch (...) { return false; }
}

bool save(const std::string& path, const std::string& state_path,
          const std::vector<std::uint64_t>& payload) noexcept {
    try {
        if (path.empty()) return false;
        // Remove any previous proof before preparing a new one. If the save
        // fails, an older clean attempt must not remain usable.
        if (!remove_proof(path)) return false;
        if (payload.empty() || payload.size() > max_words) return false;
        std::array<std::uint8_t, digest_bytes> digest{};
        if (!state_digest(state_path, digest)) return false;
        std::error_code error;
        const auto directory = parent(path);
        std::filesystem::create_directories(directory, error);
        if (error) return false;
        std::vector<std::uint8_t> bytes(magic.begin(), magic.end());
        append64(bytes, payload.size());
        bytes.insert(bytes.end(), digest.begin(), digest.end());
        for (auto word : payload) append64(bytes, word);
        const auto integrity = gba::sha1(bytes.data(), bytes.size()).bytes;
        bytes.insert(bytes.end(), integrity.begin(), integrity.end());

        const std::string temporary = path + ".tmp";
        File output(::open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600));
        if (output.fd < 0) return false;
        std::size_t at = 0;
        while (at < bytes.size()) {
            const auto count = ::write(output.fd, bytes.data() + at, bytes.size() - at);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) { ::unlink(temporary.c_str()); return false; }
            at += static_cast<std::size_t>(count);
        }
        if (::fsync(output.fd) != 0 || ::rename(temporary.c_str(), path.c_str()) != 0) {
            ::unlink(temporary.c_str());
            return false;
        }
        if (!sync_directory(directory)) { remove_proof(path); return false; }
        std::vector<std::uint8_t> verified;
        if (!read_file(path, magic.size() + 8 + 2 * digest_bytes + max_words * 8, verified) ||
                verified != bytes) { remove_proof(path); return false; }
        return true;
    } catch (...) { discard(path); return false; }
}

ReadResult consume(const std::string& path, const std::string& state_path,
                   std::vector<std::uint64_t>& payload) noexcept {
    payload.clear();
    try {
        if (path.empty()) return ReadResult::IoError;
        struct stat st{};
        if (::lstat(path.c_str(), &st) != 0)
            return errno == ENOENT ? ReadResult::Missing : ReadResult::IoError;
        std::vector<std::uint8_t> bytes;
        if (!read_file(path, magic.size() + 8 + 2 * digest_bytes + max_words * 8, bytes)) {
            return remove_proof(path) ? ReadResult::Invalid : ReadResult::IoError;
        }
        // Consume even malformed or mismatched proofs. A failed verification
        // cannot be retried with an older suspend file to resurrect eligibility.
        if (!remove_proof(path)) return ReadResult::IoError;
        constexpr auto header = magic.size() + 8 + digest_bytes;
        if (bytes.size() < header + 8 + digest_bytes ||
                !std::equal(magic.begin(), magic.end(), bytes.begin())) return ReadResult::Invalid;
        const auto count = read64(bytes.data() + magic.size());
        if (!count || count > max_words || bytes.size() != header + count * 8 + digest_bytes)
            return ReadResult::Invalid;
        const auto integrity = gba::sha1(bytes.data(), bytes.size() - digest_bytes).bytes;
        if (!std::equal(integrity.begin(), integrity.end(), bytes.end() - digest_bytes))
            return ReadResult::Invalid;
        std::array<std::uint8_t, digest_bytes> digest{};
        if (!state_digest(state_path, digest)) return ReadResult::Invalid;
        if (!std::equal(digest.begin(), digest.end(), bytes.begin() + magic.size() + 8))
            return ReadResult::Invalid;
        payload.reserve(static_cast<std::size_t>(count));
        for (std::size_t i = 0; i < count; ++i) payload.push_back(read64(bytes.data() + header + i * 8));
        return ReadResult::Restored;
    } catch (...) { payload.clear(); return ReadResult::IoError; }
}

} // namespace sma3::challenge_checkpoint
