#include <doctest/doctest.h>

#include "gta20/assets/gmp_writer.hpp"
#include "gta20/scene/draw_list.hpp"

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
