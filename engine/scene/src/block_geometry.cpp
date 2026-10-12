#include "gta20/scene/block_geometry.hpp"

namespace gta20::scene {
namespace {

using Corners = std::array<Position3f, 4>;

// Corner offsets from the block origin, indexed by BlockFace (see the contract in the header).
constexpr std::array<Corners, 5> kFaceCorners{{
    {{{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}}, // lid
    {{{0, 1, 0}, {0, 0, 0}, {0, 0, 1}, {0, 1, 1}}}, // left
    {{{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}}}, // right
    {{{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}}, // top
    {{{1, 1, 0}, {0, 1, 0}, {0, 1, 1}, {1, 1, 1}}}, // bottom
}};

constexpr std::array<u8, 6> kQuadIndices{0, 1, 2, 0, 2, 3};

} // namespace

FaceGeometry block_face_geometry(BlockFace face, Position3f origin) {
    FaceGeometry geometry;
    const Corners& corners = kFaceCorners[static_cast<std::size_t>(face)];
    for (std::size_t i = 0; i < corners.size(); ++i) {
        geometry.positions[i] = {origin.x + corners[i].x, origin.y + corners[i].y, origin.z + corners[i].z};
    }
    geometry.indices = kQuadIndices;
    return geometry;
}

} // namespace gta20::scene
