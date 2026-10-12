#include <doctest/doctest.h>

#include "gta20/assets/gmp_writer.hpp"
#include "gta20/sim/world.hpp"

using namespace gta20;
using namespace gta20::sim;

namespace {
InputFrame hold(std::initializer_list<Button> buttons) {
    InputFrame f;
    for (auto b : buttons)
        f.set(b, true);
    return f;
}
} // namespace

TEST_CASE("World: open plane - accelerates, caps speed, drags to a stop") {
    WorldSettings s;
    s.wanderers = 0;
    World w(nullptr, s);
    for (int i = 0; i < 300; ++i)
        w.step(hold({Button::Forward}));
    CHECK(w.player().speed == s.rules.max_speed);
    CHECK(w.player().position.x > s.spawn.x);
    CHECK(w.player().position.y == s.spawn.y); // heading 0 = +x
    for (int i = 0; i < 300; ++i)
        w.step({});
    CHECK(w.player().speed == Fixed{});
    CHECK(w.tick() == 600);
}

TEST_CASE("World: turning changes heading by the per-tick rate") {
    WorldSettings s;
    s.wanderers = 0;
    World w(nullptr, s);
    for (int i = 0; i < 10; ++i)
        w.step(hold({Button::Right}));
    // 270 deg/s at 30 Hz = 9 deg per tick -> 90 deg after 10 ticks (within binary-angle truncation).
    CHECK(std::abs(w.player().heading.to_degrees() - 90.0) < 0.1);
}

TEST_CASE("World: walls of the map block movement but allow sliding (SYNTHETIC map)") {
    assets::GmpWriter writer;
    const assets::BlockInfo wall{0x400, 0x400, 0x400, 0x400, 0, 0, 0};
    for (int y = 0; y < 256; ++y)
        writer.set_block(110, y, 2, wall); // wall column at x = 110
    auto map = assets::parse_gmp(writer.build_dmap());
    REQUIRE(map);
    WorldSettings s;
    s.wanderers = 0;
    s.spawn = {Fixed::from_int(105), Fixed::from_int(100)};
    World w(&map.value(), s);
    for (int i = 0; i < 300; ++i)
        w.step(hold({Button::Forward}));
    CHECK(w.player().position.x < Fixed::from_int(110));
    CHECK(w.player().position.x > Fixed::from_int(109));
    CHECK_FALSE(w.cell_passable(110, 100, 2));
}

TEST_CASE("World: identical inputs and seed give bit-identical state") {
    auto map = assets::parse_gmp(assets::build_synthetic_test_map());
    REQUIRE(map);
    WorldSettings s;
    s.seed = 1234;
    World a(&map.value(), s);
    World b(&map.value(), s);
    for (int i = 0; i < 500; ++i) {
        const auto in = hold({Button::Forward, i % 50 < 20 ? Button::Left : Button::Jump});
        a.step(in);
        b.step(in);
    }
    CHECK(a.player().position == b.player().position);
    CHECK(a.player().heading == b.player().heading);
    REQUIRE(a.wanderers().size() == b.wanderers().size());
    for (std::size_t i = 0; i < a.wanderers().size(); ++i) {
        CHECK(a.wanderers()[i].cell_x == b.wanderers()[i].cell_x);
        CHECK(a.wanderers()[i].cell_y == b.wanderers()[i].cell_y);
    }
}

TEST_CASE("World: different seeds change only the random agents") {
    WorldSettings s1, s2;
    s1.seed = 1;
    s2.seed = 2;
    World a(nullptr, s1), b(nullptr, s2);
    bool wanderers_differ = false;
    for (int i = 0; i < 200; ++i) {
        a.step(hold({Button::Forward}));
        b.step(hold({Button::Forward}));
        wanderers_differ = wanderers_differ || a.wanderers()[0].cell_x != b.wanderers()[0].cell_x ||
                           a.wanderers()[0].cell_y != b.wanderers()[0].cell_y;
    }
    CHECK(wanderers_differ);
    CHECK(a.player().position == b.player().position);
}
