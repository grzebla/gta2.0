#include "gta20/scene/world_mesh.hpp"

#include "gta20/scene/draw_list.hpp"
#include "gta20/scene/face_texture.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <string>

namespace gta20::scene {
namespace {

struct AtlasUv {
    float u = 0, v = 0;
};

// Atlas coordinate of image corner A-D (face_texture.hpp: v grows down the image like the atlas).
AtlasUv corner_uv(const UvRect& slot, TexCorner corner) {
    switch (corner) {
    case TexCorner::A: return {slot.u0, slot.v0};
    case TexCorner::B: return {slot.u1, slot.v0};
    case TexCorner::C: return {slot.u1, slot.v1};
    case TexCorner::D: return {slot.u0, slot.v1};
    }
    return {slot.u0, slot.v0};
}

bool finite(Position3f p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

} // namespace

Result<bool> WorldMesh::append_face(BlockFace face, Position3f origin, const FaceTile& texture, u16 tile_count) {
    if (static_cast<u8>(face) > static_cast<u8>(BlockFace::bottom)) {
        return make_error("world mesh: unknown block face " + std::to_string(static_cast<int>(face)));
    }
    if (!is_drawable_tile(texture.tile, tile_count)) {
        return make_error("world mesh: tile " + std::to_string(texture.tile) + " is not drawable (tile_count " +
                          std::to_string(tile_count) + ", reserved from " + std::to_string(kFirstReservedTile) + ")");
    }
    if (vertices_.size() > std::numeric_limits<u32>::max() - 4u) {
        return make_error("world mesh: vertex count exceeds the u32 index range");
    }
    const FaceGeometry geometry = block_face_geometry(face, origin);
    for (const Position3f& p : geometry.positions) {
        if (!finite(p)) {
            return make_error("world mesh: non-finite face position");
        }
    }

    // Everything that can fail is checked above; reserve first so the appends cannot throw halfway.
    vertices_.reserve(vertices_.size() + geometry.positions.size());
    indices_.reserve(indices_.size() + geometry.indices.size());
    const auto base = static_cast<u32>(vertices_.size());
    const UvRect slot = TileAtlas::slot_uv(texture.tile);
    const auto labels = face_texture_corners(face, texture.rotation, texture.flip);
    for (std::size_t i = 0; i < geometry.positions.size(); ++i) {
        const AtlasUv uv = corner_uv(slot, labels[i]);
        vertices_.push_back({geometry.positions[i], uv.u, uv.v});
    }
    for (const u8 index : geometry.indices) {
        indices_.push_back(base + index);
    }
    return true;
}

void WorldMesh::clear() {
    vertices_.clear();
    indices_.clear();
}

} // namespace gta20::scene
