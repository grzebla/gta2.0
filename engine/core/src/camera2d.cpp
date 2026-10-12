#include "g2n/core/camera2d.hpp"

namespace g2n {

std::array<float, 16> Camera2D::view_projection(Vec2f viewport_px) const {
    // clip.x =  (world.x - center.x) * ppu * 2 / width
    // clip.y = -(world.y - center.y) * ppu * 2 / height   (world y down, clip y up)
    const float sx = pixels_per_unit * 2.0f / viewport_px.x;
    const float sy = -pixels_per_unit * 2.0f / viewport_px.y;
    std::array<float, 16> m{};
    m[0] = sx;
    m[5] = sy;
    m[10] = 1.0f;
    m[12] = -center.x * sx;
    m[13] = -center.y * sy;
    m[15] = 1.0f;
    return m;
}

Vec2f Camera2D::world_to_screen(Vec2f world, Vec2f viewport_px) const {
    return {(world.x - center.x) * pixels_per_unit + viewport_px.x * 0.5f,
            (world.y - center.y) * pixels_per_unit + viewport_px.y * 0.5f};
}

Vec2f Camera2D::screen_to_world(Vec2f screen, Vec2f viewport_px) const {
    return {(screen.x - viewport_px.x * 0.5f) / pixels_per_unit + center.x,
            (screen.y - viewport_px.y * 0.5f) / pixels_per_unit + center.y};
}

Rect Camera2D::visible_world(Vec2f viewport_px) const {
    return {screen_to_world({0, 0}, viewport_px), screen_to_world(viewport_px, viewport_px)};
}

} // namespace g2n
