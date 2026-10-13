#pragma once

#include <vector>

#include "gta20/assets/gmp_map.hpp"
#include "gta20/core/result.hpp"
#include "gta20/core/types.hpp"
#include "gta20/scene/block_geometry.hpp"

namespace gta20::scene {

/// One world-mesh vertex: position in block units and normalised atlas UV (v down the atlas).
struct WorldVertex {
    Position3f position;
    float u = 0, v = 0;
    friend constexpr bool operator==(const WorldVertex&, const WorldVertex&) = default;
};

/// Texture of one block face, as encoded by a face word (SideFace/LidFace).
struct FaceTile {
    u16 tile = 0;
    assets::Rotation rotation = assets::Rotation::R0;
    bool flip = false;
};

/// First tile id reserved for the engine (992-1022), followed by the map marker 1023. Neither is a
/// style tile. The atlas' white slot (TileAtlas::kWhiteSlot = 1023) is a debug colour, not a tile.
inline constexpr u16 kFirstReservedTile = 992;

/// True for tile ids that can be drawn from the atlas: 1 <= tile < tile_count and tile < 992.
[[nodiscard]] constexpr bool is_drawable_tile(u16 tile, u16 tile_count) {
    return tile >= 1 && tile < tile_count && tile < kFirstReservedTile;
}

/// GPU-independent indexed triangle mesh of textured block faces (u32 indices).
///
/// SYNTHETIC assembly contract: append_face places exactly the face it is given, using
/// block_face_geometry for the corners, face_texture_corners for the A-D labels and
/// TileAtlas::slot_uv for the atlas rectangle. It does not decide which faces of a map are built,
/// and it does not establish the original game's elevation, wall orientation or visibility
/// (docs/specs/world-geometry.md, sections 4-6). Nothing draws this mesh yet; DrawList is unchanged.
class WorldMesh {
public:
    /// Appends four vertices (p0..p3 of block_face_geometry) and six indices rebased to them.
    /// Fails without touching the mesh for a non-drawable tile (see is_drawable_tile), an unknown
    /// face, non-finite coordinates or when the vertex count would no longer fit in u32.
    [[nodiscard]] Result<bool> append_face(BlockFace face, Position3f origin, const FaceTile& texture, u16 tile_count);

    void clear();
    [[nodiscard]] const std::vector<WorldVertex>& vertices() const { return vertices_; }
    [[nodiscard]] const std::vector<u32>& indices() const { return indices_; }

private:
    std::vector<WorldVertex> vertices_;
    std::vector<u32> indices_;
};

} // namespace gta20::scene
