#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "gta20/core/result.hpp"
#include "gta20/sim/input.hpp"

namespace gta20::testing {

/// Where expected data comes from. Synthetic data must never be presented as GTA 2 behaviour.
enum class Provenance { Synthetic, OriginalGta2, Engine };

[[nodiscard]] const char* to_string(Provenance p);

struct Tolerance {
    double absolute = 0;
    double relative = 0;
    std::optional<double> wrap_period; // e.g. 360 (degrees) or 65536 (binary angle units)

    /// Distance used for comparison (shortest way around the circle when wrapping).
    [[nodiscard]] double distance(double expected, double actual) const;
    [[nodiscard]] bool accepts(double expected, double actual, double& distance_out) const;
};

struct ToleranceSet {
    Tolerance fallback;
    std::map<std::string, Tolerance> properties;
    std::map<std::string, std::map<std::string, Tolerance>> kinds;

    [[nodiscard]] const Tolerance& lookup(const std::string& kind, const std::string& property) const;
};

struct InputSpan {
    int from_tick = 0;
    int to_tick = 0; // inclusive
    u16 buttons = 0;
};

struct Scenario {
    int schema_version = 1;
    std::string id;
    std::string description;
    Provenance provenance = Provenance::Synthetic;
    int tick_rate_hz = 30;
    int ticks = 1;
    u64 seed = 0;
    std::string world_map;                          // relative to the game dir; empty = synthetic test map
    std::optional<std::pair<double, double>> spawn; // player start in map blocks (converted to Q16.16 exactly once)
    int capture_every = 1;
    std::vector<std::string> capture_kinds;
    std::vector<InputSpan> inputs;
    ToleranceSet tolerances;
};

/// Parses JSON (comments allowed); unknown fields and buttons are errors.
[[nodiscard]] Result<Scenario> parse_scenario(const std::string& json);
[[nodiscard]] Result<Scenario> load_scenario(const std::string& path);
/// Every rule violation, empty when valid.
[[nodiscard]] std::vector<std::string> validate(const Scenario& s);

/// Per-tick input resolved once from the spans (O(1) lookup, overlapping spans OR-ed).
class InputTimeline {
public:
    InputTimeline(int ticks, const std::vector<InputSpan>& spans);
    [[nodiscard]] sim::InputFrame at(i64 tick) const;
    [[nodiscard]] int ticks() const { return static_cast<int>(frames_.size()); }

private:
    std::vector<u16> frames_;
};

} // namespace gta20::testing
