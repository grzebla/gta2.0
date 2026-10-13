#include "gta20/scene/world_mesh_map.hpp"

#include <string>

namespace gta20::scene {
namespace {

struct FaceWord {
    BlockFace face = BlockFace::lid;
    FaceTile texture;
    bool flat = false;
};

// The five face words of a block in BlockFace order. Collision bits and lid lighting are dropped.
std::array<FaceWord, 5> face_words(const assets::BlockInfo& block) {
    const assets::LidFace lid = block.lid_face();
    const auto side = [](BlockFace face, const assets::SideFace& word) {
        return FaceWord{face, {word.tile, word.rotation, word.flip}, word.flat};
    };
    return {{
        {BlockFace::lid, {lid.tile, lid.rotation, lid.flip}, lid.flat},
        side(BlockFace::left, block.left_face()),
        side(BlockFace::right, block.right_face()),
        side(BlockFace::top, block.top_face()),
        side(BlockFace::bottom, block.bottom_face()),
    }};
}

} // namespace

Result<MapMesh> build_unsloped_map_mesh(const assets::GmpMap& map, u16 tile_count) {
    MapMesh out;
    MapMeshStats& stats = out.stats;
    for (int z = 0; z < assets::kMapLevels; ++z) {
        for (int y = 0; y < assets::kMapHeight; ++y) {
            for (int x = 0; x < assets::kMapWidth; ++x) {
                ++stats.cells_visited;
                const assets::BlockInfo& block = map.block_at(x, y, z);
                if (block.empty()) {
                    ++stats.empty_cells;
                    continue;
                }
                if (block.slope() != 0) {
                    ++stats.sloped_blocks;
                    ++stats.sloped_blocks_by_code[block.slope()];
                    continue;
                }
                ++stats.unsloped_blocks;
                const Position3f origin{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
                for (const FaceWord& word : face_words(block)) {
                    const auto face_index = static_cast<std::size_t>(word.face);
                    const u16 tile = word.texture.tile;
                    if (word.flat) {
                        ++stats.flat_faces;
                        ++stats.flat_by_face[face_index];
                    } else if (tile == 0) {
                        ++stats.blank_faces;
                    } else if (tile >= kFirstReservedTile) {
                        ++stats.reserved_faces;
                    } else if (tile >= tile_count) {
                        ++stats.out_of_style_faces;
                    } else {
                        auto appended = out.mesh.append_face(word.face, origin, word.texture, tile_count);
                        if (!appended) {
                            return make_error("map mesh: cell " + std::to_string(x) + "," + std::to_string(y) + "," +
                                              std::to_string(z) + ": " + appended.error().message);
                        }
                        ++stats.emitted_faces;
                        ++stats.emitted_by_face[face_index];
                    }
                }
            }
        }
    }
    return out;
}

} // namespace gta20::scene
