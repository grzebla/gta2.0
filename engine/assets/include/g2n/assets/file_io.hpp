#pragma once

#include <filesystem>
#include <span>
#include <vector>

#include "g2n/core/result.hpp"

namespace g2n::assets {

[[nodiscard]] Result<std::vector<std::byte>> read_file(const std::filesystem::path& path);
[[nodiscard]] Result<bool> write_file(const std::filesystem::path& path, std::span<const std::byte> bytes);

/// Resolves a path inside the original game directory case-insensitively
/// (GTA 2 ships mixed-case names; Linux/macOS file systems may be case-sensitive).
[[nodiscard]] Result<std::filesystem::path> resolve_game_path(const std::filesystem::path& game_dir,
                                                              const std::filesystem::path& relative);

} // namespace g2n::assets
