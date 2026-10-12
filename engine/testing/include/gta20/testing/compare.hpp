#pragma once

#include <optional>
#include <string>
#include <vector>

#include "gta20/testing/trace.hpp"

namespace gta20::testing {

enum class DivergenceKind {
    HeaderMismatch,
    MissingTick,
    UnexpectedTick,
    MissingEntity,
    UnexpectedEntity,
    KindMismatch,
    MissingProperty,
    UnexpectedProperty,
    ValueOutOfTolerance,
};

[[nodiscard]] const char* to_string(DivergenceKind k);

struct Divergence {
    DivergenceKind kind = DivergenceKind::ValueOutOfTolerance;
    i64 tick = -1; // -1 = header
    std::string entity_id;
    std::string property;
    std::optional<double> expected, actual, distance;
    std::optional<double> absolute_tolerance, relative_tolerance;
    std::string detail;
};

struct PropertyStats {
    std::string kind;
    std::string property;
    u64 compared = 0;
    u64 out_of_tolerance = 0;
    double max_distance = 0;
    std::optional<i64> max_distance_tick;
    std::optional<i64> first_out_of_tolerance_tick;
};

struct ComparisonReport {
    std::string scenario_id;
    std::string expected_source;
    std::string actual_source;
    u64 compared_ticks = 0;
    u64 compared_values = 0;
    u64 total_divergences = 0;
    std::optional<Divergence> first_divergence; // header first, then lowest tick / entity / property
    std::vector<Divergence> divergences;        // ordered, truncated to the limit
    std::vector<PropertyStats> statistics;

    [[nodiscard]] bool passed() const { return total_divergences == 0; }
};

[[nodiscard]] ComparisonReport compare_traces(const Trace& expected, const Trace& actual,
                                              const ToleranceSet& tolerances, std::size_t max_reported = 200);

[[nodiscard]] std::string report_to_markdown(const ComparisonReport& report);
[[nodiscard]] std::string report_to_json(const ComparisonReport& report);

} // namespace gta20::testing
