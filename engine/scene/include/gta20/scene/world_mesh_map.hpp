#pragma once

#include <array>

#include "gta20/assets/gmp_map.hpp"
#include "gta20/core/result.hpp"
#include "gta20/core/types.hpp"
#include "gta20/scene/world_mesh.hpp"

namespace gta20::scene {

/// Is the mesh built from a map known to be missing content?
///
/// Neither value means "complete M1 geometry": the face selection below is provisional and no
/// value of this enum is evidence of how the original game draws a map.
enum class MapMeshCoverage : u8 {
    /// Unsupported content was skipped (a sloped block, a flat face, a reserved or out-of-style
    /// tile): the mesh is known to lack geometry the map describes.
    partial,
    /// Nothing was skipped except blank faces and empty cells. Still provisional (selection rule,
    /// elevation and wall texture orientation are unresolved), never complete M1 geometry.
    provisional,
};

/// Bounded aggregate counts of one build. Each face of a visited unsloped, non-empty cell lands in
/// exactly one of emitted/flat/blank/reserved/out_of_style, so
/// 5 * unsloped_blocks == emitted_faces + flat_faces + blank_faces + reserved_faces + out_of_style_faces.
struct MapMeshStats {
    u32 cells_visited = 0;                       // always 256 * 256 * 8
    u32 empty_cells = 0;                         // BlockInfo::empty()
    u32 sloped_blocks = 0;                       // slope() != 0: skipped whole, no face of them is emitted
    u32 unsloped_blocks = 0;                     // non-empty with slope() == 0: their five faces are classified below
    std::array<u32, 64> sloped_blocks_by_code{}; // index = slope() (6 bits); [0] stays 0

    u32 emitted_faces = 0;
    u32 flat_faces = 0;                   // flat bit set, whatever the tile: skipped, unsupported
    u32 blank_faces = 0;                  // tile 0 without the flat bit: nothing to draw
    u32 reserved_faces = 0;               // tile 992..1023 (1023 is the map marker), never the atlas white slot
    u32 out_of_style_faces = 0;           // 1 <= tile < 992 but tile >= tile_count
    std::array<u32, 5> emitted_by_face{}; // indexed by BlockFace
    std::array<u32, 5> flat_by_face{};    // indexed by BlockFace

    [[nodiscard]] bool has_unsupported() const {
        return sloped_blocks != 0 || flat_faces != 0 || reserved_faces != 0 || out_of_style_faces != 0;
    }
    [[nodiscard]] MapMeshCoverage coverage() const {
        return has_unsupported() ? MapMeshCoverage::partial : MapMeshCoverage::provisional;
    }
    friend bool operator==(const MapMeshStats&, const MapMeshStats&) = default;
};

struct MapMesh {
    WorldMesh mesh;
    MapMeshStats stats;
};

/// Builds the indexed CPU mesh of every unsloped map cell (docs/specs/world-mesh.md). Camera
/// independent; identical inputs give identical vertex/index sequences and statistics.
///
/// PARTIAL, PROVISIONAL contract (local convention, not original-game evidence):
/// - Cells are visited z = 0..7, then y = 0..255, then x = 0..255; faces of a cell in BlockFace order
///   (lid, left, right, top, bottom). Cell (x, y, z) is the block origin (x, y, z), so lids lie at
///   heights 1..8 and the mesh spans z = 0..8.
/// - Blocks with slope() != 0 are skipped whole and counted by slope code (no slope approximation).
/// - In an unsloped block, a face with the flat bit is skipped and counted; otherwise every face with
///   a drawable tile (is_drawable_tile) is emitted. The wall and bullet_wall bits, lid lighting and
///   neighbouring blocks are not consulted: no collision-based visibility, no hidden-face removal.
/// Fails only if WorldMesh::append_face fails (cannot happen for a parsed map).
[[nodiscard]] Result<MapMesh> build_unsloped_map_mesh(const assets::GmpMap& map, u16 tile_count);

} // namespace gta20::scene
