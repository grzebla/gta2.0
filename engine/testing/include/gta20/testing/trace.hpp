#pragma once

#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

#include "gta20/core/result.hpp"
#include "gta20/testing/scenario.hpp"

namespace gta20::testing {

/// One observable object; numeric properties sorted by name.
struct EntityState {
    std::string id;
    std::string kind;
    std::vector<std::pair<std::string, double>> properties;

    [[nodiscard]] const double* find(const std::string& name) const;
};

/// State after `tick` completed ticks (0 = initial state); entities sorted by id.
struct TickSnapshot {
    i64 tick = 0;
    std::vector<EntityState> entities;
};

struct TraceHeader {
    int schema_version = 1;
    std::string scenario_id;
    Provenance provenance = Provenance::Synthetic;
    std::string adapter;
    std::string adapter_version;
    int tick_rate_hz = 0;
    int ticks = 0;
    u64 seed = 0;
    std::string source; // free-form origin note, never compared
};

struct Trace {
    TraceHeader header;
    std::vector<TickSnapshot> snapshots;
};

/// Every integrity problem, empty when valid: supported header (schema 1, scenario id, known
/// provenance, tick rate > 0, ticks >= 0), at least one snapshot, snapshot ticks strictly increasing
/// within 0..header.ticks (sparse schedules are fine), entity ids non-empty and unique per tick,
/// property names unique per entity.
[[nodiscard]] std::vector<std::string> validate(const Trace& trace);

/// Canonical JSON Lines: line 1 {"header":{...}}, then {"tick":N,"entities":[...]}.
/// Sorted keys, shortest round-trip doubles, no timestamps: identical runs give identical bytes.
void write_trace(const Trace& trace, std::ostream& out);
[[nodiscard]] std::string trace_to_string(const Trace& trace);
/// Rejects duplicate JSON keys, unknown provenance and any validate() problem.
[[nodiscard]] Result<Trace> read_trace(std::istream& in);
[[nodiscard]] Result<Trace> read_trace_file(const std::string& path);
[[nodiscard]] Result<bool> write_trace_file(const Trace& trace, const std::string& path);

/// Shortest decimal that parses back to the same double; non-finite as "nan"/"inf"/"-inf" strings.
[[nodiscard]] std::string format_number(double value);

} // namespace gta20::testing
