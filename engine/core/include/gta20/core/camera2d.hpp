#pragma once

#include <array>

#include "gta20/core/vec.hpp"

namespace gta20 {

struct Rect {
    Vec2f min;
    Vec2f max;
};

/// Top-down orthographic camera. World units are map blocks; world +y points
/// down the screen (map rows), like GTA 2's top-down view.
struct Camera2D {
    Vec2f center{};
    float pixels_per_unit = 32.0f;

    /// Column-major 4x4 matrix mapping world (x, y) to clip space for a viewport in pixels.
    [[nodiscard]] std::array<float, 16> view_projection(Vec2f viewport_px) const;
    [[nodiscard]] Vec2f world_to_screen(Vec2f world, Vec2f viewport_px) const;
    [[nodiscard]] Vec2f screen_to_world(Vec2f screen, Vec2f viewport_px) const;
    /// World-space rectangle covered by the viewport (for culling).
    [[nodiscard]] Rect visible_world(Vec2f viewport_px) const;
};

} // namespace gta20
