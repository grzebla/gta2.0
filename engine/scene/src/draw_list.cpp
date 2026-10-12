#include "g2n/scene/draw_list.hpp"

#include <algorithm>
#include <cmath>

namespace g2n::scene {
namespace {

QuadInstance cell_quad(int x, int y, u8 r, u8 g, u8 b) {
    QuadInstance q;
    q.center_x = static_cast<float>(x) + 0.5f;
    q.center_y = static_cast<float>(y) + 0.5f;
    q.r = r;
    q.g = g;
    q.b = b;
    return q;
}

} // namespace

void append_map(DrawList& list, const assets::GmpMap& map, const Rect& visible) {
    const int x0 = std::max(0, static_cast<int>(std::floor(visible.min.x)));
    const int y0 = std::max(0, static_cast<int>(std::floor(visible.min.y)));
    const int x1 = std::min(assets::kMapWidth - 1, static_cast<int>(std::floor(visible.max.x)));
    const int y1 = std::min(assets::kMapHeight - 1, static_cast<int>(std::floor(visible.max.y)));
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
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

} // namespace g2n::scene
