#include <doctest/doctest.h>

#include "gta20/assets/gmp_writer.hpp"
#include "gta20/scene/block_geometry.hpp"
#include "gta20/scene/draw_list.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <tuple>
#include <vector>

using namespace gta20;

TEST_CASE("Scene: map quads are culled to the visible rectangle") {
    auto map = assets::parse_gmp(assets::build_synthetic_test_map());
    REQUIRE(map);
    scene::DrawList list;
    scene::append_map(list, map.value(), Rect{{96, 96}, {105.5f, 99.5f}});
    CHECK(list.quads().size() == 10 * 4);
    for (const auto& q : list.quads()) {
        CHECK(q.center_x >= 96.5f);
        CHECK(q.center_x <= 105.5f);
    }
    list.clear();
    scene::append_map(list, map.value(), Rect{{0, 0}, {10, 10}}); // empty part of the map
    CHECK(list.quads().empty());
}

TEST_CASE("Scene: entities add a body and a heading marker") {
    scene::DrawList list;
    scene::append_entities(list, {{{10, 10}, 0.0f, 1.0f, 255, 0, 0}});
    REQUIRE(list.quads().size() == 2);
    CHECK(list.quads()[1].center_x == doctest::Approx(10.5));
    CHECK(list.quads()[1].center_y == doctest::Approx(10.0));
}

// Unit-block face geometry. SYNTHETIC expectations written from the local contract in
// block_geometry.hpp (x = map column, y = map row, z up), independent of the implementation.
namespace {

using scene::BlockFace;
using scene::FaceGeometry;
using scene::Position3f;

struct FaceSpec {
    BlockFace face;
    const char* name;
    int axis;   // 0 = x, 1 = y, 2 = z: the coordinate that is constant on the face
    float side; // offset of the face plane from the block origin along `axis`
    Position3f normal;
};

constexpr std::array<FaceSpec, 5> kFaces{{
    {BlockFace::lid, "lid", 2, 1, {0, 0, 1}},
    {BlockFace::left, "left", 0, 0, {-1, 0, 0}},
    {BlockFace::right, "right", 0, 1, {1, 0, 0}},
    {BlockFace::top, "top", 1, 0, {0, -1, 0}},
    {BlockFace::bottom, "bottom", 1, 1, {0, 1, 0}},
}};

constexpr std::array<Position3f, 3> kOrigins{{{0, 0, 0}, {3, 5, 2}, {255, 255, 7}}};

float coord(const Position3f& p, int axis) {
    return axis == 0 ? p.x : axis == 1 ? p.y : p.z;
}

Position3f operator+(Position3f a, Position3f b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Position3f operator-(Position3f a, Position3f b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Position3f cross(Position3f a, Position3f b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float length(Position3f v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

bool less(const Position3f& a, const Position3f& b) {
    return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
}

std::vector<Position3f> sorted(std::vector<Position3f> v) {
    std::sort(v.begin(), v.end(), less);
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
}

std::vector<Position3f> corner_set(const FaceGeometry& g) {
    return sorted({g.positions.begin(), g.positions.end()});
}

// The four corners of the face plane: the constant axis is fixed, the two others take 0 and 1.
std::vector<Position3f> expected_corners(const FaceSpec& spec, Position3f origin) {
    std::vector<Position3f> corners;
    for (int a = 0; a < 2; ++a) {
        for (int b = 0; b < 2; ++b) {
            std::array<float, 3> offset{};
            offset[static_cast<std::size_t>(spec.axis)] = spec.side;
            offset[static_cast<std::size_t>((spec.axis + 1) % 3)] = static_cast<float>(a);
            offset[static_cast<std::size_t>((spec.axis + 2) % 3)] = static_cast<float>(b);
            corners.push_back(origin + Position3f{offset[0], offset[1], offset[2]});
        }
    }
    return sorted(corners);
}

// Corners of `g` at height `z` (one horizontal edge of a wall).
std::vector<Position3f> edge_at_z(const FaceGeometry& g, float z) {
    std::vector<Position3f> edge;
    for (const auto& p : g.positions) {
        if (p.z == z) {
            edge.push_back(p);
        }
    }
    return sorted(edge);
}

std::vector<Position3f> shared_corners(const FaceGeometry& a, const FaceGeometry& b) {
    const auto ca = corner_set(a);
    const auto cb = corner_set(b);
    std::vector<Position3f> shared;
    std::set_intersection(ca.begin(), ca.end(), cb.begin(), cb.end(), std::back_inserter(shared), less);
    return shared;
}

} // namespace

TEST_CASE("[world-geometry] block faces lie on their planes with the expected corners") {
    for (const auto& spec : kFaces) {
        CAPTURE(spec.name);
        for (const auto& origin : kOrigins) {
            CAPTURE(origin.x);
            CAPTURE(origin.y);
            CAPTURE(origin.z);
            const auto g = scene::block_face_geometry(spec.face, origin);
            for (const auto& p : g.positions) {
                CHECK(coord(p, spec.axis) == coord(origin, spec.axis) + spec.side);
            }
            CHECK(corner_set(g).size() == 4);
            CHECK(corner_set(g) == expected_corners(spec, origin));
        }
    }
}

TEST_CASE("[world-geometry] block face triangles wind outward and cover one unit square") {
    for (const auto& spec : kFaces) {
        CAPTURE(spec.name);
        for (const auto& origin : kOrigins) {
            CAPTURE(origin.x);
            const auto g = scene::block_face_geometry(spec.face, origin);
            std::array<int, 4> uses{};
            float area = 0;
            for (std::size_t t = 0; t < 2; ++t) {
                const u8 i0 = g.indices[3 * t];
                const u8 i1 = g.indices[3 * t + 1];
                const u8 i2 = g.indices[3 * t + 2];
                REQUIRE(i0 < 4);
                REQUIRE(i1 < 4);
                REQUIRE(i2 < 4);
                CHECK(i0 != i1);
                CHECK(i1 != i2);
                CHECK(i0 != i2);
                ++uses[i0];
                ++uses[i1];
                ++uses[i2];
                const Position3f n = cross(g.positions[i1] - g.positions[i0], g.positions[i2] - g.positions[i0]);
                CHECK(length(n) > 0.0f); // non-degenerate
                // Half of a unit square has area 1/2, so the cross product is exactly the unit outward normal.
                CHECK(n == spec.normal);
                area += 0.5f * length(n);
            }
            CHECK(area == doctest::Approx(1.0));
            // Both triangles together use all four (distinct) corners and share one edge, the diagonal.
            CHECK(std::count(uses.begin(), uses.end(), 1) == 2);
            CHECK(std::count(uses.begin(), uses.end(), 2) == 2);
            CHECK(corner_set(g).size() == 4);
        }
    }
}

TEST_CASE("[world-geometry] block faces translate with the block origin") {
    for (const auto& spec : kFaces) {
        CAPTURE(spec.name);
        const auto base = scene::block_face_geometry(spec.face, {0, 0, 0});
        for (const auto& origin : kOrigins) {
            CAPTURE(origin.x);
            const auto g = scene::block_face_geometry(spec.face, origin);
            CHECK(g.indices == base.indices);
            for (std::size_t i = 0; i < 4; ++i) {
                CHECK(g.positions[i] == base.positions[i] + origin);
            }
            CHECK(corner_set(g) == expected_corners(spec, origin));
        }
    }
}

TEST_CASE("[world-geometry] adjacent block faces share seams exactly") {
    const auto face = [](BlockFace f, Position3f o) { return scene::block_face_geometry(f, o); };
    for (const auto& o : kOrigins) {
        CAPTURE(o.x);
        CAPTURE(o.y);
        CAPTURE(o.z);
        // Horizontal neighbours: the touching walls coincide.
        CHECK(corner_set(face(BlockFace::right, o)) == corner_set(face(BlockFace::left, o + Position3f{1, 0, 0})));
        CHECK(corner_set(face(BlockFace::bottom, o)) == corner_set(face(BlockFace::top, o + Position3f{0, 1, 0})));

        const Position3f above = o + Position3f{0, 0, 1};
        std::vector<Position3f> upper_edges_below;
        std::vector<Position3f> lower_edges_above;
        for (const auto& spec : kFaces) {
            CAPTURE(spec.name);
            if (spec.face == BlockFace::lid) {
                // Stacked lids are parallel, one unit apart, and share no corner.
                CHECK(shared_corners(face(BlockFace::lid, o), face(BlockFace::lid, above)).empty());
                continue;
            }
            const auto lower = face(spec.face, o);
            const auto upper = face(spec.face, above);
            // Vertically stacked walls share exactly the edge at z + 1 ...
            const auto seam = shared_corners(lower, upper);
            CHECK(seam.size() == 2);
            CHECK(seam == edge_at_z(lower, o.z + 1));
            CHECK(seam == edge_at_z(upper, o.z + 1));
            // ... which is also an edge of the lower block's lid.
            CHECK(shared_corners(lower, face(BlockFace::lid, o)) == seam);
            const auto upper_edge = edge_at_z(lower, o.z + 1);
            const auto lower_edge = edge_at_z(upper, o.z + 1);
            upper_edges_below.insert(upper_edges_below.end(), upper_edge.begin(), upper_edge.end());
            lower_edges_above.insert(lower_edges_above.end(), lower_edge.begin(), lower_edge.end());
        }
        // The lid's corners are exactly the walls' upper corners and the block above's lower corners.
        CHECK(sorted(upper_edges_below) == corner_set(face(BlockFace::lid, o)));
        CHECK(sorted(lower_edges_above) == corner_set(face(BlockFace::lid, o)));
    }
}

TEST_CASE("[world-geometry] block face geometry is identical across repeated calls") {
    for (const auto& spec : kFaces) {
        CAPTURE(spec.name);
        for (const auto& origin : kOrigins) {
            CAPTURE(origin.x);
            const auto first = scene::block_face_geometry(spec.face, origin);
            for (int i = 0; i < 3; ++i) {
                CHECK(scene::block_face_geometry(spec.face, origin) == first);
            }
        }
    }
}
