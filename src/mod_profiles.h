#pragma once

#include <filesystem>
#include <string>

namespace gbarecomp {

// Call once on the game thread before run_game, never from a UI worker.
// The real ROM must match the same SHA1 used by the strict-static runner.
// A failed or corrupt catalog falls back to unmodified presentation.
bool sma3_mod_initialize(const std::filesystem::path& root,
                         const std::filesystem::path& rom_path,
                         std::string* error = nullptr);

// Serialized catalog operations. All responses are ASCII JSON, including
// Unicode escapes for metadata, and include the authoritative current list.
// Import installs disabled. Enable replaces the prior active color profile.
// Changes affect complete presented frames without touching guest state.
std::string sma3_mod_list();
std::string sma3_mod_import(const std::filesystem::path& archive);
std::string sma3_mod_set_enabled(const std::string& id, bool enabled);
std::string sma3_mod_remove(const std::string& id);

}  // namespace gbarecomp
