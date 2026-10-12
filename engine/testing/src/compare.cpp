#include "g2n/testing/compare.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

#include <nlohmann/json.hpp>

namespace g2n::testing {
namespace {

void compare_headers(const TraceHeader& e, const TraceHeader& a, std::vector<Divergence>& out) {
    const auto check = [&](const char* field, const std::string& ev, const std::string& av) {
        if (ev != av) {
            Divergence d;
            d.kind = DivergenceKind::HeaderMismatch;
            d.property = field;
            d.detail = "expected '" + ev + "', actual '" + av + "'";
            out.push_back(std::move(d));
        }
    };
    check("scenarioId", e.scenario_id, a.scenario_id);
    check("tickRateHz", std::to_string(e.tick_rate_hz), std::to_string(a.tick_rate_hz));
    check("ticks", std::to_string(e.ticks), std::to_string(a.ticks));
    check("seed", std::to_string(e.seed), std::to_string(a.seed));
}

} // namespace

const char* to_string(DivergenceKind k) {
    switch (k) {
    case DivergenceKind::HeaderMismatch: return "HeaderMismatch";
    case DivergenceKind::MissingTick: return "MissingTick";
    case DivergenceKind::UnexpectedTick: return "UnexpectedTick";
    case DivergenceKind::MissingEntity: return "MissingEntity";
    case DivergenceKind::UnexpectedEntity: return "UnexpectedEntity";
    case DivergenceKind::KindMismatch: return "KindMismatch";
    case DivergenceKind::MissingProperty: return "MissingProperty";
    case DivergenceKind::UnexpectedProperty: return "UnexpectedProperty";
    case DivergenceKind::ValueOutOfTolerance: return "ValueOutOfTolerance";
    }
    return "?";
}

ComparisonReport compare_traces(const Trace& expected, const Trace& actual, const ToleranceSet& tolerances,
                                std::size_t max_reported) {
    ComparisonReport report;
    report.scenario_id = expected.header.scenario_id;
    std::vector<Divergence> all;
    std::map<std::pair<std::string, std::string>, PropertyStats> stats;

    compare_headers(expected.header, actual.header, all);

    std::map<i64, const TickSnapshot*> actual_by_tick;
    for (const auto& s : actual.snapshots)
        actual_by_tick[s.tick] = &s;
    std::set<i64> expected_ticks;

    for (const auto& exp : expected.snapshots) {
        expected_ticks.insert(exp.tick);
        const auto found = actual_by_tick.find(exp.tick);
        if (found == actual_by_tick.end()) {
            Divergence d;
            d.kind = DivergenceKind::MissingTick;
            d.tick = exp.tick;
            all.push_back(std::move(d));
            continue;
        }
        ++report.compared_ticks;
        const TickSnapshot& act = *found->second;
        std::map<std::string, const EntityState*> actual_entities;
        for (const auto& e : act.entities)
            actual_entities[e.id] = &e;
        std::set<std::string> expected_ids;

        for (const auto& e : exp.entities) {
            expected_ids.insert(e.id);
            const auto a_it = actual_entities.find(e.id);
            Divergence base;
            base.tick = exp.tick;
            base.entity_id = e.id;
            if (a_it == actual_entities.end()) {
                base.kind = DivergenceKind::MissingEntity;
                all.push_back(base);
                continue;
            }
            const EntityState& a = *a_it->second;
            if (a.kind != e.kind) {
                base.kind = DivergenceKind::KindMismatch;
                base.detail = "expected kind '" + e.kind + "', actual '" + a.kind + "'";
                all.push_back(base);
                continue;
            }
            for (const auto& [name, ev] : e.properties) {
                const double* av = a.find(name);
                Divergence d = base;
                d.property = name;
                d.expected = ev;
                if (av == nullptr) {
                    d.kind = DivergenceKind::MissingProperty;
                    all.push_back(std::move(d));
                    continue;
                }
                const Tolerance& tol = tolerances.lookup(e.kind, name);
                double distance = 0;
                const bool ok = tol.accepts(ev, *av, distance);
                ++report.compared_values;
                auto& st = stats[{e.kind, name}];
                st.kind = e.kind;
                st.property = name;
                ++st.compared;
                if (distance > st.max_distance || (!st.max_distance_tick && distance > 0)) {
                    st.max_distance = distance;
                    st.max_distance_tick = exp.tick;
                }
                if (!ok) {
                    ++st.out_of_tolerance;
                    if (!st.first_out_of_tolerance_tick)
                        st.first_out_of_tolerance_tick = exp.tick;
                    d.kind = DivergenceKind::ValueOutOfTolerance;
                    d.actual = *av;
                    d.distance = distance;
                    d.absolute_tolerance = tol.absolute;
                    d.relative_tolerance = tol.relative;
                    all.push_back(std::move(d));
                }
            }
            for (const auto& [name, av] : a.properties) {
                if (e.find(name) == nullptr) {
                    Divergence d = base;
                    d.kind = DivergenceKind::UnexpectedProperty;
                    d.property = name;
                    d.actual = av;
                    all.push_back(std::move(d));
                }
            }
        }
        for (const auto& a : act.entities) {
            if (!expected_ids.contains(a.id)) {
                Divergence d;
                d.kind = DivergenceKind::UnexpectedEntity;
                d.tick = exp.tick;
                d.entity_id = a.id;
                all.push_back(std::move(d));
            }
        }
    }
    for (const auto& s : actual.snapshots) {
        if (!expected_ticks.contains(s.tick)) {
            Divergence d;
            d.kind = DivergenceKind::UnexpectedTick;
            d.tick = s.tick;
            all.push_back(std::move(d));
        }
    }

    std::stable_sort(all.begin(), all.end(), [](const Divergence& a, const Divergence& b) {
        return std::tie(a.tick, a.entity_id, a.property) < std::tie(b.tick, b.entity_id, b.property);
    });
    report.total_divergences = all.size();
    if (!all.empty())
        report.first_divergence = all.front();
    if (all.size() > max_reported)
        all.resize(max_reported);
    report.divergences = std::move(all);
    for (auto& [key, st] : stats)
        report.statistics.push_back(std::move(st));
    return report;
}

namespace {

std::string fmt(const std::optional<double>& v) {
    return v ? format_number(*v) : "-";
}
std::string fmt_tick(const std::optional<i64>& v) {
    return v ? std::to_string(*v) : "-";
}

std::string row(const Divergence& d) {
    std::ostringstream o;
    o << "| " << (d.tick < 0 ? std::string("header") : std::to_string(d.tick)) << " | "
      << (d.entity_id.empty() ? "-" : d.entity_id) << " | " << (d.property.empty() ? "-" : d.property) << " | "
      << to_string(d.kind) << " | " << fmt(d.expected) << " | " << fmt(d.actual) << " | " << fmt(d.distance) << " | "
      << (d.absolute_tolerance ? "abs " + fmt(d.absolute_tolerance) + ", rel " + fmt(d.relative_tolerance)
                               : (d.detail.empty() ? "-" : d.detail))
      << " |";
    return o.str();
}

} // namespace

std::string report_to_markdown(const ComparisonReport& r) {
    std::ostringstream o;
    o << "### Conformance: `" << r.scenario_id << "` - " << (r.passed() ? "PASS" : "FAIL") << "\n\n";
    o << "- expected: `" << r.expected_source << "`\n- actual: `" << r.actual_source << "`\n";
    o << "- compared ticks: " << r.compared_ticks << ", values: " << r.compared_values
      << ", divergences: " << r.total_divergences << "\n\n";
    const char* table_header = "| tick | entity | property | kind | expected | actual | distance | tolerance "
                               "|\n|---|---|---|---|---|---|---|---|\n";
    if (r.first_divergence) {
        o << "**First divergence**\n\n" << table_header << row(*r.first_divergence) << "\n\n";
    }
    if (!r.statistics.empty()) {
        o << "**Per-property statistics**\n\n| kind | property | compared | out of tol. | max distance (tick) | first "
             "out-of-tol. tick |\n|---|---|---|---|---|---|\n";
        for (const auto& s : r.statistics) {
            o << "| " << s.kind << " | " << s.property << " | " << s.compared << " | " << s.out_of_tolerance << " | "
              << format_number(s.max_distance) << " (" << fmt_tick(s.max_distance_tick) << ") | "
              << fmt_tick(s.first_out_of_tolerance_tick) << " |\n";
        }
        o << "\n";
    }
    if (r.divergences.size() > 1) {
        o << "**Divergences** (showing " << r.divergences.size() << " of " << r.total_divergences << ")\n\n"
          << table_header;
        for (const auto& d : r.divergences)
            o << row(d) << "\n";
    }
    return o.str();
}

std::string report_to_json(const ComparisonReport& r) {
    using nlohmann::json;
    const auto opt = [](const std::optional<double>& v) { return v ? json(*v) : json(nullptr); };
    const auto div = [&](const Divergence& d) {
        return json{{"kind", to_string(d.kind)},
                    {"tick", d.tick},
                    {"entityId", d.entity_id},
                    {"property", d.property},
                    {"expected", opt(d.expected)},
                    {"actual", opt(d.actual)},
                    {"distance", opt(d.distance)},
                    {"absoluteTolerance", opt(d.absolute_tolerance)},
                    {"relativeTolerance", opt(d.relative_tolerance)},
                    {"detail", d.detail}};
    };
    json j{{"scenarioId", r.scenario_id},
           {"expectedSource", r.expected_source},
           {"actualSource", r.actual_source},
           {"passed", r.passed()},
           {"comparedTicks", r.compared_ticks},
           {"comparedValues", r.compared_values},
           {"totalDivergences", r.total_divergences}};
    j["firstDivergence"] = r.first_divergence ? div(*r.first_divergence) : json(nullptr);
    j["divergences"] = json::array();
    for (const auto& d : r.divergences)
        j["divergences"].push_back(div(d));
    j["statistics"] = json::array();
    for (const auto& s : r.statistics) {
        j["statistics"].push_back(
            {{"kind", s.kind},
             {"property", s.property},
             {"compared", s.compared},
             {"outOfTolerance", s.out_of_tolerance},
             {"maxDistance", s.max_distance},
             {"maxDistanceTick", s.max_distance_tick ? json(*s.max_distance_tick) : json(nullptr)},
             {"firstOutOfToleranceTick",
              s.first_out_of_tolerance_tick ? json(*s.first_out_of_tolerance_tick) : json(nullptr)}});
    }
    return j.dump(2);
}

} // namespace g2n::testing
