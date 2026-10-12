#pragma once

#include <array>

#include "gta20/core/types.hpp"

namespace gta20::scene {

/// GPU-independent geometry of one face of a unit block.
///
/// Local contract (not a statement about the original game's elevation convention):
/// a block with origin (x, y, z) occupies [x, x+1] x [y, y+1] x [z, z+1]. x follows map
/// columns, y follows map rows and z increases upward.
///
///   face    plane    outward normal
///   left    x        -X
///   right   x + 1    +X
///   top     y        -Y   (a wall facing the previous map row, not a horizontal surface)
///   bottom  y + 1    +Y   (a wall facing the next map row, not a horizontal surface)
///   lid     z + 1    +Z
///
/// Each face is two triangles {0, 1, 2} and {0, 2, 3} covering exactly one unit square. The
/// corners are ordered so that (p1 - p0) x (p2 - p0) and (p2 - p0) x (p3 - p0) point along
/// the outward normal. Walls list their two lower corners (z) first, then the upper ones (z + 1).
enum class BlockFace : u8 { lid, left, right, top, bottom };

/// Plain position in block units.
struct Position3f {
    float x = 0, y = 0, z = 0;
    friend constexpr bool operator==(Position3f, Position3f) = default;
};

struct FaceGeometry {
    std::array<Position3f, 4> positions{};
    std::array<u8, 6> indices{};
    friend constexpr bool operator==(const FaceGeometry&, const FaceGeometry&) = default;
};

/// Four corners and six triangle indices of `face` of the unit block at `origin`.
[[nodiscard]] FaceGeometry block_face_geometry(BlockFace face, Position3f origin);

} // namespace gta20::scene
