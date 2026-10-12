#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "gta20/core/result.hpp"
#include "gta20/core/types.hpp"

namespace gta20::assets {

/// What we know about a local GTA 2 installation (never shipped with the engine).
struct GameInstall {
    std::filesystem::path root;
    std::optional<std::string> exe_version; // e.g. "9.6.0.0" from gta2.exe's version resource
    std::vector<std::string> levels;        // single-player maps found, e.g. "data/wil.gmp"
};

/// Reads the VS_FIXEDFILEINFO file version of a Windows executable (data only - never executed).
[[nodiscard]] std::optional<std::string> pe_file_version(std::span<const std::byte> exe);

/// Inspects a directory that should contain an original GTA 2 installation (gta2.exe + data/).
[[nodiscard]] Result<GameInstall> inspect_game_dir(const std::filesystem::path& root);

/// Game directory from GTA20_GAME_DIR (empty if unset). "~/" is expanded with HOME/USERPROFILE.
[[nodiscard]] std::filesystem::path game_dir_from_environment();

} // namespace gta20::assets
