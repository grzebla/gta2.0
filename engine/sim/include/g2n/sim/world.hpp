#pragma once

#include <vector>

#include "g2n/assets/gmp_map.hpp"
#include "g2n/core/angle.hpp"
#include "g2n/core/handle_pool.hpp"
#include "g2n/core/rng.hpp"
#include "g2n/core/vec.hpp"
#include "g2n/sim/input.hpp"

namespace g2n::sim {

/// PROTOTYPE movement rules used for the vertical slice and the conformance pipeline.
/// The constants are invented and do NOT describe GTA 2; they will be replaced by
/// behaviour measured on the original game (docs/reference/gta2re.md).
struct PrototypeRules {
    Fixed max_speed = Fixed::from_ratio(8, 1);     // blocks per second
    Fixed acceleration = Fixed::from_ratio(12, 1); // blocks per second^2
    Fixed braking = Fixed::from_ratio(24, 1);
    Fixed drag = Fixed::from_ratio(10, 1);
    u32 turn_rate_degrees = 270;                 // degrees per second
    Fixed body_radius = Fixed::from_ratio(1, 4); // blocks
};

struct Player {
    EntityId id;
    FixedVec2 position; // map blocks
    Angle heading;
    Fixed speed;   // blocks per second
    int level = 2; // z layer the body occupies
};

/// SYNTHETIC wandering agent: a grid random walk driven by the seeded RNG.
/// Exists to exercise multi-entity capture and RNG determinism; not GTA 2 AI.
struct Wanderer {
    EntityId id;
    int cell_x = 0;
    int cell_y = 0;
};

struct WorldSettings {
    u32 tick_rate_hz = 30;
    u64 seed = 0;
    FixedVec2 spawn{Fixed::from_ratio(975, 10), Fixed::from_ratio(975, 10)}; // road crossing in the synthetic map
    int level = 2;
    int wanderers = 2;
    PrototypeRules rules{};
};

/// The whole simulation state. Deterministic: same settings + same inputs give
/// bit-identical state on every platform (fixed point only, no floating point).
class World {
public:
    /// map may be null (open plane without collision). It must outlive the World.
    World(const assets::GmpMap* map, const WorldSettings& settings);

    /// Advances exactly one fixed tick.
    void step(InputFrame input);

    [[nodiscard]] u64 tick() const { return tick_; }
    [[nodiscard]] const Player& player() const { return player_; }
    [[nodiscard]] const std::vector<Wanderer>& wanderers() const { return wanderers_; }
    [[nodiscard]] const WorldSettings& settings() const { return settings_; }
    [[nodiscard]] const assets::GmpMap* map() const { return map_; }

    /// True when a body may stand in map cell (x, y) on the given level.
    [[nodiscard]] bool cell_passable(int x, int y, int level) const;

private:
    void move_player(InputFrame input);
    bool body_fits(FixedVec2 center) const;

    const assets::GmpMap* map_;
    WorldSettings settings_;
    HandlePool ids_;
    SplitMix64 rng_;
    Player player_;
    std::vector<Wanderer> wanderers_;
    u64 tick_ = 0;
    // Per-tick deltas derived once from the per-second rules.
    Fixed accel_per_tick_, brake_per_tick_, drag_per_tick_, max_speed_;
    Fixed dt_;
    Angle turn_per_tick_;
};

} // namespace g2n::sim
