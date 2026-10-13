#include "gta20/scene/face_texture.hpp"

namespace gta20::scene {
namespace {

// Geometry corner index (block_geometry.cpp) at each viewed position, clockwise from the
// top-left: top-left, top-right, bottom-right, bottom-left. Indexed by BlockFace.
constexpr std::array<std::array<u8, 4>, 5> kViewedCorners{{
    {{0, 1, 2, 3}}, // lid: p0 = (x, y) is the top-left corner on screen
    {{2, 3, 0, 1}}, // left: upper corners p2, p3 on top, seen from -X
    {{2, 3, 0, 1}}, // right: seen from +X
    {{2, 3, 0, 1}}, // top: seen from -Y
    {{2, 3, 0, 1}}, // bottom: seen from +Y
}};

// Image corners in the same clockwise order as the viewed positions.
constexpr std::array<TexCorner, 4> kClockwise{TexCorner::A, TexCorner::B, TexCorner::C, TexCorner::D};

// Horizontal mirror in texture space: the left and right columns swap.
constexpr TexCorner mirrored(TexCorner c) {
    switch (c) {
    case TexCorner::A: return TexCorner::B;
    case TexCorner::B: return TexCorner::A;
    case TexCorner::C: return TexCorner::D;
    case TexCorner::D: return TexCorner::C;
    }
    return c;
}

} // namespace

std::array<TexCorner, 4> face_texture_corners(BlockFace face, assets::Rotation rotation, bool flip) {
    const auto quarter_turns = static_cast<std::size_t>(static_cast<u8>(rotation) & 3u);
    const auto& viewed = kViewedCorners[static_cast<std::size_t>(face)];
    std::array<TexCorner, 4> labels{};
    for (std::size_t position = 0; position < 4; ++position) {
        // A clockwise quarter turn moves each image corner one viewed position further clockwise,
        // so the corner now at `position` came from `quarter_turns` positions back.
        const TexCorner corner = kClockwise[(position + 4 - quarter_turns) % 4];
        // The mirror is applied to the image first, so it relabels the corner that was turned here.
        labels[viewed[position]] = flip ? mirrored(corner) : corner;
    }
    return labels;
}

} // namespace gta20::scene
