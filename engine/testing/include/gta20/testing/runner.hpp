#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gta20/assets/gmp_map.hpp"
#include "gta20/testing/trace.hpp"

namespace gta20::testing {

struct StepInput {
    i64 tick = 0;
    u32 tick_rate_hz = 0;
    sim::InputFrame input;
};

/// Bridge between the runner and a simulation (our engine, or a recorder attached
/// to the original game). Must be deterministic.
class SimulationAdapter {
public:
    virtual ~SimulationAdapter() = default;
    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual std::string version() const = 0;
    virtual Result<bool> initialize(const Scenario& scenario) = 0;
    virtual void step(const StepInput& input) = 0;
    [[nodiscard]] virtual std::vector<EntityState> capture() const = 0;
};

/// Our engine's gta20::sim::World, headless. Uses the scenario's map from game_dir if set,
/// otherwise the SYNTHETIC test map.
[[nodiscard]] std::unique_ptr<SimulationAdapter> make_engine_adapter(std::string game_dir = {});

/// Runs the scenario: snapshot at tick 0, then after every `capture.everyTicks` ticks and after the last.
[[nodiscard]] Result<Trace> run_scenario(const Scenario& scenario, SimulationAdapter& adapter);

} // namespace gta20::testing
