#include "gta20/scene/draw_list.hpp"

#include "gta20/assets/sty_style.hpp"

#include <algorithm>
#include <cmath>

namespace gta20::scene {
namespace {

QuadInstance cell_quad(int x, int y, u8 r, u8 g, u8 b) {
    QuadInstance q;
    q.uv = TileAtlas::white_uv();
    q.center_x = static_cast<float>(x) + 0.5f;
    q.center_y = static_cast<float>(y) + 0.5f;
    q.r = r;
    q.g = g;
    q.b = b;
    return q;
}

struct VisibleCells {
    int x0, y0, x1, y1;
};

VisibleCells clip(const Rect& visible) {
    return {std::max(0, static_cast<int>(std::floor(visible.min.x))),
            std::max(0, static_cast<int>(std::floor(visible.min.y))),
            std::min(assets::kMapWidth - 1, static_cast<int>(std::floor(visible.max.x))),
            std::min(assets::kMapHeight - 1, static_cast<int>(std::floor(visible.max.y)))};
}

} // namespace

UvRect TileAtlas::slot_uv(u16 slot) {
    // Half-texel inset keeps nearest/linear sampling inside the slot.
    constexpr float inv = 1.0f / static_cast<float>(kSize);
    const int column_index = slot % kColumns;
    const int row_index = slot / kColumns; // integer grid row, intentionally truncated
    const auto col = static_cast<float>(column_index);
    const auto row = static_cast<float>(row_index);
    return {(col * kSlotSize + 0.5f) * inv, (row * kSlotSize + 0.5f) * inv, ((col + 1) * kSlotSize - 0.5f) * inv,
            ((row + 1) * kSlotSize - 0.5f) * inv};
}

namespace {

void fill_white_slot(TileAtlas& atlas) {
    const int sx = (TileAtlas::kWhiteSlot % TileAtlas::kColumns) * TileAtlas::kSlotSize;
    const int sy = (TileAtlas::kWhiteSlot / TileAtlas::kColumns) * TileAtlas::kSlotSize;
    for (int y = 0; y < TileAtlas::kSlotSize; ++y) {
        for (int x = 0; x < TileAtlas::kSlotSize; ++x) {
            const auto at =
                (static_cast<std::size_t>(sy + y) * TileAtlas::kSize + static_cast<std::size_t>(sx + x)) * 4;
            atlas.rgba[at] = atlas.rgba[at + 1] = atlas.rgba[at + 2] = atlas.rgba[at + 3] = 255;
        }
    }
}

} // namespace

TileAtlas make_white_atlas() {
    TileAtlas atlas;
    atlas.rgba.assign(static_cast<std::size_t>(TileAtlas::kSize) * TileAtlas::kSize * 4, 0);
    fill_white_slot(atlas);
    return atlas;
}

TileAtlas build_tile_atlas(const assets::StyStyle& style) {
    TileAtlas atlas = make_white_atlas();
    const auto count = static_cast<u16>(std::min<std::size_t>(style.tile_count(), TileAtlas::kWhiteSlot));
    std::vector<u8> tile(static_cast<std::size_t>(assets::kTileSize) * assets::kTileSize * 4);
    for (u16 t = 0; t < count; ++t) {
        style.decode_tile(t, tile);
        const int sx = (t % TileAtlas::kColumns) * TileAtlas::kSlotSize;
        const int sy = (t / TileAtlas::kColumns) * TileAtlas::kSlotSize;
        for (int y = 0; y < assets::kTileSize; ++y) {
            std::copy_n(
                tile.begin() + static_cast<std::ptrdiff_t>(y) * assets::kTileSize * 4, assets::kTileSize * 4,
                atlas.rgba.begin() +
                    static_cast<std::ptrdiff_t>(
                        (static_cast<std::size_t>(sy + y) * TileAtlas::kSize + static_cast<std::size_t>(sx)) * 4));
        }
    }
    atlas.tile_count = count;
    return atlas;
}

void append_map_textured(DrawList& list, const assets::GmpMap& map, const Rect& visible, u16 tile_count) {
    constexpr float kQuarterTurn = 1.5707963f;
    // Lid lighting (bits 10-11: 0 normal, 1-3 increasingly dark) is NOT applied: the document
    // gives no factors and guessed ones visibly distorted roofs. To be measured on the original.
    const VisibleCells c = clip(visible);
    for (int z = 0; z < assets::kMapLevels; ++z) {
        for (int y = c.y0; y <= c.y1; ++y) {
            for (int x = c.x0; x <= c.x1; ++x) {
                const auto& block = map.block_at(x, y, z);
                const auto lid = block.lid_face();
                // 0 = blank; 1023 = dummy marking 3-sided diagonal slopes; 992-1022 are reserved
                // for the engine (format documents) - none of these is a drawable style tile.
                if (lid.tile == 0 || lid.tile >= tile_count) {
                    continue;
                }
                QuadInstance q;
                q.center_x = static_cast<float>(x) + 0.5f;
                q.center_y = static_cast<float>(y) + 0.5f;
                q.uv = TileAtlas::slot_uv(lid.tile);
                if (lid.flip) {
                    std::swap(q.uv.u0, q.uv.u1); // mirror horizontally in tile space, before rotation
                }
                // Hypothesis (docs/specs/world-geometry.md section 5): rotation codes turn the tile
                // clockwise on screen.
                q.rotation = static_cast<float>(static_cast<int>(lid.rotation)) * kQuarterTurn;
                list.add(q);
            }
        }
    }
}

void append_map(DrawList& list, const assets::GmpMap& map, const Rect& visible) {
    const VisibleCells c = clip(visible);
    for (int y = c.y0; y <= c.y1; ++y) {
        for (int x = c.x0; x <= c.x1; ++x) {
            const int z = map.top_level(x, y);
            if (z < 0) {
                continue;
            }
            const auto& block = map.block_at(x, y, z);
            const bool walled =
                block.left_face().wall || block.right_face().wall || block.top_face().wall || block.bottom_face().wall;
            // Higher levels are drawn brighter so buildings read as raised.
            const auto lift = static_cast<u8>(std::min(z * 12, 80));
            if (walled) {
                list.add(cell_quad(x, y, static_cast<u8>(110 + lift), static_cast<u8>(80 + lift),
                                   static_cast<u8>(70 + lift)));
                continue;
            }
            switch (block.ground()) {
            case assets::GroundType::Road: list.add(cell_quad(x, y, 60, 60, 66)); break;
            case assets::GroundType::Pavement: list.add(cell_quad(x, y, 150, 150, 140)); break;
            case assets::GroundType::Field: list.add(cell_quad(x, y, 70, 120, 60)); break;
            case assets::GroundType::Air:
                list.add(cell_quad(x, y, static_cast<u8>(90 + lift), static_cast<u8>(90 + lift),
                                   static_cast<u8>(110 + lift)));
                break;
            }
        }
    }
}

void append_entities(DrawList& list, const std::vector<EntityView>& entities) {
    for (const auto& e : entities) {
        QuadInstance body;
        body.uv = TileAtlas::white_uv();
        body.center_x = e.position.x;
        body.center_y = e.position.y;
        body.width = e.size;
        body.height = e.size;
        body.rotation = e.heading_radians;
        body.r = e.r;
        body.g = e.g;
        body.b = e.b;
        list.add(body);
        // Heading marker: small quad in front of the body.
        QuadInstance nose = body;
        nose.width = nose.height = e.size * 0.35f;
        nose.center_x = e.position.x + std::cos(e.heading_radians) * e.size * 0.5f;
        nose.center_y = e.position.y + std::sin(e.heading_radians) * e.size * 0.5f;
        nose.r = nose.g = nose.b = 255;
        list.add(nose);
    }
}

} // namespace gta20::scene
