#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <set>
#include <sstream>

#include "g2n/testing/compare.hpp"
#include "g2n/testing/runner.hpp"

using namespace g2n;
using namespace g2n::testing;

namespace {

const char* kScenario = R"({
  // SYNTHETIC unit-test scenario
  "id": "synthetic-unit",
  "provenance": "synthetic",
  "simulation": { "tickRateHz": 30, "ticks": 40, "seed": 9 },
  "inputs": [ { "fromTick": 0, "toTick": 19, "buttons": ["forward"] },
              { "fromTick": 10, "toTick": 25, "buttons": ["left", "attack"] } ],
  "capture": { "everyTicks": 8 },
  "tolerances": { "kinds": { "player": { "headingUnits": { "absolute": 0, "wrapPeriod": 65536 } } } }
})";

Trace make_trace(std::initializer_list<std::pair<i64, double>> points) {
    Trace t;
    t.header = {1, "synthetic-unit", Provenance::Synthetic, "test", "1", 30, 2, 7, {}};
    for (const auto& [tick, x] : points) {
        t.snapshots.push_back({tick, {{"p", "body", {{"x", x}, {"y", 0}}}}});
    }
    return t;
}

} // namespace

TEST_CASE("Scenario: parses, validates, rejects unknown fields and buttons") {
    auto s = parse_scenario(kScenario);
    REQUIRE_MESSAGE(s, (s ? "" : s.error().message));
    CHECK(validate(s.value()).empty());
    CHECK(s.value().inputs.size() == 2);
    CHECK(s.value().tolerances.lookup("player", "headingUnits").wrap_period == 65536.0);

    CHECK_FALSE(parse_scenario(
        R"({"id":"synthetic-x","provenance":"synthetic","simulation":{"tickRateHz":30,"ticks":1},"typo":1})"));
    CHECK_FALSE(parse_scenario(
        R"({"id":"synthetic-x","provenance":"synthetic","simulation":{"tickRateHz":30,"ticks":5},"inputs":[{"fromTick":0,"toTick":1,"buttons":["fly"]}]})"));
    CHECK_FALSE(
        parse_scenario(R"({"id":"synthetic-x","provenance":"made-up","simulation":{"tickRateHz":30,"ticks":1}})"));
}

TEST_CASE("Scenario: synthetic data must be labelled; ranges are checked") {
    auto s = parse_scenario(kScenario).value();
    s.id = "wil-drive";
    CHECK_FALSE(validate(s).empty());
    s.id = "synthetic-x";
    s.inputs[0].to_tick = 40;
    CHECK_FALSE(validate(s).empty());
    s.inputs[0].to_tick = 1;
    s.tick_rate_hz = 0;
    CHECK_FALSE(validate(s).empty());
}

TEST_CASE("InputTimeline: overlapping spans are OR-ed, out of range is empty") {
    const InputTimeline t(10, {{2, 5, 1}, {4, 7, 4}});
    CHECK(t.at(1).buttons == 0);
    CHECK(t.at(2).buttons == 1);
    CHECK(t.at(4).buttons == 5);
    CHECK(t.at(7).buttons == 4);
    CHECK(t.at(-1).buttons == 0);
    CHECK(t.at(10).buttons == 0);
}

TEST_CASE("Runner: snapshot schedule, determinism and byte-stable traces") {
    const auto scenario = parse_scenario(kScenario).value();
    auto a1 = make_engine_adapter();
    auto a2 = make_engine_adapter();
    auto t1 = run_scenario(scenario, *a1);
    auto t2 = run_scenario(scenario, *a2);
    REQUIRE(t1);
    REQUIRE(t2);
    std::vector<i64> ticks;
    for (const auto& s : t1.value().snapshots)
        ticks.push_back(s.tick);
    CHECK(ticks == std::vector<i64>{0, 8, 16, 24, 32, 40});
    CHECK(trace_to_string(t1.value()) == trace_to_string(t2.value()));
    CHECK(compare_traces(t1.value(), t2.value(), scenario.tolerances).passed());
}

TEST_CASE("Trace: canonical round trip including non-finite values") {
    Trace t = make_trace({{0, 0.1 + 0.2}, {1, -0.0}});
    t.snapshots[0].entities[0].properties.emplace_back("nan", std::numeric_limits<double>::quiet_NaN());
    t.snapshots[0].entities[0].properties.emplace_back("inf", -std::numeric_limits<double>::infinity());
    const std::string text = trace_to_string(t);
    std::istringstream in(text);
    auto back = read_trace(in);
    REQUIRE(back);
    CHECK(trace_to_string(back.value()) == text);
    CHECK(*back.value().snapshots[0].entities[0].find("x") == 0.1 + 0.2);
    CHECK(std::isnan(*back.value().snapshots[0].entities[0].find("nan")));
    CHECK(text.rfind("{\"header\":", 0) == 0);
}

TEST_CASE("Trace: rejects non-increasing ticks") {
    std::istringstream in(trace_to_string(make_trace({{0, 1}, {1, 2}})) + "{\"tick\":1,\"entities\":[]}\n");
    CHECK_FALSE(read_trace(in));
}

TEST_CASE("Compare: tolerance, first divergence, statistics") {
    const auto expected = make_trace({{0, 0}, {1, 1}, {2, 2}});
    const auto actual = make_trace({{0, 0.05}, {1, 1.5}, {2, 9}});
    ToleranceSet tol;
    tol.fallback.absolute = 0.1;
    const auto r = compare_traces(expected, actual, tol);
    CHECK_FALSE(r.passed());
    CHECK(r.total_divergences == 2);
    REQUIRE(r.first_divergence);
    CHECK(r.first_divergence->tick == 1);
    CHECK(r.first_divergence->property == "x");
    CHECK(*r.first_divergence->distance == 0.5);
    const auto& x = r.statistics.front();
    CHECK(x.property == "x");
    CHECK(x.first_out_of_tolerance_tick == 1);
    CHECK(x.max_distance == 7);
    CHECK(x.max_distance_tick == 2);
    CHECK(report_to_markdown(r).find("| 1 | p | x | ValueOutOfTolerance | 1 | 1.5 | 0.5 |") != std::string::npos);
    CHECK(report_to_json(r).find("\"passed\": false") != std::string::npos);
}

TEST_CASE("Compare: wrap-around, relative and NaN rules") {
    Tolerance wrap{1, 0, 360.0};
    double d = 0;
    CHECK(wrap.accepts(359, 359.5, d));
    CHECK(d == 0.5);
    CHECK_FALSE(wrap.accepts(359, 0.5, d)); // 1.5 the short way round, above 1
    CHECK(d == 1.5);
    CHECK(Tolerance{0, 0.01, std::nullopt}.accepts(1000, 1009, d));
    CHECK(Tolerance{}.accepts(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), d));
    CHECK_FALSE(Tolerance{1e9, 0, std::nullopt}.accepts(1, std::numeric_limits<double>::infinity(), d));
}

TEST_CASE("Compare: structural differences are all reported, header first") {
    auto expected = make_trace({{0, 0}, {1, 1}});
    auto actual = make_trace({{0, 0}, {2, 1}});
    actual.header.seed = 8;
    actual.snapshots[0].entities[0].properties.emplace_back("z", 1);
    actual.snapshots[0].entities.push_back({"q", "body", {}});
    const auto r = compare_traces(expected, actual, {});
    std::set<DivergenceKind> kinds;
    for (const auto& dv : r.divergences)
        kinds.insert(dv.kind);
    CHECK(kinds.contains(DivergenceKind::HeaderMismatch));
    CHECK(kinds.contains(DivergenceKind::UnexpectedProperty));
    CHECK(kinds.contains(DivergenceKind::UnexpectedEntity));
    CHECK(kinds.contains(DivergenceKind::MissingTick));
    CHECK(kinds.contains(DivergenceKind::UnexpectedTick));
    CHECK(r.first_divergence->kind == DivergenceKind::HeaderMismatch);
}

TEST_CASE("Runner: wall-slide scenario really collides and slides (SYNTHETIC map)") {
    auto scenario =
        load_scenario(std::string(G2N_SOURCE_DIR) + "/tests/conformance/scenarios/synthetic-wall-slide.json");
    REQUIRE_MESSAGE(scenario, (scenario ? "" : scenario.error().message));
    auto adapter = make_engine_adapter();
    auto trace = run_scenario(scenario.value(), *adapter);
    REQUIRE(trace);
    bool blocked_while_sliding = false;
    bool passed_corner = false;
    double previous_x = 0, previous_y = 0;
    for (const auto& s : trace.value().snapshots) {
        const double x = *s.entities.at(0).find("x");
        const double y = *s.entities.at(0).find("y");
        if (s.tick > 0 && x == previous_x && y > previous_y && x > 101.0 && x < 102.0)
            blocked_while_sliding = true;
        if (y > 109 && x > 102.0)
            passed_corner = true;
        previous_x = x;
        previous_y = y;
    }
    CHECK(blocked_while_sliding); // x pinned at the wall (102 - body radius) while y advances
    CHECK(passed_corner);         // after the corner (y > 109) x moves on
}
