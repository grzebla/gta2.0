#pragma once

#include <vector>

#include "g2n/assets/gmp_map.hpp"
#include "g2n/core/camera2d.hpp"

namespace g2n::assets {
class StyStyle;
}

namespace g2n::scene {

/// Texture rectangle in normalised atlas coordinates.
struct UvRect {
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
};

/// One instanced quad. Layout matches the GPU vertex input (40 bytes, no padding).
struct QuadInstance {
    float center_x = 0, center_y = 0; // world units (blocks)
    float width = 1, height = 1;
    float rotation = 0; // radians, clockwise on screen
    u8 r = 255, g = 255, b = 255, a = 255;
    UvRect uv{}; // multiplied with the colour; the atlas' white slot gives a flat colour
};
static_assert(sizeof(QuadInstance) == 40, "QuadInstance must match the GPU vertex layout");

/// RGBA8 texture holding every style tile in a 32 x 32 grid of 64 x 64 slots.
/// Slot 1023 (unused by GTA 2 tiles, which stop at 991) is opaque white for untextured quads.
struct TileAtlas {
    static constexpr int kSlotSize = 64;
    static constexpr int kColumns = 32;
    static constexpr int kSize = kSlotSize * kColumns; // 2048
    static constexpr u16 kWhiteSlot = 1023;

    std::vector<u8> rgba; // kSize * kSize * 4, row-major, top row first
    u16 tile_count = 0;

    [[nodiscard]] static UvRect slot_uv(u16 slot);
    [[nodiscard]] static UvRect white_uv() { return slot_uv(kWhiteSlot); }
};

/// Decodes every tile of the style into an atlas (needs the original .sty).
[[nodiscard]] TileAtlas build_tile_atlas(const assets::StyStyle& style);
/// Atlas with only the white slot - for the synthetic/debug colour view.
[[nodiscard]] TileAtlas make_white_atlas();

/// Per-frame list of quads, drawn in order with a single instanced draw call.
class DrawList {
public:
    void clear() { quads_.clear(); }
    void add(const QuadInstance& q) { quads_.push_back(q); }
    [[nodiscard]] const std::vector<QuadInstance>& quads() const { return quads_; }

private:
    std::vector<QuadInstance> quads_;
};

struct EntityView {
    Vec2f position;
    float heading_radians = 0;
    float size = 0.5f;
    u8 r = 255, g = 255, b = 255;
};

/// Debug view: topmost block of every visible column as a flat colour by ground type / walls.
void append_map(DrawList& list, const assets::GmpMap& map, const Rect& visible);

/// Textured top-down view: the lid of every non-empty block, lowest level first, so higher
/// lids cover lower ones and transparent lid pixels show what is below. Lids are drawn
/// flat (slopes and walls are not projected yet). Lid tiles >= tile_count (reserved 992-1022,
/// the 3-sided-slope marker 1023) are skipped.
void append_map_textured(DrawList& list, const assets::GmpMap& map, const Rect& visible, u16 tile_count);

void append_entities(DrawList& list, const std::vector<EntityView>& entities);

} // namespace g2n::scene
