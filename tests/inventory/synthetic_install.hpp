#pragma once

// SYNTHETIC GTA 2 installations for the strict map inventory tests. Nothing here is original
// data: maps come from GmpWriter, styles are minimal GBST containers and test1.seq / *.mmp are
// generated from the recorded map-to-style associations.

#include <filesystem>
#include <string_view>
#include <vector>

#include "gta20/core/result.hpp"

namespace gta20::testsupport {

std::vector<std::byte> text_bytes(std::string_view s);

/// The smallest GBST container parse_sty accepts (palettes only, no tiles).
const std::vector<std::byte>& synthetic_style();

/// A map with 4 occupied cells: 2 road cells (slope 0, z 0), a diagonal slope 49 with lid 1023
/// (z 2) and a pavement block with side tile 995 and lid 996 (z 5).
const std::vector<std::byte>& synthetic_map();

/// Writes all 17 expected maps, their styles and configuration files under `root`.
[[nodiscard]] Result<bool> write_synthetic_install(const std::filesystem::path& root);

} // namespace gta20::testsupport
