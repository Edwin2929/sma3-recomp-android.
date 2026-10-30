#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sma3::challenge_checkpoint {

// Host-only proof attached to one exact automatic suspend image. These
// operations run while gameplay is stopped, never from the frame observer.
// A successful consume durably removes the proof before returning its data.
// Restoring the same image later cannot roll back a failed challenge attempt.
enum class ReadResult { Missing, Restored, Invalid, IoError };
bool save(const std::string& record_path, const std::string& state_path,
          const std::vector<std::uint64_t>& payload) noexcept;
ReadResult consume(const std::string& record_path, const std::string& state_path,
                   std::vector<std::uint64_t>& payload) noexcept;
bool discard(const std::string& record_path) noexcept;

} // namespace sma3::challenge_checkpoint
