#include "g2n/testing/runner.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <set>

#include "g2n/assets/file_io.hpp"
#include "g2n/assets/gmp_writer.hpp"
#include "g2n/sim/world.hpp"

namespace g2n::testing {
namespace {

class EngineAdapter final : public SimulationAdapter {
public:
    explicit EngineAdapter(std::string game_dir) : game_dir_(std::move(game_dir)) {}

    std::string id() const override { return "engine"; }
    // Bump when the captured semantics of g2n::sim change (golden traces must be regenerated).
    std::string version() const override { return "prototype-1"; }

    Result<bool> initialize(const Scenario& scenario) override {
        std::vector<std::byte> bytes;
        if (scenario.world_map.empty()) {
            bytes = assets::build_synthetic_test_map();
        } else {
            if (game_dir_.empty()) {
                return make_error("scenario needs world.map '" + scenario.world_map +
                                  "' but no game directory was given");
            }
            auto path = assets::resolve_game_path(game_dir_, scenario.world_map);
            if (!path)
                return path.error();
            auto file = assets::read_file(path.value());
            if (!file)
                return file.error();
            bytes = std::move(file.value());
        }
        auto map = assets::parse_gmp(bytes);
        if (!map)
            return map.error();
        map_ = std::move(map.value());

        sim::WorldSettings settings;
        settings.tick_rate_hz = static_cast<u32>(scenario.tick_rate_hz);
        settings.seed = scenario.seed;
        if (scenario.spawn) {
            // The only float -> fixed conversion: done once, rounding to the nearest 1/65536.
            const auto to_fixed = [](double v) {
                return Fixed::from_raw(static_cast<i32>(std::llround(v * Fixed::kOneRaw)));
            };
            settings.spawn = {to_fixed(scenario.spawn->first), to_fixed(scenario.spawn->second)};
        }
        world_.emplace(&map_.value(), settings);
        return true;
    }

    void step(const StepInput& input) override {
        if (world_) {
            world_->step(input.input);
        }
    }

    std::vector<EntityState> capture() const override {
        std::vector<EntityState> out;
        if (!world_) {
            return out; // not initialised
        }
        const auto& p = world_->player();
        // Values are exact: Q16.16 converts to double without rounding.
        out.push_back({"player",
                       "player",
                       {{"headingUnits", static_cast<double>(p.heading.units())},
                        {"speed", p.speed.to_double()},
                        {"x", p.position.x.to_double()},
                        {"y", p.position.y.to_double()}}});
        int i = 0;
        for (const auto& w : world_->wanderers()) {
            out.push_back({"wanderer-" + std::to_string(i++),
                           "synthetic-wanderer",
                           {{"cellX", static_cast<double>(w.cell_x)}, {"cellY", static_cast<double>(w.cell_y)}}});
        }
        return out;
    }

private:
    std::string game_dir_;
    std::optional<assets::GmpMap> map_;
    std::optional<sim::World> world_;
};

TickSnapshot snapshot(i64 tick, const SimulationAdapter& adapter, const std::set<std::string>& kinds) {
    TickSnapshot s{tick, adapter.capture()};
    if (!kinds.empty()) {
        std::erase_if(s.entities, [&](const EntityState& e) { return !kinds.contains(e.kind); });
    }
    std::sort(s.entities.begin(), s.entities.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    return s;
}

} // namespace

std::unique_ptr<SimulationAdapter> make_engine_adapter(std::string game_dir) {
    return std::make_unique<EngineAdapter>(std::move(game_dir));
}

Result<Trace> run_scenario(const Scenario& scenario, SimulationAdapter& adapter) {
    if (const auto errors = validate(scenario); !errors.empty()) {
        return make_error("invalid scenario: " + errors.front());
    }
    if (auto ok = adapter.initialize(scenario); !ok) {
        return ok.error();
    }
    const std::set<std::string> kinds(scenario.capture_kinds.begin(), scenario.capture_kinds.end());
    const InputTimeline timeline(scenario.ticks, scenario.inputs);

    Trace trace;
    trace.header = {1,
                    scenario.id,
                    scenario.provenance,
                    adapter.id(),
                    adapter.version(),
                    scenario.tick_rate_hz,
                    scenario.ticks,
                    scenario.seed,
                    {}};
    trace.snapshots.push_back(snapshot(0, adapter, kinds));
    for (i64 tick = 0; tick < scenario.ticks; ++tick) {
        adapter.step({tick, static_cast<u32>(scenario.tick_rate_hz), timeline.at(tick)});
        const i64 completed = tick + 1;
        if (completed % scenario.capture_every == 0 || completed == scenario.ticks) {
            trace.snapshots.push_back(snapshot(completed, adapter, kinds));
        }
    }
    for (const auto& s : trace.snapshots) {
        for (std::size_t i = 1; i < s.entities.size(); ++i) {
            if (s.entities[i].id == s.entities[i - 1].id) {
                return make_error("adapter reported duplicate entity id '" + s.entities[i].id + "'");
            }
        }
    }
    return trace;
}

} // namespace g2n::testing
