#pragma once

#include "time_trial_ghost_detail.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace sma3::ghost_detail {

enum class ReadResult { Missing, Valid, Invalid, IoError };
enum class WriteResult { Saved, AlreadyBetter, InvalidExisting, IoError };

inline std::filesystem::path file_path(const std::filesystem::path& directory, unsigned course) {
    return directory / ("course-" + std::to_string(course) + ".ghost");
}
inline ReadResult read_file(const std::filesystem::path& path, unsigned course,
                           std::unique_ptr<Run>& run) noexcept {
    run.reset();
    try {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) return ec ? ReadResult::IoError : ReadResult::Missing;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec) return ReadResult::IoError;
        if (size < 64 || size > kMaxFileBytes) return ReadResult::Invalid;
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        auto* input = std::fopen(path.string().c_str(), "rb");
        if (!input) return ReadResult::IoError;
        const auto count = std::fread(bytes.data(), 1, bytes.size(), input);
        const int trailing = std::fgetc(input);
        const bool error = std::ferror(input) != 0;
        const bool closed = std::fclose(input) == 0;
        if (count != bytes.size() || error || !closed) return ReadResult::IoError;
        if (trailing != EOF) return ReadResult::Invalid;
        run = decode(bytes, course);
        return run ? ReadResult::Valid : ReadResult::Invalid;
    } catch (...) { return ReadResult::IoError; }
}
inline bool sync_file(std::FILE* file) noexcept {
    if (std::fflush(file)) return false;
#ifdef _WIN32
    return _commit(_fileno(file)) == 0;
#else
    return ::fsync(::fileno(file)) == 0;
#endif
}
inline bool sync_directory(const std::filesystem::path& directory) noexcept {
#ifdef _WIN32
    (void)directory; return true;
#else
    const int fd = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0) return false;
    const bool synced = ::fsync(fd) == 0;
    const bool closed = ::close(fd) == 0;
    return synced && closed;
#endif
}
inline WriteResult write_file(const std::filesystem::path& directory, const Run& run,
                             std::unique_ptr<Run>& existing) noexcept {
    existing.reset();
    try {
        if (!valid_run(run)) return WriteResult::IoError;
        std::error_code ec;
        std::filesystem::create_directories(directory, ec);
        if (ec) return WriteResult::IoError;
        const auto path = file_path(directory, run.course);
        const auto prior = read_file(path, run.course, existing);
        if (prior == ReadResult::Invalid) return WriteResult::InvalidExisting;
        if (prior == ReadResult::IoError) return WriteResult::IoError;
        if (existing && existing->duration <= run.duration)
            return sync_directory(directory) ? WriteResult::AlreadyBetter : WriteResult::IoError;
        existing.reset();
        const auto bytes = encode(run);
        if (bytes.empty() || bytes.size() > kMaxFileBytes) return WriteResult::IoError;
        const auto temporary = path.string() + ".tmp";
        auto* output = std::fopen(temporary.c_str(), "wb");
        if (!output) return WriteResult::IoError;
        const bool wrote = std::fwrite(bytes.data(), 1, bytes.size(), output) == bytes.size();
        const bool synced = wrote && sync_file(output);
        const bool closed = std::fclose(output) == 0;
        if (!synced || !closed) { std::remove(temporary.c_str()); return WriteResult::IoError; }
        // Rename in the same directory is atomic on the Android/Linux targets.
        if (std::rename(temporary.c_str(), path.string().c_str())) {
            std::remove(temporary.c_str()); return WriteResult::IoError;
        }
        if (!sync_directory(directory)) return WriteResult::IoError;
        std::unique_ptr<Run> verified;
        if (read_file(path, run.course, verified) != ReadResult::Valid || encode(*verified) != bytes)
            return WriteResult::IoError;
        return WriteResult::Saved;
    } catch (...) { return WriteResult::IoError; }
}

}  // namespace sma3::ghost_detail
