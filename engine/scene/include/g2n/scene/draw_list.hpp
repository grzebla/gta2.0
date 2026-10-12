#pragma once

#include <vector>

#include "g2n/assets/gmp_map.hpp"
#include "g2n/core/camera2d.hpp"

namespace g2n::scene {

/// One instanced quad. Layout matches the GPU vertex input (24 bytes, no padding).
struct QuadInstance {
    float center_x = 0, center_y = 0; // world units (blocks)
    float width = 1, height = 1;
    float rotation = 0; // radians, clockwise on screen
    u8 r = 255, g = 255, b = 255, a = 255;
};
static_assert(sizeof(QuadInstance) == 24, "QuadInstance must match the GPU vertex layout");

/// Per-frame list of quads, drawn in order with a single instanced draw call.
/// Reuses its storage across frames (no per-frame allocation after warm-up).
class DrawList {
public:
    void clear() { quads_.clear(); }
    void add(const QuadInstance& q) { quads_.push_back(q); }
    [[nodiscard]] const std::vector<QuadInstance>& quads() const { return quads_; }

private:
    std::vector<QuadInstance> quads_;
};

/// Interpolated, render-only view of the simulation (floats are fine here).
struct EntityView {
    Vec2f position;
    float heading_radians = 0;
    float size = 0.5f;
    u8 r = 255, g = 255, b = 255;
};

/// Adds the topmost block of every visible map column. Colours are a debug palette
/// keyed by ground type and walls - real tiles need the style (.sty) parser.
void append_map(DrawList& list, const assets::GmpMap& map, const Rect& visible);

void append_entities(DrawList& list, const std::vector<EntityView>& entities);

} // namespace g2n::scene
