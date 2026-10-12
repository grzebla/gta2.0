#include <doctest/doctest.h>

#include <array>
#include <set>

#include "g2n/core/angle.hpp"
#include "g2n/core/byte_reader.hpp"
#include "g2n/core/camera2d.hpp"
#include "g2n/core/clock.hpp"
#include "g2n/core/diagnostics.hpp"
#include "g2n/core/handle_pool.hpp"
#include "g2n/core/rng.hpp"

using namespace g2n;

TEST_CASE("Fixed: exact Q16.16 arithmetic") {
    const Fixed a = Fixed::from_ratio(3, 2);
    const Fixed b = Fixed::from_int(-2);
    CHECK((a + b).raw() == -Fixed::kOneRaw / 2);
    CHECK((a * b).raw() == -3 * Fixed::kOneRaw);
    CHECK((b / a).raw() == Fixed::from_ratio(-4, 3).raw());
    CHECK(Fixed::from_ratio(-1, 2).floor_int() == -1);
    CHECK(Fixed::from_ratio(7, 4).to_double() == 1.75);
    CHECK(clamp(Fixed::from_int(9), Fixed::from_int(0), Fixed::from_int(8)) == Fixed::from_int(8));
}

TEST_CASE("Fixed: overflow wraps instead of being undefined") {
    const Fixed big = Fixed::from_raw(0x7FFFFFFF);
    CHECK((big + Fixed::from_raw(1)).raw() == static_cast<i32>(0x80000000U));
}

TEST_CASE("Angle: degrees round trip and wrap") {
    CHECK(Angle::from_degrees(90).units() == 16384);
    CHECK(Angle::from_degrees(-90).units() == 49152);
    CHECK(Angle::from_degrees(450).units() == 16384);
    CHECK((Angle::from_units(65000) + Angle::from_units(1000)).units() == 464);
}

TEST_CASE("Angle: table sine is exact at quadrants and symmetric") {
    CHECK(sin(Angle::from_units(0)).raw() == 0);
    CHECK(sin(Angle::from_degrees(90)).raw() == Fixed::kOneRaw);
    CHECK(sin(Angle::from_degrees(270)).raw() == -Fixed::kOneRaw);
    CHECK(cos(Angle::from_units(0)).raw() == Fixed::kOneRaw);
    CHECK(cos(Angle::from_degrees(180)).raw() == -Fixed::kOneRaw);
    for (u32 u = 0; u < 32768; u += 64) {
        const auto a = Angle::from_units(static_cast<u16>(u));
        CHECK(sin(a + Angle::from_units(32768)).raw() == -sin(a).raw());
    }
    // sin(30 deg) = 0.5 within one table step.
    CHECK(std::abs(sin(Angle::from_degrees(30)).to_double() - 0.5) < 0.004);
}

TEST_CASE("SplitMix64 matches the published reference sequence for seed 0") {
    SplitMix64 rng(0);
    CHECK(rng.next() == 0xE220A8397B1DCDAFULL);
    CHECK(rng.next() == 0x6E789E6AA1B965F4ULL);
    CHECK(rng.next() == 0x06C45D188009454FULL);
}

TEST_CASE("SplitMix64: bounded values stay in range and depend on the seed") {
    SplitMix64 a(42), b(42), c(43);
    bool differs = false;
    for (int i = 0; i < 1000; ++i) {
        const u32 x = a.next_below(10);
        CHECK(x < 10);
        CHECK(x == b.next_below(10));
        differs = differs || x != c.next_below(10);
    }
    CHECK(differs);
}

TEST_CASE("FixedStepClock: no drift and exact deltas") {
    FixedStepClock clock(30);
    u64 sum = 0;
    for (int i = 0; i < 108000; ++i) { // one hour at 30 Hz
        sum += clock.next_delta_ns();
        clock.advance();
    }
    CHECK(clock.elapsed_ns() == 3600ULL * 1'000'000'000ULL);
    CHECK(sum == clock.elapsed_ns());
}

TEST_CASE("FrameAccumulator: whole steps, interpolation factor and spiral guard") {
    FrameAccumulator acc(30, 5);
    CHECK(acc.add(16'666'667) == 0);
    CHECK(acc.alpha() == doctest::Approx(0.5).epsilon(0.001));
    CHECK(acc.add(16'666'667) == 1);
    CHECK(acc.add(1'000'000'000) == 5); // 30 steps due, only 5 run
    CHECK(acc.dropped_steps() == 25);
    CHECK(acc.alpha() < 1.0f);
}

TEST_CASE("HandlePool: generations detect stale ids and slots are reused") {
    HandlePool pool;
    const EntityId a = pool.allocate();
    const EntityId b = pool.allocate();
    CHECK(pool.alive(a));
    CHECK(pool.release(a));
    CHECK_FALSE(pool.alive(a));
    CHECK_FALSE(pool.release(a));
    const EntityId c = pool.allocate();
    CHECK(c.index == a.index);
    CHECK(c.generation != a.generation);
    CHECK(pool.alive(b));
    CHECK(pool.live_count() == 2);
    CHECK_FALSE(pool.alive(EntityId{}));
}

TEST_CASE("ByteReader: little endian, sticky failure") {
    const std::array<std::byte, 7> data = {std::byte{0x34}, std::byte{0x12}, std::byte{0x78}, std::byte{0x56},
                                           std::byte{0x34}, std::byte{0x12}, std::byte{'A'}};
    ByteReader r(data);
    CHECK(r.u16le() == 0x1234);
    CHECK(r.u32le() == 0x12345678);
    CHECK(r.fixed_string(1) == "A");
    CHECK_FALSE(r.failed());
    CHECK(r.u8le() == 0);
    CHECK(r.failed());
    CHECK(r.u16le() == 0);
    CHECK(r.remaining() == 0);
}

TEST_CASE("Camera2D: world/screen mapping is consistent with the projection") {
    Camera2D cam{{100, 50}, 32};
    const Vec2f viewport{1280, 720};
    const Vec2f s = cam.world_to_screen({101, 51}, viewport);
    CHECK(s.x == doctest::Approx(672));
    CHECK(s.y == doctest::Approx(392));
    const Vec2f w = cam.screen_to_world(s, viewport);
    CHECK(w.x == doctest::Approx(101));
    CHECK(w.y == doctest::Approx(51));
    const auto m = cam.view_projection(viewport);
    // Camera centre maps to clip origin; +1 world y (down) maps to negative clip y.
    CHECK(m[0] * 100 + m[12] == doctest::Approx(0));
    CHECK(m[5] * 51 + m[13] < 0);
    const auto vis = cam.visible_world(viewport);
    CHECK(vis.max.x - vis.min.x == doctest::Approx(1280.0 / 32));
}

TEST_CASE("summarize: percentiles") {
    std::vector<u64> v;
    for (u64 i = 1; i <= 100; ++i)
        v.push_back(i);
    const auto s = summarize(v);
    CHECK(s.count == 100);
    CHECK(s.min_ns == 1);
    CHECK(s.max_ns == 100);
    CHECK(s.p50_ns == 51);
    CHECK(s.p99_ns == 99);
    CHECK(s.mean_ns == doctest::Approx(50.5));
}
