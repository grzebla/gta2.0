#pragma once

#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

#include "g2n/core/result.hpp"
#include "g2n/testing/scenario.hpp"

namespace g2n::testing {

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

/// Canonical JSON Lines: line 1 {"header":{...}}, then {"tick":N,"entities":[...]}.
/// Sorted keys, shortest round-trip doubles, no timestamps: identical runs give identical bytes.
void write_trace(const Trace& trace, std::ostream& out);
[[nodiscard]] std::string trace_to_string(const Trace& trace);
[[nodiscard]] Result<Trace> read_trace(std::istream& in);
[[nodiscard]] Result<Trace> read_trace_file(const std::string& path);
[[nodiscard]] Result<bool> write_trace_file(const Trace& trace, const std::string& path);

/// Shortest decimal that parses back to the same double; non-finite as "nan"/"inf"/"-inf" strings.
[[nodiscard]] std::string format_number(double value);

} // namespace g2n::testing
