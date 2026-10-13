#include <doctest/doctest.h>

#include "gta20/assets/gmp_writer.hpp"
#include "gta20/scene/block_geometry.hpp"
#include "gta20/scene/draw_list.hpp"
#include "gta20/scene/face_texture.hpp"
#include "gta20/scene/world_mesh.hpp"
#include "gta20/scene/world_mesh_map.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <set>
#include <string>
#include <tuple>
#include <utility>
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

// Texture-corner mapping. SYNTHETIC expectations written by hand from docs/specs/world-geometry.md,
// sections 3 and 5 (A = (0,0), B = (1,0), C = (1,1), D = (0,1) with v down the image; flip mirrors
// A<->B and D<->C before a clockwise turn), not from the implementation.
// - Lid rows: table 5.1 read at p0..p3 = NW, NE, SE, SW. Current-engine regression behaviour of
//   append_map_textured, not evidence of the original game.
// - Wall rows: hypothesis, see docs/specs/world-geometry.md (sections 3 and 5.3). The face seen from
//   outside has p2, p3 on top and p0, p1 below, so table 5.1's NW, NE, SE, SW become p2, p3, p0, p1.
namespace {

using assets::Rotation;
using scene::TexCorner;

struct MappingCase {
    BlockFace face;
    const char* face_name;
    Rotation rotation;
    bool flip;
    const char* expected; // labels at p0, p1, p2, p3
};

constexpr std::array<MappingCase, 40> kMappingCases{{
    {BlockFace::lid, "lid", Rotation::R0, false, "ABCD"},
    {BlockFace::lid, "lid", Rotation::R90, false, "DABC"},
    {BlockFace::lid, "lid", Rotation::R180, false, "CDAB"},
    {BlockFace::lid, "lid", Rotation::R270, false, "BCDA"},
    {BlockFace::lid, "lid", Rotation::R0, true, "BADC"},
    {BlockFace::lid, "lid", Rotation::R90, true, "CBAD"},
    {BlockFace::lid, "lid", Rotation::R180, true, "DCBA"},
    {BlockFace::lid, "lid", Rotation::R270, true, "ADCB"},

    {BlockFace::left, "left", Rotation::R0, false, "CDAB"},
    {BlockFace::left, "left", Rotation::R90, false, "BCDA"},
    {BlockFace::left, "left", Rotation::R180, false, "ABCD"},
    {BlockFace::left, "left", Rotation::R270, false, "DABC"},
    {BlockFace::left, "left", Rotation::R0, true, "DCBA"},
    {BlockFace::left, "left", Rotation::R90, true, "ADCB"},
    {BlockFace::left, "left", Rotation::R180, true, "BADC"},
    {BlockFace::left, "left", Rotation::R270, true, "CBAD"},

    {BlockFace::right, "right", Rotation::R0, false, "CDAB"},
    {BlockFace::right, "right", Rotation::R90, false, "BCDA"},
    {BlockFace::right, "right", Rotation::R180, false, "ABCD"},
    {BlockFace::right, "right", Rotation::R270, false, "DABC"},
    {BlockFace::right, "right", Rotation::R0, true, "DCBA"},
    {BlockFace::right, "right", Rotation::R90, true, "ADCB"},
    {BlockFace::right, "right", Rotation::R180, true, "BADC"},
    {BlockFace::right, "right", Rotation::R270, true, "CBAD"},

    {BlockFace::top, "top", Rotation::R0, false, "CDAB"},
    {BlockFace::top, "top", Rotation::R90, false, "BCDA"},
    {BlockFace::top, "top", Rotation::R180, false, "ABCD"},
    {BlockFace::top, "top", Rotation::R270, false, "DABC"},
    {BlockFace::top, "top", Rotation::R0, true, "DCBA"},
    {BlockFace::top, "top", Rotation::R90, true, "ADCB"},
    {BlockFace::top, "top", Rotation::R180, true, "BADC"},
    {BlockFace::top, "top", Rotation::R270, true, "CBAD"},

    {BlockFace::bottom, "bottom", Rotation::R0, false, "CDAB"},
    {BlockFace::bottom, "bottom", Rotation::R90, false, "BCDA"},
    {BlockFace::bottom, "bottom", Rotation::R180, false, "ABCD"},
    {BlockFace::bottom, "bottom", Rotation::R270, false, "DABC"},
    {BlockFace::bottom, "bottom", Rotation::R0, true, "DCBA"},
    {BlockFace::bottom, "bottom", Rotation::R90, true, "ADCB"},
    {BlockFace::bottom, "bottom", Rotation::R180, true, "BADC"},
    {BlockFace::bottom, "bottom", Rotation::R270, true, "CBAD"},
}};

const char* rotation_name(Rotation r) {
    switch (r) {
    case Rotation::R0: return "R0";
    case Rotation::R90: return "R90";
    case Rotation::R180: return "R180";
    case Rotation::R270: return "R270";
    }
    return "R?";
}

std::string label_string(const std::array<TexCorner, 4>& corners) {
    std::string s;
    for (const TexCorner c : corners) {
        switch (c) {
        case TexCorner::A: s += 'A'; break;
        case TexCorner::B: s += 'B'; break;
        case TexCorner::C: s += 'C'; break;
        case TexCorner::D: s += 'D'; break;
        default: s += '?'; break;
        }
    }
    return s;
}

bool is_permutation_of_abcd(std::string s) {
    std::sort(s.begin(), s.end());
    return s == "ABCD";
}

std::string describe(const MappingCase& c) {
    return std::string(c.face_name) + " " + rotation_name(c.rotation) + (c.flip ? " flip" : " noflip");
}

} // namespace

TEST_CASE("[world-geometry] face texture corners match the specified labels for all 40 combinations") {
    std::set<std::tuple<int, int, bool>> seen;
    std::string report;
    for (const auto& c : kMappingCases) {
        const std::string context = describe(c);
        CAPTURE(context);
        CAPTURE(c.expected);
        CHECK(is_permutation_of_abcd(c.expected)); // sanity of the hand-written table
        seen.emplace(static_cast<int>(c.face), static_cast<int>(c.rotation), c.flip);
        const std::string actual = label_string(scene::face_texture_corners(c.face, c.rotation, c.flip));
        CAPTURE(actual);
        CHECK(is_permutation_of_abcd(actual));
        CHECK(actual == c.expected);
        report += "\n  " + context + ": p0..p3 = " + actual + (actual == c.expected ? " ok" : " MISMATCH");
    }
    // Every face x {R0, R90, R180, R270} x {no flip, flip} exactly once.
    CHECK(seen.size() == 40);
    MESSAGE("face/rotation/flip combinations checked: " << seen.size() << report);
}

TEST_CASE("[world-geometry] face texture corners read only the two rotation bits") {
    for (const auto& c : kMappingCases) {
        const std::string context = describe(c);
        CAPTURE(context);
        const auto wide = static_cast<Rotation>(static_cast<u8>(c.rotation) + 4);
        CHECK(label_string(scene::face_texture_corners(c.face, wide, c.flip)) == c.expected);
    }
}

TEST_CASE("[world-geometry] unrotated texture axes follow the section 3 proposal") {
    // u = direction A -> B, v = direction A -> D (docs/specs/world-geometry.md, section 3 table).
    struct Axes {
        BlockFace face;
        const char* name;
        Position3f u, v;
    };
    constexpr std::array<Axes, 5> kAxes{{
        {BlockFace::lid, "lid", {1, 0, 0}, {0, 1, 0}},
        {BlockFace::left, "left", {0, 1, 0}, {0, 0, -1}},
        {BlockFace::right, "right", {0, -1, 0}, {0, 0, -1}},
        {BlockFace::top, "top", {-1, 0, 0}, {0, 0, -1}},
        {BlockFace::bottom, "bottom", {1, 0, 0}, {0, 0, -1}},
    }};
    for (const auto& axes : kAxes) {
        CAPTURE(axes.name);
        const auto g = scene::block_face_geometry(axes.face, {3, 5, 2});
        const auto labels = scene::face_texture_corners(axes.face, Rotation::R0, false);
        std::array<Position3f, 4> at{}; // positions of A, B, C, D
        for (std::size_t i = 0; i < 4; ++i) {
            at[static_cast<std::size_t>(labels[i])] = g.positions[i];
        }
        CHECK(at[1] - at[0] == axes.u);
        CHECK(at[3] - at[0] == axes.v);
        CHECK(at[2] == at[0] + axes.u + axes.v);
    }
}

TEST_CASE("[world-geometry] lid texture corners match the current append_map_textured output") {
    // SYNTHETIC map: one lid per lid combination, tile 1, in a row at y = 20, z = 3.
    constexpr u16 kTile = 1;
    constexpr int kY = 20;
    constexpr int kZ = 3;
    std::vector<MappingCase> lids;
    std::copy_if(kMappingCases.begin(), kMappingCases.end(), std::back_inserter(lids),
                 [](const MappingCase& c) { return c.face == BlockFace::lid; });
    REQUIRE(lids.size() == 8);
    assets::GmpWriter writer;
    for (std::size_t i = 0; i < lids.size(); ++i) {
        assets::BlockInfo block;
        block.lid =
            static_cast<u16>(kTile | (lids[i].flip ? 1u << 13 : 0u) | (static_cast<unsigned>(lids[i].rotation) << 14));
        writer.set_block(10 + 2 * static_cast<int>(i), kY, kZ, block);
    }
    auto map = assets::parse_gmp(writer.build_dmap());
    REQUIRE(map);
    scene::DrawList list;
    scene::append_map_textured(list, map.value(), Rect{{0, 0}, {255, 255}}, kTile + 1);
    REQUIRE(list.quads().size() == lids.size());

    const scene::UvRect slot = scene::TileAtlas::slot_uv(kTile); // unflipped: u0 left, v0 top
    // Documented quad transformation (quad.vert.glsl, draw_list.hpp): unit corner a in [-1/2, 1/2]^2
    // is scaled by the size, rotated clockwise on screen (y down) by `rotation` about the centre and
    // samples mix(uv0, uv1, a + 1/2).
    constexpr std::array<std::array<float, 2>, 4> kUnitCorners{
        {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}}};
    std::size_t checked = 0;
    std::string report;
    for (const auto& q : list.quads()) {
        const auto index = static_cast<std::size_t>(std::lround((q.center_x - 10.5f) / 2.0f));
        REQUIRE(index < lids.size());
        const MappingCase& c = lids[index];
        const std::string context = describe(c);
        CAPTURE(context);
        const int x = 10 + 2 * static_cast<int>(index);
        CHECK(q.center_x == doctest::Approx(x + 0.5));
        CHECK(q.center_y == doctest::Approx(kY + 0.5));
        const auto g = scene::block_face_geometry(BlockFace::lid, {static_cast<float>(x), static_cast<float>(kY), kZ});

        std::string drawn = "????"; // label found at each fixed geometric corner p0..p3
        const float s = std::sin(q.rotation);
        const float k = std::cos(q.rotation);
        for (const auto& a : kUnitCorners) {
            const float cx = a[0] * q.width;
            const float cy = a[1] * q.height;
            const float wx = q.center_x + cx * k - cy * s;
            const float wy = q.center_y + cx * s + cy * k;
            const float u = q.uv.u0 + (q.uv.u1 - q.uv.u0) * (a[0] + 0.5f);
            const float v = q.uv.v0 + (q.uv.v1 - q.uv.v0) * (a[1] + 0.5f);
            const bool left = std::abs(u - slot.u0) < std::abs(u - slot.u1);
            const bool upper = std::abs(v - slot.v0) < std::abs(v - slot.v1);
            const char label = upper ? (left ? 'A' : 'B') : (left ? 'D' : 'C');
            bool found = false;
            for (std::size_t p = 0; p < 4; ++p) {
                if (std::abs(g.positions[p].x - wx) < 1e-3f && std::abs(g.positions[p].y - wy) < 1e-3f) {
                    drawn[p] = label;
                    found = true;
                }
            }
            CHECK(found);
        }
        CAPTURE(drawn);
        CHECK(is_permutation_of_abcd(drawn));
        CHECK(drawn == c.expected);
        CHECK(label_string(scene::face_texture_corners(BlockFace::lid, c.rotation, c.flip)) == drawn);
        ++checked;
        report += "\n  " + context + ": append_map_textured p0..p3 = " + drawn;
    }
    CHECK(checked == 8);
    MESSAGE("existing-lid regression combinations checked: " << checked << report);
}

// World mesh assembly. SYNTHETIC expectations: corner positions are the literal unit-block corners of
// the block_geometry.hpp contract, labels come from the hand-written kMappingCases table and atlas
// rectangles are written out from the 32 x 32 grid of 64-pixel slots with half-texel insets. None of
// them calls block_face_geometry, face_texture_corners or TileAtlas::slot_uv. This is an assembly
// contract only: it is not evidence of the original game's elevation, wall orientation or visibility.
namespace {

using scene::FaceTile;
using scene::WorldMesh;
using scene::WorldVertex;

// p0..p3 of each face of the block at the origin, indexed by BlockFace (block_geometry.hpp).
constexpr std::array<std::array<Position3f, 4>, 5> kExpectedCorners{{
    {{{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}}, // lid
    {{{0, 1, 0}, {0, 0, 0}, {0, 0, 1}, {0, 1, 1}}}, // left
    {{{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}}}, // right
    {{{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}}, // top
    {{{1, 1, 0}, {0, 1, 0}, {0, 1, 1}, {1, 1, 1}}}, // bottom
}};

constexpr std::array<u32, 6> kQuadPattern{0, 1, 2, 0, 2, 3};
constexpr u16 kStyleTiles = 992; // a full style: tiles 0-991

struct ExpectedSlot {
    float u0, v0, u1, v1;
};

// Slot rectangle of `tile` in the 2048 x 2048 atlas, half a texel inside the 64 x 64 slot. All values
// are exact in float (multiples of 1/4096), so they can be compared with ==.
ExpectedSlot expected_slot(u16 tile) {
    const auto col = static_cast<float>(tile % 32);
    const auto row = static_cast<float>(tile / 32);
    return {(64.0f * col + 0.5f) / 2048.0f, (64.0f * row + 0.5f) / 2048.0f, (64.0f * col + 63.5f) / 2048.0f,
            (64.0f * row + 63.5f) / 2048.0f};
}

// Atlas UV of image corner `label` ('A'..'D', v down the image).
std::array<float, 2> expected_uv(u16 tile, char label) {
    const ExpectedSlot s = expected_slot(tile);
    switch (label) {
    case 'A': return {s.u0, s.v0};
    case 'B': return {s.u1, s.v0};
    case 'C': return {s.u1, s.v1};
    default: return {s.u0, s.v1};
    }
}

Position3f outward_normal(BlockFace face) {
    for (const auto& spec : kFaces) {
        if (spec.face == face) {
            return spec.normal;
        }
    }
    return {};
}

bool finite(const WorldVertex& v) {
    return std::isfinite(v.position.x) && std::isfinite(v.position.y) && std::isfinite(v.position.z) &&
           std::isfinite(v.u) && std::isfinite(v.v);
}

// Checks the face appended last: positions, rebased indices, outward winding, finite values and UVs
// inside the tile's slot. `base` is the vertex count before the append.
void check_appended_face(const WorldMesh& mesh, std::size_t base, BlockFace face, Position3f origin, u16 tile) {
    REQUIRE(mesh.vertices().size() == base + 4);
    REQUIRE(mesh.indices().size() == base / 4 * 6 + 6);
    const auto& corners = kExpectedCorners[static_cast<std::size_t>(face)];
    const ExpectedSlot slot = expected_slot(tile);
    for (std::size_t i = 0; i < 4; ++i) {
        const WorldVertex& v = mesh.vertices()[base + i];
        CHECK(v.position == origin + corners[i]);
        CHECK(finite(v));
        CHECK(v.u >= slot.u0);
        CHECK(v.u <= slot.u1);
        CHECK(v.v >= slot.v0);
        CHECK(v.v <= slot.v1);
        CHECK(v.u > 0.0f);
        CHECK(v.u < 1.0f);
        CHECK(v.v > 0.0f);
        CHECK(v.v < 1.0f);
    }
    const std::size_t first = mesh.indices().size() - 6;
    for (std::size_t k = 0; k < 6; ++k) {
        CHECK(mesh.indices()[first + k] == static_cast<u32>(base) + kQuadPattern[k]);
        CHECK(mesh.indices()[first + k] < mesh.vertices().size());
    }
    for (std::size_t t = 0; t < 2; ++t) {
        const Position3f p0 = mesh.vertices()[mesh.indices()[first + 3 * t]].position;
        const Position3f p1 = mesh.vertices()[mesh.indices()[first + 3 * t + 1]].position;
        const Position3f p2 = mesh.vertices()[mesh.indices()[first + 3 * t + 2]].position;
        CHECK(cross(p1 - p0, p2 - p0) == outward_normal(face)); // outward, half a unit square
    }
}

} // namespace

TEST_CASE("WorldMesh: all 40 face/rotation/flip combinations give the expected corners and atlas UVs") {
    constexpr std::array<Position3f, 2> kCaseOrigins{{{3, 5, 2}, {255, 255, 7}}};
    std::set<std::tuple<int, int, bool>> seen;
    WorldMesh mesh;
    for (std::size_t n = 0; n < kMappingCases.size(); ++n) {
        const MappingCase& c = kMappingCases[n];
        const std::string context = describe(c);
        CAPTURE(context);
        const auto tile = static_cast<u16>(1 + 25 * n); // 1 .. 976: many atlas rows and columns
        const Position3f origin = kCaseOrigins[n % kCaseOrigins.size()];
        CAPTURE(tile);
        const std::size_t base = mesh.vertices().size();
        REQUIRE(mesh.append_face(c.face, origin, FaceTile{tile, c.rotation, c.flip}, kStyleTiles).ok());
        check_appended_face(mesh, base, c.face, origin, tile);
        for (std::size_t i = 0; i < 4; ++i) {
            CAPTURE(i);
            const auto uv = expected_uv(tile, c.expected[i]);
            CHECK(mesh.vertices()[base + i].u == uv[0]);
            CHECK(mesh.vertices()[base + i].v == uv[1]);
        }
        seen.emplace(static_cast<int>(c.face), static_cast<int>(c.rotation), c.flip);
    }
    CHECK(seen.size() == 40);
    CHECK(mesh.vertices().size() == 160);
    CHECK(mesh.indices().size() == 240);
}

TEST_CASE("WorldMesh: faces at different origins are rebased and leave earlier contents unchanged") {
    struct Append {
        BlockFace face;
        Position3f origin;
        FaceTile texture;
    };
    const std::array<Append, 7> appends{{
        {BlockFace::lid, {0, 0, 0}, {1, Rotation::R0, false}},
        {BlockFace::left, {255, 255, 7}, {991, Rotation::R90, true}},
        {BlockFace::right, {12, 40, 3}, {32, Rotation::R180, false}},
        {BlockFace::top, {255, 0, 7}, {63, Rotation::R270, true}},
        {BlockFace::bottom, {0, 255, 0}, {500, Rotation::R0, true}},
        {BlockFace::lid, {255, 255, 7}, {2, Rotation::R90, false}},
        {BlockFace::lid, {12, 40, 3}, {2, Rotation::R90, false}}, // same face and texture, other origin
    }};
    WorldMesh mesh;
    for (std::size_t n = 0; n < appends.size(); ++n) {
        const Append& a = appends[n];
        CAPTURE(n);
        const std::vector<WorldVertex> vertices_before = mesh.vertices();
        const std::vector<u32> indices_before = mesh.indices();
        REQUIRE(mesh.append_face(a.face, a.origin, a.texture, kStyleTiles).ok());
        check_appended_face(mesh, vertices_before.size(), a.face, a.origin, a.texture.tile);
        CHECK(std::equal(vertices_before.begin(), vertices_before.end(), mesh.vertices().begin()));
        CHECK(std::equal(indices_before.begin(), indices_before.end(), mesh.indices().begin()));
    }
    REQUIRE(mesh.vertices().size() == 4 * appends.size());
    REQUIRE(mesh.indices().size() == 6 * appends.size());
    // Every index of the finished mesh is in bounds and each face uses only its own four vertices.
    for (std::size_t i = 0; i < mesh.indices().size(); ++i) {
        CHECK(mesh.indices()[i] < mesh.vertices().size());
        CHECK(mesh.indices()[i] / 4 == i / 6);
    }
    // The two lids with the same tile and transform have the same UVs and translated corners.
    for (std::size_t i = 0; i < 4; ++i) {
        const WorldVertex& high = mesh.vertices()[20 + i];
        const WorldVertex& mid = mesh.vertices()[24 + i];
        CHECK(high.u == mid.u);
        CHECK(high.v == mid.v);
        CHECK(high.position - mid.position == Position3f{243, 215, 4});
    }
    CHECK(mesh.vertices()[20].position == Position3f{255, 255, 8}); // lid of 255,255,7 at z + 1

    mesh.clear();
    CHECK(mesh.vertices().empty());
    CHECK(mesh.indices().empty());
    REQUIRE(mesh.append_face(BlockFace::top, {7, 7, 1}, {3, Rotation::R0, false}, kStyleTiles).ok());
    check_appended_face(mesh, 0, BlockFace::top, {7, 7, 1}, 3);
}

TEST_CASE("WorldMesh: blank, missing and reserved tiles are rejected without modifying the mesh") {
    WorldMesh mesh;
    REQUIRE(mesh.append_face(BlockFace::lid, {1, 2, 3}, {5, Rotation::R90, true}, kStyleTiles).ok());
    const std::vector<WorldVertex> vertices = mesh.vertices();
    const std::vector<u32> indices = mesh.indices();

    struct Rejected {
        const char* why;
        u16 tile;
        u16 tile_count;
    };
    constexpr std::array<Rejected, 12> kRejected{{
        {"blank", 0, kStyleTiles},
        {"blank, empty style", 0, 0},
        {"missing, empty style", 1, 0},
        {"missing, tile == tile_count", 40, 40},
        {"missing, tile > tile_count", 41, 40},
        {"first reserved", 992, kStyleTiles},
        {"reserved although tile_count covers it", 992, 1023},
        {"reserved", 1000, 1024},
        {"last reserved", 1022, 1024},
        {"map marker 1023", 1023, 1024},
        {"atlas white slot", scene::TileAtlas::kWhiteSlot, kStyleTiles},
        {"outside the 10-bit range", 1024, 65535},
    }};
    for (const auto& r : kRejected) {
        CAPTURE(r.why);
        CAPTURE(r.tile);
        CAPTURE(r.tile_count);
        CHECK_FALSE(scene::is_drawable_tile(r.tile, r.tile_count));
        for (const auto& spec : kFaces) {
            CAPTURE(spec.name);
            for (const bool flip : {false, true}) {
                const auto result =
                    mesh.append_face(spec.face, {9, 9, 1}, {r.tile, Rotation::R180, flip}, r.tile_count);
                REQUIRE_FALSE(result.ok());
                CHECK_FALSE(result.error().message.empty());
                CHECK(mesh.vertices() == vertices);
                CHECK(mesh.indices() == indices);
            }
        }
    }

    // An unknown face and non-finite corners are rejected the same way.
    const float inf = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(mesh.append_face(static_cast<BlockFace>(5), {0, 0, 0}, {1, Rotation::R0, false}, kStyleTiles).ok());
    CHECK_FALSE(mesh.append_face(BlockFace::lid, {nan, 0, 0}, {1, Rotation::R0, false}, kStyleTiles).ok());
    CHECK_FALSE(mesh.append_face(BlockFace::left, {0, inf, 0}, {1, Rotation::R0, false}, kStyleTiles).ok());
    CHECK_FALSE(mesh.append_face(BlockFace::top, {0, 0, -inf}, {1, Rotation::R0, false}, kStyleTiles).ok());
    CHECK(mesh.vertices() == vertices);
    CHECK(mesh.indices() == indices);

    // Drawable boundaries are accepted.
    struct Accepted {
        u16 tile;
        u16 tile_count;
    };
    constexpr std::array<Accepted, 4> kAccepted{{{1, 2}, {39, 40}, {991, kStyleTiles}, {991, 1024}}};
    for (const auto& a : kAccepted) {
        CAPTURE(a.tile);
        CAPTURE(a.tile_count);
        CHECK(scene::is_drawable_tile(a.tile, a.tile_count));
        const std::size_t base = mesh.vertices().size();
        REQUIRE(mesh.append_face(BlockFace::right, {4, 4, 4}, {a.tile, Rotation::R0, false}, a.tile_count).ok());
        check_appended_face(mesh, base, BlockFace::right, {4, 4, 4}, a.tile);
    }
}

TEST_CASE("WorldMesh: a batch of more than 65535 vertices keeps full u32 indices") {
    constexpr std::size_t kFacesInBatch = 16400; // 65600 vertices
    WorldMesh mesh;
    const auto origin_of = [](std::size_t f) {
        return Position3f{static_cast<float>(f % 256), static_cast<float>((f / 256) % 256), static_cast<float>(f % 8)};
    };
    const auto face_of = [](std::size_t f) { return static_cast<BlockFace>(f % 5); };
    const auto tile_of = [](std::size_t f) { return static_cast<u16>(1 + f % 991); };
    for (std::size_t f = 0; f < kFacesInBatch; ++f) {
        const auto result = mesh.append_face(face_of(f), origin_of(f), {tile_of(f), Rotation::R0, false}, kStyleTiles);
        REQUIRE(result.ok());
    }
    REQUIRE(mesh.vertices().size() == 4 * kFacesInBatch);
    REQUIRE(mesh.indices().size() == 6 * kFacesInBatch);
    CHECK(mesh.vertices().size() > 65535);

    std::size_t wrong_index = 0;
    std::size_t wrong_position = 0;
    std::size_t non_finite = 0;
    u32 largest = 0;
    for (std::size_t f = 0; f < kFacesInBatch; ++f) {
        for (std::size_t k = 0; k < 6; ++k) {
            const u32 index = mesh.indices()[6 * f + k];
            largest = std::max(largest, index);
            if (index != static_cast<u32>(4 * f) + kQuadPattern[k] || index >= mesh.vertices().size()) {
                ++wrong_index;
            }
        }
        const auto& corners = kExpectedCorners[f % 5];
        for (std::size_t i = 0; i < 4; ++i) {
            const WorldVertex& v = mesh.vertices()[4 * f + i];
            if (!(v.position == origin_of(f) + corners[i])) {
                ++wrong_position;
            }
            if (!finite(v)) {
                ++non_finite;
            }
        }
    }
    CHECK(wrong_index == 0);
    CHECK(wrong_position == 0);
    CHECK(non_finite == 0);
    CHECK(largest == 4 * kFacesInBatch - 1);
    // Face 16384 is the first whose indices do not fit in 16 bits.
    CHECK(mesh.indices()[6 * 16384] == 65536u);
    CHECK(mesh.indices()[6 * 16384 + 2] == 65538u);
    // Appending to the large mesh still rebases and keeps the earlier contents.
    const WorldVertex last = mesh.vertices().back();
    REQUIRE(mesh.append_face(BlockFace::lid, {255, 255, 7}, {991, Rotation::R270, true}, kStyleTiles).ok());
    check_appended_face(mesh, 4 * kFacesInBatch, BlockFace::lid, {255, 255, 7}, 991);
    CHECK(mesh.vertices()[4 * kFacesInBatch - 1] == last);
}

// Map mesh builder. SYNTHETIC GmpWriter fixtures; expectations follow the partial, provisional builder
// contract in docs/specs/world-mesh.md (cell x,y,z = block origin, every drawable face of an unsloped
// block emitted, no collision-bit or neighbour gating). Local convention only: none of these counts
// is evidence of which faces, heights or textures the original game draws.
namespace {

using scene::MapMesh;
using scene::MapMeshCoverage;
using scene::MapMeshStats;

constexpr u32 kMapCells = 256u * 256u * 8u;

constexpr u16 side_word(u16 tile, Rotation rotation = Rotation::R0, bool flip = false, bool wall = false,
                        bool bullet_wall = false, bool flat = false) {
    return static_cast<u16>(tile | (wall ? 1u << 10 : 0u) | (bullet_wall ? 1u << 11 : 0u) | (flat ? 1u << 12 : 0u) |
                            (flip ? 1u << 13 : 0u) | (static_cast<unsigned>(rotation) << 14));
}

constexpr u16 lid_word(u16 tile, Rotation rotation = Rotation::R0, bool flip = false, unsigned lighting = 0,
                       bool flat = false) {
    return static_cast<u16>(tile | ((lighting & 3u) << 10) | (flat ? 1u << 12 : 0u) | (flip ? 1u << 13 : 0u) |
                            (static_cast<unsigned>(rotation) << 14));
}

constexpr u8 slope_type(unsigned slope, unsigned ground) {
    return static_cast<u8>((slope << 2) | ground);
}

// Unsloped block whose five faces all show `tile` with R0, no flip and no flags.
assets::BlockInfo full_block(u16 tile) {
    assets::BlockInfo block;
    block.left = block.right = block.top = block.bottom = block.lid = tile;
    return block;
}

// Block with only a slope_type byte (ground type and/or slope code) and five blank face words.
assets::BlockInfo bare_block(u8 slope_type_byte) {
    assets::BlockInfo block;
    block.slope_type = slope_type_byte;
    return block;
}

assets::BlockInfo faces(u16 left, u16 right, u16 top, u16 bottom, u16 lid) {
    assets::BlockInfo block;
    block.left = left;
    block.right = right;
    block.top = top;
    block.bottom = bottom;
    block.lid = lid;
    return block;
}

MapMesh build_map(const assets::GmpWriter& writer, u16 tile_count) {
    auto map = assets::parse_gmp(writer.build_dmap());
    REQUIRE(map);
    auto built = scene::build_unsloped_map_mesh(map.value(), tile_count);
    REQUIRE(built);
    return std::move(built).value();
}

template <std::size_t N>
u32 sum(const std::array<u32, N>& values) {
    u32 total = 0;
    for (const u32 v : values) {
        total += v;
    }
    return total;
}

// Contract invariants of every build: the counters add up, the mesh has four vertices and six rebased
// indices per emitted face, every vertex is finite and inside [0, 256] x [0, 256] x [0, 8], and each
// face's UVs are corners of exactly one drawable atlas slot (never the white slot 1023).
void check_map_mesh(const MapMesh& built, u16 tile_count) {
    const MapMeshStats& s = built.stats;
    CHECK(s.cells_visited == kMapCells);
    CHECK(s.empty_cells + s.sloped_blocks + s.unsloped_blocks == kMapCells);
    CHECK(5 * s.unsloped_blocks ==
          s.emitted_faces + s.flat_faces + s.blank_faces + s.reserved_faces + s.out_of_style_faces);
    CHECK(sum(s.emitted_by_face) == s.emitted_faces);
    CHECK(sum(s.flat_by_face) == s.flat_faces);
    CHECK(sum(s.sloped_blocks_by_code) == s.sloped_blocks);
    CHECK(s.sloped_blocks_by_code[0] == 0);
    const bool unsupported =
        s.sloped_blocks != 0 || s.flat_faces != 0 || s.reserved_faces != 0 || s.out_of_style_faces != 0;
    CHECK(s.has_unsupported() == unsupported);
    CHECK(s.coverage() == (unsupported ? MapMeshCoverage::partial : MapMeshCoverage::provisional));

    const auto& vertices = built.mesh.vertices();
    const auto& indices = built.mesh.indices();
    REQUIRE(vertices.size() == 4u * s.emitted_faces);
    REQUIRE(indices.size() == 6u * s.emitted_faces);
    std::size_t wrong_index = 0;
    std::size_t bad_vertex = 0;
    std::size_t bad_uv = 0;
    for (std::size_t f = 0; f < s.emitted_faces; ++f) {
        for (std::size_t k = 0; k < 6; ++k) {
            const u32 index = indices[6 * f + k];
            if (index != static_cast<u32>(4 * f) + kQuadPattern[k] || index >= vertices.size()) {
                ++wrong_index;
            }
        }
        // The slot is identified from the first corner; all four must be corners of that slot.
        const WorldVertex& first = vertices[4 * f];
        const auto tile = static_cast<u16>(static_cast<int>(first.v * 32.0f) * 32 + static_cast<int>(first.u * 32.0f));
        if (!scene::is_drawable_tile(tile, tile_count) || tile == scene::TileAtlas::kWhiteSlot) {
            ++bad_uv;
        }
        const ExpectedSlot slot = expected_slot(tile);
        for (std::size_t i = 0; i < 4; ++i) {
            const WorldVertex& v = vertices[4 * f + i];
            const Position3f& p = v.position;
            if (!finite(v) || p.x < 0 || p.x > 256 || p.y < 0 || p.y > 256 || p.z < 0 || p.z > 8) {
                ++bad_vertex;
            }
            if ((v.u != slot.u0 && v.u != slot.u1) || (v.v != slot.v0 && v.v != slot.v1)) {
                ++bad_uv;
            }
        }
    }
    CHECK(wrong_index == 0);
    CHECK(bad_vertex == 0);
    CHECK(bad_uv == 0);
}

// Emitted faces whose p0..p3 are exactly `face` of the block at `origin` (kExpectedCorners).
std::vector<std::size_t> find_faces(const MapMesh& built, BlockFace face, Position3f origin) {
    const auto& corners = kExpectedCorners[static_cast<std::size_t>(face)];
    const auto& vertices = built.mesh.vertices();
    std::vector<std::size_t> found;
    for (std::size_t f = 0; 4 * f < vertices.size(); ++f) {
        bool same = true;
        for (std::size_t i = 0; i < 4; ++i) {
            same = same && vertices[4 * f + i].position == origin + corners[i];
        }
        if (same) {
            found.push_back(f);
        }
    }
    return found;
}

// The UVs of emitted face `f` are the image corners `labels` (p0..p3, copied by hand from
// kMappingCases) of `tile`'s atlas slot.
void check_face_uv(const MapMesh& built, std::size_t f, u16 tile, const char* labels) {
    CAPTURE(f);
    CAPTURE(labels);
    for (std::size_t i = 0; i < 4; ++i) {
        CAPTURE(i);
        const auto uv = expected_uv(tile, labels[i]);
        CHECK(built.mesh.vertices()[4 * f + i].u == uv[0]);
        CHECK(built.mesh.vertices()[4 * f + i].v == uv[1]);
    }
}

// The block at `origin` contributes exactly one copy of `face`, textured with `labels` of `tile`.
void check_single_face(const MapMesh& built, BlockFace face, Position3f origin, u16 tile, const char* labels) {
    const auto found = find_faces(built, face, origin);
    REQUIRE(found.size() == 1);
    check_face_uv(built, found.front(), tile, labels);
}

Position3f origin_of(int x, int y, int z) {
    return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
}

} // namespace

TEST_CASE("WorldMeshMap: an empty map visits all 256x256x8 cells and emits nothing") {
    const MapMesh built = build_map(assets::GmpWriter{}, kStyleTiles);
    check_map_mesh(built, kStyleTiles);
    MapMeshStats expected;
    expected.cells_visited = kMapCells;
    expected.empty_cells = kMapCells;
    CHECK(built.stats == expected);
    CHECK(built.mesh.vertices().empty());
    CHECK(built.mesh.indices().empty());
    CHECK(built.stats.coverage() == MapMeshCoverage::provisional);
}

TEST_CASE("WorldMeshMap: one block emits its lid and all four walls with transformed UVs") {
    assets::GmpWriter writer;
    writer.set_block(10, 20, 3,
                     faces(side_word(6), side_word(7, Rotation::R180, true), side_word(8, Rotation::R270),
                           side_word(9, Rotation::R90, true), lid_word(5, Rotation::R90, true, 3)));
    const MapMesh built = build_map(writer, kStyleTiles);
    check_map_mesh(built, kStyleTiles);

    const MapMeshStats& s = built.stats;
    CHECK(s.empty_cells == kMapCells - 1);
    CHECK(s.unsloped_blocks == 1);
    CHECK(s.emitted_faces == 5);
    CHECK(s.emitted_by_face == std::array<u32, 5>{1, 1, 1, 1, 1});
    CHECK(s.blank_faces + s.flat_faces + s.reserved_faces + s.out_of_style_faces + s.sloped_blocks == 0);
    CHECK(s.coverage() == MapMeshCoverage::provisional);

    // Faces follow BlockFace order; cell 10,20,3 is the block origin, so the lid lies at height 4.
    const Position3f origin{10, 20, 3};
    for (const auto face : {BlockFace::lid, BlockFace::left, BlockFace::right, BlockFace::top, BlockFace::bottom}) {
        CHECK(find_faces(built, face, origin) == std::vector<std::size_t>{static_cast<std::size_t>(face)});
    }
    const auto& v = built.mesh.vertices();
    CHECK(v[0].position == Position3f{10, 20, 4});  // lid p0
    CHECK(v[2].position == Position3f{11, 21, 4});  // lid p2
    CHECK(v[4].position == Position3f{10, 21, 3});  // left p0
    CHECK(v[8].position == Position3f{11, 20, 3});  // right p0
    CHECK(v[12].position == Position3f{10, 20, 3}); // top p0
    CHECK(v[18].position == Position3f{10, 21, 4}); // bottom p2
    check_face_uv(built, 0, 5, "CBAD");             // lid R90 flip (lighting bits ignored)
    check_face_uv(built, 1, 6, "CDAB");             // left R0
    check_face_uv(built, 2, 7, "BADC");             // right R180 flip
    check_face_uv(built, 3, 8, "DABC");             // top R270
    check_face_uv(built, 4, 9, "ADCB");             // bottom R90 flip
}

TEST_CASE("WorldMeshMap: stacked blocks, gaps between levels and touching neighbours keep every face") {
    assets::GmpWriter writer;
    for (const int z : {0, 1, 2, 5}) { // levels 3 and 4 of the column stay empty
        writer.set_block(100, 100, z, full_block(12));
    }
    writer.set_block(60, 60, 4, full_block(13));
    writer.set_block(61, 60, 4, full_block(13));
    const MapMesh built = build_map(writer, kStyleTiles);
    check_map_mesh(built, kStyleTiles);

    const MapMeshStats& s = built.stats;
    CHECK(s.unsloped_blocks == 6);
    CHECK(s.emitted_faces == 30);
    CHECK(s.emitted_by_face == std::array<u32, 5>{6, 6, 6, 6, 6});
    CHECK(s.blank_faces == 0);
    CHECK(s.coverage() == MapMeshCoverage::provisional);

    // No neighbour elimination: covered lids and touching walls are emitted once each.
    for (const int z : {0, 1, 2, 5}) {
        CAPTURE(z);
        const Position3f origin = origin_of(100, 100, z);
        check_single_face(built, BlockFace::lid, origin, 12, "ABCD");
        check_single_face(built, BlockFace::right, origin, 12, "CDAB");
        const auto lid = find_faces(built, BlockFace::lid, origin);
        CHECK(built.mesh.vertices()[4 * lid.front()].position.z == static_cast<float>(z + 1));
    }
    for (const int z : {3, 4}) {
        CAPTURE(z);
        for (const auto& spec : kFaces) {
            CHECK(find_faces(built, spec.face, origin_of(100, 100, z)).empty());
        }
    }
    check_single_face(built, BlockFace::right, {60, 60, 4}, 13, "CDAB"); // plane x = 61
    check_single_face(built, BlockFace::left, {61, 60, 4}, 13, "CDAB");  // the same plane, facing -X
    // Nothing of the column lies strictly between the lid of level 2 (z = 3) and the base of level 5.
    std::size_t in_gap = 0;
    for (const auto& vertex : built.mesh.vertices()) {
        const Position3f& p = vertex.position;
        if (p.x >= 100 && p.x <= 101 && p.y >= 100 && p.y <= 101 && p.z > 3 && p.z < 5) {
            ++in_gap;
        }
    }
    CHECK(in_gap == 0);
}

TEST_CASE("WorldMeshMap: blocks at the map corners on levels 0 and 7 span heights 0..8") {
    struct Corner {
        int x, y, z;
        u16 tile;
    };
    constexpr std::array<Corner, 6> kCorners{{
        {0, 0, 0, 21},
        {255, 0, 7, 22},
        {0, 255, 7, 23},
        {255, 255, 0, 24},
        {255, 255, 7, 25},
        {0, 0, 7, 26},
    }};
    assets::GmpWriter writer;
    for (const Corner& c : kCorners) {
        writer.set_block(c.x, c.y, c.z, full_block(c.tile));
    }
    const MapMesh built = build_map(writer, kStyleTiles);
    check_map_mesh(built, kStyleTiles);
    CHECK(built.stats.unsloped_blocks == 6);
    CHECK(built.stats.emitted_faces == 30);
    CHECK(built.stats.emitted_by_face == std::array<u32, 5>{6, 6, 6, 6, 6});

    Position3f low{1000, 1000, 1000};
    Position3f high{-1, -1, -1};
    for (const auto& vertex : built.mesh.vertices()) {
        const Position3f& p = vertex.position;
        low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
        high = {std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z)};
    }
    CHECK(low == Position3f{0, 0, 0});
    CHECK(high == Position3f{256, 256, 8});

    for (const Corner& c : kCorners) {
        CAPTURE(c.x);
        CAPTURE(c.y);
        CAPTURE(c.z);
        const Position3f origin = origin_of(c.x, c.y, c.z);
        check_single_face(built, BlockFace::lid, origin, c.tile, "ABCD");
        for (const auto face : {BlockFace::left, BlockFace::right, BlockFace::top, BlockFace::bottom}) {
            check_single_face(built, face, origin, c.tile, "CDAB");
        }
    }
    const auto corner = [&built](BlockFace face, Position3f origin, std::size_t i) {
        const auto found = find_faces(built, face, origin);
        REQUIRE(found.size() == 1);
        return built.mesh.vertices()[4 * found.front() + i].position;
    };
    CHECK(corner(BlockFace::lid, {255, 255, 7}, 2) == Position3f{256, 256, 8});
    CHECK(corner(BlockFace::lid, {0, 0, 0}, 0) == Position3f{0, 0, 1});
    CHECK(corner(BlockFace::left, {0, 0, 0}, 1) == Position3f{0, 0, 0});
    CHECK(corner(BlockFace::bottom, {0, 255, 7}, 0) == Position3f{1, 256, 7});
    CHECK(corner(BlockFace::right, {255, 0, 7}, 1) == Position3f{256, 1, 7});
}

TEST_CASE("WorldMeshMap: wall and bullet_wall bits are not visibility gates") {
    struct Bits {
        bool wall, bullet_wall;
    };
    constexpr std::array<Bits, 4> kBits{{{false, false}, {true, false}, {false, true}, {true, true}}};
    assets::GmpWriter with_bits;
    assets::GmpWriter without_bits;
    for (std::size_t i = 0; i < kBits.size(); ++i) {
        const auto tile = static_cast<u16>(40 + i);
        const bool flip = i % 2 == 0;
        const u16 collide = side_word(tile, Rotation::R90, flip, kBits[i].wall, kBits[i].bullet_wall);
        const u16 plain = side_word(tile, Rotation::R90, flip);
        with_bits.set_block(30 + static_cast<int>(i), 30, 2, faces(collide, collide, collide, collide, lid_word(50)));
        without_bits.set_block(30 + static_cast<int>(i), 30, 2, faces(plain, plain, plain, plain, lid_word(50)));
    }
    // Collision bits on blank side faces do not make them drawable.
    const u16 blank_wall = side_word(0, Rotation::R0, false, true, true);
    with_bits.set_block(40, 30, 2, faces(blank_wall, blank_wall, blank_wall, blank_wall, 0));
    without_bits.set_block(40, 30, 2, bare_block(slope_type(0, 1)));

    const MapMesh collide = build_map(with_bits, kStyleTiles);
    const MapMesh plain = build_map(without_bits, kStyleTiles);
    check_map_mesh(collide, kStyleTiles);
    check_map_mesh(plain, kStyleTiles);
    CHECK(collide.stats.unsloped_blocks == 5);
    CHECK(collide.stats.emitted_faces == 20);
    CHECK(collide.stats.emitted_by_face == std::array<u32, 5>{4, 4, 4, 4, 4});
    CHECK(collide.stats.blank_faces == 5);
    CHECK(collide.stats.coverage() == MapMeshCoverage::provisional);
    CHECK(collide.stats == plain.stats);
    CHECK(collide.mesh.vertices() == plain.mesh.vertices());
    CHECK(collide.mesh.indices() == plain.mesh.indices());
    check_single_face(collide, BlockFace::right, {31, 30, 2}, 41, "BCDA");  // wall bit, R90
    check_single_face(collide, BlockFace::top, {32, 30, 2}, 42, "ADCB");    // bullet_wall bit, R90 flip
    check_single_face(collide, BlockFace::bottom, {33, 30, 2}, 43, "BCDA"); // both bits, R90
}

TEST_CASE("WorldMeshMap: flat faces are skipped and counted, which makes the coverage partial") {
    assets::GmpWriter writer;
    writer.set_block(70, 70, 1,
                     faces(side_word(51, Rotation::R0, false, true, false, true), // flat + wall
                           side_word(0, Rotation::R0, false, false, false, true), // flat, blank tile
                           side_word(52, Rotation::R180),
                           side_word(1023, Rotation::R0, false, false, false, true), // flat, reserved tile
                           lid_word(50, Rotation::R0, false, 0, true)));
    writer.set_block(71, 70, 1, full_block(53));
    const MapMesh built = build_map(writer, kStyleTiles);
    check_map_mesh(built, kStyleTiles);

    const MapMeshStats& s = built.stats;
    CHECK(s.unsloped_blocks == 2);
    CHECK(s.flat_faces == 4);
    CHECK(s.flat_by_face == std::array<u32, 5>{1, 1, 1, 0, 1});
    CHECK(s.emitted_faces == 6);
    CHECK(s.emitted_by_face == std::array<u32, 5>{1, 1, 1, 2, 1});
    CHECK(s.blank_faces == 0);
    CHECK(s.reserved_faces == 0);
    CHECK(s.out_of_style_faces == 0);
    CHECK(s.coverage() == MapMeshCoverage::partial);
    check_single_face(built, BlockFace::top, {70, 70, 1}, 52, "ABCD");
    for (const auto face : {BlockFace::lid, BlockFace::left, BlockFace::right, BlockFace::bottom}) {
        CHECK(find_faces(built, face, {70, 70, 1}).empty());
    }

    // A single flat lid next to fully supported content is enough for a partial verdict.
    assets::GmpWriter one_flat;
    one_flat.set_block(5, 5, 0, full_block(53));
    one_flat.set_block(6, 5, 0, faces(0, 0, 0, 0, lid_word(54, Rotation::R0, false, 0, true)));
    const MapMesh single = build_map(one_flat, kStyleTiles);
    check_map_mesh(single, kStyleTiles);
    CHECK(single.stats.emitted_faces == 5);
    CHECK(single.stats.flat_faces == 1);
    CHECK(single.stats.blank_faces == 4);
    CHECK(single.stats.coverage() == MapMeshCoverage::partial);
}

TEST_CASE("WorldMeshMap: sloped blocks are skipped whole and counted by slope code") {
    struct Sloped {
        int x, y, z;
        unsigned slope, ground;
    };
    constexpr std::array<Sloped, 6> kSloped{{
        {80, 80, 0, 1, 0},
        {81, 80, 0, 1, 1},
        {82, 80, 0, 8, 2},
        {83, 80, 1, 41, 3},
        {84, 80, 2, 49, 1},
        {85, 80, 7, 63, 0},
    }};
    assets::GmpWriter writer;
    for (const Sloped& b : kSloped) {
        assets::BlockInfo block = full_block(60);
        block.slope_type = slope_type(b.slope, b.ground);
        if (b.slope == 49) {
            block.lid = 1023; // three-sided slope marker: part of the skipped sloped block only
        }
        writer.set_block(b.x, b.y, b.z, block);
    }
    writer.set_block(86, 80, 0, bare_block(slope_type(1, 0))); // slope code without any face
    assets::BlockInfo pavement = full_block(61);
    pavement.slope_type = slope_type(0, 2); // ground type only: unsloped
    writer.set_block(87, 80, 0, pavement);
    const MapMesh built = build_map(writer, kStyleTiles);
    check_map_mesh(built, kStyleTiles);

    const MapMeshStats& s = built.stats;
    std::array<u32, 64> by_code{};
    by_code[1] = 3;
    by_code[8] = 1;
    by_code[41] = 1;
    by_code[49] = 1;
    by_code[63] = 1;
    CHECK(s.sloped_blocks == 7);
    CHECK(s.sloped_blocks_by_code == by_code);
    CHECK(s.unsloped_blocks == 1);
    CHECK(s.emitted_faces == 5);
    CHECK(s.reserved_faces == 0);
    CHECK(s.blank_faces == 0);
    CHECK(s.flat_faces == 0);
    CHECK(s.coverage() == MapMeshCoverage::partial);
    for (const Sloped& b : kSloped) {
        CAPTURE(b.slope);
        for (const auto& spec : kFaces) {
            CHECK(find_faces(built, spec.face, origin_of(b.x, b.y, b.z)).empty()); // no flattened slope
        }
    }
    check_single_face(built, BlockFace::lid, {87, 80, 0}, 61, "ABCD");
}

TEST_CASE("WorldMeshMap: blank, reserved and out-of-style tiles are counted separately") {
    constexpr u16 kTiles = 40;
    assets::GmpWriter writer;
    writer.set_block(
        90, 90, 3, faces(side_word(992), side_word(1022), side_word(40), side_word(39), lid_word(1023, Rotation::R90)));
    writer.set_block(91, 90, 3, bare_block(slope_type(0, 1))); // road without faces
    writer.set_block(92, 90, 3,
                     faces(side_word(41), side_word(991), side_word(1, Rotation::R0, true),
                           side_word(1023, Rotation::R0, false, true), 0));
    const MapMesh built = build_map(writer, kTiles);
    check_map_mesh(built, kTiles);

    const MapMeshStats& s = built.stats;
    CHECK(s.unsloped_blocks == 3);
    CHECK(s.emitted_faces == 2);
    CHECK(s.emitted_by_face == std::array<u32, 5>{0, 0, 0, 1, 1});
    CHECK(s.blank_faces == 6);
    CHECK(s.reserved_faces == 4);     // 992, 1022, lid 1023, wall 1023
    CHECK(s.out_of_style_faces == 3); // 40, 41, 991
    CHECK(s.flat_faces == 0);
    CHECK(s.coverage() == MapMeshCoverage::partial);
    check_single_face(built, BlockFace::bottom, {90, 90, 3}, 39, "CDAB");
    check_single_face(built, BlockFace::top, {92, 90, 3}, 1, "DCBA");
    // The reserved marker 1023 is not drawn as the atlas' white debug slot.
    const ExpectedSlot white = expected_slot(scene::TileAtlas::kWhiteSlot);
    std::size_t white_vertices = 0;
    for (const auto& vertex : built.mesh.vertices()) {
        if (vertex.u >= white.u0 && vertex.u <= white.u1 && vertex.v >= white.v0 && vertex.v <= white.v1) {
            ++white_vertices;
        }
    }
    CHECK(white_vertices == 0);

    // Blank faces alone are not unsupported content; a single reserved lid is.
    assets::GmpWriter blank_only;
    blank_only.set_block(1, 1, 1, bare_block(slope_type(0, 3)));
    const MapMesh blank = build_map(blank_only, kTiles);
    check_map_mesh(blank, kTiles);
    CHECK(blank.stats.blank_faces == 5);
    CHECK(blank.stats.emitted_faces == 0);
    CHECK(blank.stats.coverage() == MapMeshCoverage::provisional);
    assets::GmpWriter reserved_only;
    reserved_only.set_block(1, 1, 1, faces(0, 0, 0, 0, 1023));
    const MapMesh reserved = build_map(reserved_only, kTiles);
    check_map_mesh(reserved, kTiles);
    CHECK(reserved.stats.reserved_faces == 1);
    CHECK(reserved.stats.blank_faces == 4);
    CHECK(reserved.stats.coverage() == MapMeshCoverage::partial);
}

TEST_CASE("WorldMeshMap: tile_count 0 emits no face and reports every style tile as out of style") {
    assets::GmpWriter writer;
    writer.set_block(3, 3, 3, full_block(5));
    writer.set_block(4, 3, 3, faces(side_word(7), side_word(7), side_word(7), side_word(7), 1023));
    writer.set_block(5, 3, 3, faces(side_word(991), side_word(1), side_word(2), side_word(3), 0));
    const MapMesh built = build_map(writer, 0);
    check_map_mesh(built, 0);

    const MapMeshStats& s = built.stats;
    CHECK(s.unsloped_blocks == 3);
    CHECK(s.emitted_faces == 0);
    CHECK(s.out_of_style_faces == 13);
    CHECK(s.reserved_faces == 1);
    CHECK(s.blank_faces == 1);
    CHECK(built.mesh.vertices().empty());
    CHECK(built.mesh.indices().empty());
    CHECK(s.coverage() == MapMeshCoverage::partial);

    // The same map with a full style emits the 13 style tiles; the reserved lid stays partial.
    const MapMesh full = build_map(writer, kStyleTiles);
    check_map_mesh(full, kStyleTiles);
    CHECK(full.stats.emitted_faces == 13);
    CHECK(full.stats.out_of_style_faces == 0);
    CHECK(full.stats.reserved_faces == 1);
    CHECK(full.stats.coverage() == MapMeshCoverage::partial);
}

namespace {

// Mixed SYNTHETIC content for the determinism test: 40 blocks with every kind of face word, written in
// either order (which changes the unique-block table but not the cells).
void add_mixed_content(assets::GmpWriter& writer, bool reverse) {
    struct Cell {
        int x, y, z;
        assets::BlockInfo block;
    };
    std::vector<Cell> cells;
    for (int i = 0; i < 40; ++i) {
        const auto tile = static_cast<u16>(1 + 23 * i);
        const auto turn = [i](int k) { return static_cast<Rotation>((i + k) % 4); };
        assets::BlockInfo block =
            faces(side_word(static_cast<u16>(tile + 1), turn(1), i % 2 == 0, i % 5 == 0),
                  side_word(static_cast<u16>(i % 7 == 0 ? 0 : tile + 2), turn(2), false, true, true),
                  side_word(static_cast<u16>(i % 9 == 0 ? 1000 : tile + 3), turn(3), true),
                  side_word(static_cast<u16>(tile + 4), turn(0), i % 4 == 1, false, false, i % 13 == 0),
                  lid_word(tile, turn(0), i % 3 == 0, static_cast<unsigned>(i), i % 11 == 5));
        block.slope_type = slope_type(i % 6 == 0 ? static_cast<unsigned>(i) : 0u, static_cast<unsigned>(i % 4));
        cells.push_back({(i * 37) % 256, (i * 91) % 256, i % 8, block});
    }
    if (reverse) {
        std::reverse(cells.begin(), cells.end());
    }
    for (const Cell& c : cells) {
        writer.set_block(c.x, c.y, c.z, c.block);
    }
}

} // namespace

TEST_CASE("WorldMeshMap: repeated builds give identical vertices, indices and statistics") {
    assets::GmpWriter forward;
    assets::GmpWriter backward;
    add_mixed_content(forward, false);
    add_mixed_content(backward, true);
    const auto bytes = forward.build_dmap();
    CHECK(bytes != backward.build_dmap()); // different unique-block tables, same cells

    auto map = assets::parse_gmp(bytes);
    REQUIRE(map);
    auto first = scene::build_unsloped_map_mesh(map.value(), kStyleTiles);
    auto second = scene::build_unsloped_map_mesh(map.value(), kStyleTiles);
    REQUIRE(first);
    REQUIRE(second);
    const MapMesh reordered = build_map(backward, kStyleTiles);
    check_map_mesh(first.value(), kStyleTiles);

    // Blocks i = 6, 12, ..., 36 are sloped (i = 0 has slope code 0). Of the 34 unsloped blocks: flat
    // lids i = 5, 16, 27, 38 and bottoms i = 0, 13, 26, 39; reserved tops i = 0, 9, 27 (18 and 36 are
    // sloped); blank rights i = 0, 7, 14, 21, 28, 35. Everything else is drawable.
    const MapMeshStats& s = first.value().stats;
    CHECK(s.sloped_blocks == 6);
    CHECK(s.unsloped_blocks == 34);
    CHECK(s.flat_faces == 8);
    CHECK(s.flat_by_face == std::array<u32, 5>{4, 0, 0, 0, 4});
    CHECK(s.reserved_faces == 3);
    CHECK(s.blank_faces == 6);
    CHECK(s.out_of_style_faces == 0);
    CHECK(s.emitted_faces == 153);
    CHECK(s.emitted_by_face == std::array<u32, 5>{30, 34, 28, 31, 30});
    CHECK(s.coverage() == MapMeshCoverage::partial);
    const std::array<const MapMesh*, 2> others{&second.value(), &reordered};
    for (const MapMesh* other : others) {
        CHECK(other->stats == s);
        CHECK(other->mesh.vertices() == first.value().mesh.vertices());
        CHECK(other->mesh.indices() == first.value().mesh.indices());
    }
}
