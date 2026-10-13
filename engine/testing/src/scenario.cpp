#include "gta20/testing/scenario.hpp"

#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <regex>
#include <set>
#include <sstream>

#include <nlohmann/json.hpp>

namespace gta20::testing {
namespace {

using nlohmann::json;

constexpr std::array<const char*, static_cast<int>(sim::Button::Count)> kButtonNames = {
    "forward", "backward", "left", "right", "attack", "jump", "enterExit", "previousWeapon", "nextWeapon", "special"};

void reject_unknown(const json& object, std::initializer_list<const char*> allowed, const std::string& where) {
    std::set<std::string> ok(allowed.begin(), allowed.end());
    for (const auto& [key, value] : object.items()) {
        if (!ok.contains(key)) {
            throw std::runtime_error("unknown field '" + where + key + "'");
        }
    }
}

Tolerance parse_tolerance(const json& j, const std::string& where) {
    reject_unknown(j, {"absolute", "relative", "wrapPeriod"}, where);
    Tolerance t;
    t.absolute = j.value("absolute", 0.0);
    t.relative = j.value("relative", 0.0);
    if (j.contains("wrapPeriod")) {
        t.wrap_period = j.at("wrapPeriod").get<double>();
    }
    return t;
}

Provenance parse_provenance(const std::string& s) {
    if (const auto p = provenance_from_string(s))
        return *p;
    throw std::runtime_error("unknown provenance '" + s + "' (synthetic | originalGta2 | engine)");
}

} // namespace

const char* to_string(Provenance p) {
    switch (p) {
    case Provenance::Synthetic: return "synthetic";
    case Provenance::OriginalGta2: return "originalGta2";
    case Provenance::Engine: return "engine";
    }
    return "?";
}

std::optional<Provenance> provenance_from_string(const std::string& s) {
    for (const auto p : {Provenance::Synthetic, Provenance::OriginalGta2, Provenance::Engine}) {
        if (s == to_string(p))
            return p;
    }
    return std::nullopt;
}

double Tolerance::distance(double expected, double actual) const {
    if (std::isnan(expected) || std::isnan(actual)) {
        return std::isnan(expected) && std::isnan(actual) ? 0.0 : std::numeric_limits<double>::infinity();
    }
    if (expected == actual) {
        return 0.0;
    }
    double d = std::fabs(expected - actual);
    if (wrap_period && *wrap_period > 0 && std::isfinite(d)) {
        d = std::fmod(d, *wrap_period);
        d = std::min(d, *wrap_period - d);
    }
    return d;
}

bool Tolerance::accepts(double expected, double actual, double& distance_out) const {
    distance_out = distance(expected, actual);
    return distance_out <= absolute || (relative > 0 && distance_out <= relative * std::fabs(expected));
}

const Tolerance& ToleranceSet::lookup(const std::string& kind, const std::string& property) const {
    if (const auto k = kinds.find(kind); k != kinds.end()) {
        if (const auto p = k->second.find(property); p != k->second.end()) {
            return p->second;
        }
    }
    if (const auto p = properties.find(property); p != properties.end()) {
        return p->second;
    }
    return fallback;
}

Result<Scenario> parse_scenario(const std::string& text) {
    try {
        const json j = json::parse(text, nullptr, true, /*ignore_comments=*/true);
        reject_unknown(j,
                       {"schemaVersion", "id", "description", "provenance", "simulation", "world", "inputs", "capture",
                        "tolerances"},
                       "");
        Scenario s;
        s.schema_version = j.value("schemaVersion", 1);
        s.id = j.at("id").get<std::string>();
        s.description = j.value("description", std::string{});
        s.provenance = parse_provenance(j.at("provenance").get<std::string>());

        const json& sim = j.at("simulation");
        reject_unknown(sim, {"tickRateHz", "ticks", "seed"}, "simulation.");
        s.tick_rate_hz = sim.at("tickRateHz").get<int>();
        s.ticks = sim.at("ticks").get<int>();
        s.seed = sim.value("seed", u64{0});

        if (j.contains("world")) {
            const json& world = j.at("world");
            reject_unknown(world, {"map", "spawn"}, "world.");
            s.world_map = world.value("map", std::string{});
            if (world.contains("spawn")) {
                reject_unknown(world.at("spawn"), {"x", "y"}, "world.spawn.");
                s.spawn =
                    std::make_pair(world.at("spawn").at("x").get<double>(), world.at("spawn").at("y").get<double>());
            }
        }
        if (j.contains("inputs")) {
            for (const auto& span : j.at("inputs")) {
                reject_unknown(span, {"fromTick", "toTick", "buttons"}, "inputs[].");
                InputSpan in{span.at("fromTick").get<int>(), span.at("toTick").get<int>(), 0};
                for (const auto& b : span.at("buttons")) {
                    const auto name = b.get<std::string>();
                    bool found = false;
                    for (std::size_t i = 0; i < kButtonNames.size(); ++i) {
                        if (name == kButtonNames[i]) {
                            in.buttons = static_cast<u16>(in.buttons | (1U << i));
                            found = true;
                        }
                    }
                    if (!found) {
                        throw std::runtime_error("unknown button '" + name + "'");
                    }
                }
                if (in.buttons == 0) {
                    throw std::runtime_error("inputs[].buttons must not be empty");
                }
                s.inputs.push_back(in);
            }
        }
        if (j.contains("capture")) {
            const json& c = j.at("capture");
            reject_unknown(c, {"everyTicks", "kinds"}, "capture.");
            s.capture_every = c.value("everyTicks", 1);
            s.capture_kinds = c.value("kinds", std::vector<std::string>{});
        }
        if (j.contains("tolerances")) {
            const json& t = j.at("tolerances");
            reject_unknown(t, {"default", "properties", "kinds"}, "tolerances.");
            if (t.contains("default"))
                s.tolerances.fallback = parse_tolerance(t.at("default"), "tolerances.default.");
            if (t.contains("properties")) {
                for (const auto& [name, tol] : t.at("properties").items()) {
                    s.tolerances.properties[name] = parse_tolerance(tol, "tolerances.properties." + name + ".");
                }
            }
            if (t.contains("kinds")) {
                for (const auto& [kind, props] : t.at("kinds").items()) {
                    for (const auto& [name, tol] : props.items()) {
                        s.tolerances.kinds[kind][name] =
                            parse_tolerance(tol, "tolerances.kinds." + kind + "." + name + ".");
                    }
                }
            }
        }
        return s;
    } catch (const std::exception& e) {
        return make_error(std::string("invalid scenario: ") + e.what());
    }
}

Result<Scenario> load_scenario(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        return make_error("cannot open " + path);
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    auto parsed = parse_scenario(buffer.str());
    if (!parsed) {
        return make_error(path + ": " + parsed.error().message);
    }
    return parsed;
}

std::vector<std::string> validate(const Scenario& s) {
    std::vector<std::string> errors;
    if (s.schema_version != 1)
        errors.push_back("schemaVersion must be 1");
    static const std::regex id_pattern("^[a-z0-9][a-z0-9-]{0,63}$");
    if (!std::regex_match(s.id, id_pattern)) {
        errors.push_back("id must match ^[a-z0-9][a-z0-9-]{0,63}$");
    } else if (s.provenance == Provenance::Synthetic && s.id.rfind("synthetic-", 0) != 0) {
        errors.push_back("synthetic scenarios must have an id starting with 'synthetic-'");
    }
    if (s.tick_rate_hz < 1 || s.tick_rate_hz > 1000)
        errors.push_back("simulation.tickRateHz must be within 1..1000");
    if (s.ticks < 1 || s.ticks > 1'000'000)
        errors.push_back("simulation.ticks must be within 1..1000000");
    for (std::size_t i = 0; i < s.inputs.size(); ++i) {
        const auto& in = s.inputs[i];
        if (in.from_tick < 0 || in.to_tick < in.from_tick || in.to_tick >= s.ticks) {
            errors.push_back("inputs[" + std::to_string(i) + "] must satisfy 0 <= fromTick <= toTick < ticks");
        }
    }
    if (s.capture_every < 1)
        errors.push_back("capture.everyTicks must be >= 1");
    if (s.spawn && (s.spawn->first < 1 || s.spawn->first >= 255 || s.spawn->second < 1 || s.spawn->second >= 255)) {
        errors.push_back("world.spawn must lie within the playable area 1..255");
    }
    const auto check = [&](const Tolerance& t, const std::string& name) {
        if (t.absolute < 0 || t.relative < 0 || (t.wrap_period && *t.wrap_period <= 0)) {
            errors.push_back("tolerance '" + name + "' must be non-negative with a positive wrapPeriod");
        }
    };
    check(s.tolerances.fallback, "default");
    for (const auto& [n, t] : s.tolerances.properties)
        check(t, n);
    for (const auto& [k, props] : s.tolerances.kinds) {
        for (const auto& [n, t] : props)
            check(t, k + "." + n);
    }
    return errors;
}

InputTimeline::InputTimeline(int ticks, const std::vector<InputSpan>& spans)
    : frames_(static_cast<std::size_t>(ticks > 0 ? ticks : 0), 0) {
    for (const auto& span : spans) {
        const int from = std::max(span.from_tick, 0);
        const int to = std::min(span.to_tick, ticks - 1);
        for (int t = from; t <= to; ++t) {
            frames_[static_cast<std::size_t>(t)] =
                static_cast<u16>(frames_[static_cast<std::size_t>(t)] | span.buttons);
        }
    }
}

sim::InputFrame InputTimeline::at(i64 tick) const {
    if (tick < 0 || tick >= static_cast<i64>(frames_.size())) {
        return {};
    }
    return {frames_[static_cast<std::size_t>(tick)]};
}

} // namespace gta20::testing
