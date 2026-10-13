#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "gta20/core/types.hpp"

namespace gta20::assets {

/// Which style (.sty) an original map (.gmp) is played with. `source` names the original
/// configuration file that declares the pair (test1.seq for single player, *.mmp for multiplayer).
/// An association whose style is not known has an empty `style` and says why in `source`.
struct MapStyleAssociation {
    std::string_view map;    // e.g. "data/wil.gmp" (file names as shipped; compared case-insensitively)
    std::string_view style;  // e.g. "data/wil.sty"; empty = unresolved
    std::string_view source; // e.g. "data/test1.seq"
    [[nodiscard]] constexpr bool resolved() const { return !style.empty(); }
};

/// The 17 maps of the PC release (9.6 / 10.5) and their styles.
[[nodiscard]] std::span<const MapStyleAssociation> original_map_styles();

/// Association for a map given as "data/x.gmp", "x.gmp" or any path ending in that file name
/// (case-insensitive); nullptr for maps that are not part of the original release.
[[nodiscard]] const MapStyleAssociation* find_map_style(std::string_view map);

/// Aggregate facts about one map. Counts are over occupied map cells (non-empty blocks in the
/// 256x256x8 grid), never individual records.
struct MapInventoryEntry {
    std::string map;
    std::string style;
    std::string source;
    std::size_t bytes = 0;
    std::size_t unique_blocks = 0;
    std::size_t occupied_cells = 0;
    int min_level = -1; // lowest/highest z holding a non-empty block
    int max_level = -1;
    std::array<u32, 64> slope_cells{}; // occupied cells per slope code (slope_type bits 2-7)
    u32 reserved_side_faces = 0;       // side faces with tiles 992-1022 (engine-reserved)
    u32 reserved_lids = 0;             // lids with tiles 992-1022
    u32 marker_side_faces = 0;         // side faces with tile 1023
    u32 marker_lids = 0;               // lids with tile 1023 (3-sided diagonal slope marker)
};

struct StyleInventoryEntry {
    std::string style;
    u16 version = 0;
    std::size_t tiles = 0;
};

struct MapInventory {
    std::filesystem::path root;
    std::vector<StyleInventoryEntry> styles;
    std::vector<MapInventoryEntry> maps;
    std::vector<std::string> problems; // empty = the installation passed every strict check

    [[nodiscard]] bool passed() const { return problems.empty(); }
};

/// Strict check of an original installation against `expected` (default: original_map_styles()).
/// Fails (problems non-empty) when `root` is empty, is not a game directory, data/ does not hold
/// exactly the expected maps, a map or style does not parse, a map has no blocks, an association
/// is unresolved, or the game's own configuration files (test1.seq, *.mmp) do not declare the
/// recorded map-to-style pairs.
[[nodiscard]] MapInventory take_map_inventory(const std::filesystem::path& root,
                                              std::span<const MapStyleAssociation> expected = original_map_styles());

/// Human-readable report: one summary line per style and a few lines per map (bounded size;
/// aggregate counts only, no map records or pixels).
[[nodiscard]] std::string format_map_inventory(const MapInventory& inventory);

} // namespace gta20::assets
