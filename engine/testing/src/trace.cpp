#include "g2n/testing/trace.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>

#include <nlohmann/json.hpp>

namespace g2n::testing {
namespace {

using nlohmann::json;

std::string quote(const std::string& s) {
    return json(s).dump();
}

double parse_number(const json& j) {
    if (j.is_string()) {
        const auto s = j.get<std::string>();
        if (s == "nan")
            return std::numeric_limits<double>::quiet_NaN();
        if (s == "inf")
            return std::numeric_limits<double>::infinity();
        if (s == "-inf")
            return -std::numeric_limits<double>::infinity();
        throw std::runtime_error("invalid number '" + s + "'");
    }
    return j.get<double>();
}

} // namespace

const double* EntityState::find(const std::string& name) const {
    for (const auto& [n, v] : properties) {
        if (n == name)
            return &v;
    }
    return nullptr;
}

std::string format_number(double value) {
    if (std::isnan(value))
        return "\"nan\"";
    if (std::isinf(value))
        return value > 0 ? "\"inf\"" : "\"-inf\"";
    if (value == 0)
        return "0"; // also normalises -0
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return {buffer, result.ptr};
}

void write_trace(const Trace& trace, std::ostream& out) {
    const auto& h = trace.header;
    out << R"({"header":{"adapter":)" << quote(h.adapter) << R"(,"adapterVersion":)" << quote(h.adapter_version)
        << R"(,"provenance":)" << quote(to_string(h.provenance)) << R"(,"scenarioId":)" << quote(h.scenario_id)
        << R"(,"schemaVersion":)" << h.schema_version << R"(,"seed":)" << h.seed;
    if (!h.source.empty()) {
        out << R"(,"source":)" << quote(h.source);
    }
    out << R"(,"tickRateHz":)" << h.tick_rate_hz << R"(,"ticks":)" << h.ticks << "}}\n";

    for (const auto& snapshot : trace.snapshots) {
        auto entities = snapshot.entities;
        std::sort(entities.begin(), entities.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
        out << R"({"tick":)" << snapshot.tick << R"(,"entities":[)";
        for (std::size_t i = 0; i < entities.size(); ++i) {
            auto props = entities[i].properties;
            std::sort(props.begin(), props.end());
            out << (i ? "," : "") << R"({"id":)" << quote(entities[i].id) << R"(,"kind":)" << quote(entities[i].kind)
                << R"(,"properties":{)";
            for (std::size_t p = 0; p < props.size(); ++p) {
                out << (p ? "," : "") << quote(props[p].first) << ':' << format_number(props[p].second);
            }
            out << "}}";
        }
        out << "]}\n";
    }
}

std::string trace_to_string(const Trace& trace) {
    std::ostringstream out;
    write_trace(trace, out);
    return out.str();
}

Result<Trace> read_trace(std::istream& in) {
    Trace trace;
    std::string line;
    int line_number = 0;
    try {
        if (!std::getline(in, line)) {
            return make_error("trace is empty");
        }
        ++line_number;
        const json header = json::parse(line).at("header");
        auto& h = trace.header;
        h.schema_version = header.at("schemaVersion").get<int>();
        if (h.schema_version != 1) {
            return make_error("unsupported trace schemaVersion " + std::to_string(h.schema_version));
        }
        h.scenario_id = header.at("scenarioId").get<std::string>();
        const auto provenance = header.at("provenance").get<std::string>();
        h.provenance = provenance == "originalGta2" ? Provenance::OriginalGta2
                       : provenance == "engine"     ? Provenance::Engine
                                                    : Provenance::Synthetic;
        h.adapter = header.at("adapter").get<std::string>();
        h.adapter_version = header.at("adapterVersion").get<std::string>();
        h.tick_rate_hz = header.at("tickRateHz").get<int>();
        h.ticks = header.at("ticks").get<int>();
        h.seed = header.value("seed", u64{0});
        h.source = header.value("source", std::string{});

        while (std::getline(in, line)) {
            ++line_number;
            if (line.empty())
                continue;
            const json j = json::parse(line);
            TickSnapshot snapshot;
            snapshot.tick = j.at("tick").get<i64>();
            if (!trace.snapshots.empty() && snapshot.tick <= trace.snapshots.back().tick) {
                return make_error("line " + std::to_string(line_number) + ": ticks must be strictly increasing");
            }
            for (const auto& e : j.at("entities")) {
                EntityState state{e.at("id").get<std::string>(), e.at("kind").get<std::string>(), {}};
                for (const auto& [name, value] : e.at("properties").items()) {
                    state.properties.emplace_back(name, parse_number(value));
                }
                std::sort(state.properties.begin(), state.properties.end());
                snapshot.entities.push_back(std::move(state));
            }
            trace.snapshots.push_back(std::move(snapshot));
        }
    } catch (const std::exception& e) {
        return make_error("line " + std::to_string(line_number) + ": " + e.what());
    }
    return trace;
}

Result<Trace> read_trace_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        return make_error("cannot open " + path);
    }
    return read_trace(in);
}

Result<bool> write_trace_file(const Trace& trace, const std::string& path) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return make_error("cannot write " + path);
    }
    write_trace(trace, out);
    return true;
}

} // namespace g2n::testing
