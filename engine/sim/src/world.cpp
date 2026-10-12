#include "g2n/sim/world.hpp"

namespace g2n::sim {

World::World(const assets::GmpMap* map, const WorldSettings& settings)
    : map_(map), settings_(settings), rng_(settings.seed) {
    const i32 rate = static_cast<i32>(settings.tick_rate_hz == 0 ? 1 : settings.tick_rate_hz);
    dt_ = Fixed::from_ratio(1, rate);
    accel_per_tick_ = settings.rules.acceleration * dt_;
    brake_per_tick_ = settings.rules.braking * dt_;
    drag_per_tick_ = settings.rules.drag * dt_;
    max_speed_ = settings.rules.max_speed;
    turn_per_tick_ = Angle::from_units(static_cast<u16>(
        (static_cast<u64>(settings.rules.turn_rate_degrees) * Angle::kFullTurn) / (360U * static_cast<u32>(rate))));

    player_.id = ids_.allocate();
    player_.position = settings.spawn;
    player_.level = settings.level;

    const int spawn_x = settings.spawn.x.floor_int();
    const int spawn_y = settings.spawn.y.floor_int();
    for (int i = 0; i < settings.wanderers; ++i) {
        wanderers_.push_back({ids_.allocate(), spawn_x + 2 + i, spawn_y});
    }
}

bool World::cell_passable(int x, int y, int level) const {
    if (x < 1 || y < 1 || x > assets::kMapWidth - 2 || y > assets::kMapHeight - 2) {
        return false; // one-block unused border, as in the map format document
    }
    if (map_ == nullptr) {
        return true;
    }
    const auto& block = map_->block_at(x, y, level);
    // Prototype rule: a block with a lid or any wall face on the body's level is solid.
    return block.lid == 0 && !block.left_face().wall && !block.right_face().wall && !block.top_face().wall &&
           !block.bottom_face().wall;
}

bool World::body_fits(FixedVec2 center) const {
    const Fixed r = settings_.rules.body_radius;
    const int x0 = (center.x - r).floor_int();
    const int x1 = (center.x + r).floor_int();
    const int y0 = (center.y - r).floor_int();
    const int y1 = (center.y + r).floor_int();
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            if (!cell_passable(x, y, player_.level)) {
                return false;
            }
        }
    }
    return true;
}

void World::move_player(InputFrame input) {
    Player& p = player_;
    if (input.held(Button::Left)) {
        p.heading = p.heading - turn_per_tick_;
    }
    if (input.held(Button::Right)) {
        p.heading = p.heading + turn_per_tick_;
    }

    if (input.held(Button::Forward)) {
        p.speed = min(max_speed_, p.speed + accel_per_tick_);
    } else if (input.held(Button::Backward)) {
        p.speed = max(Fixed{}, p.speed - brake_per_tick_);
    } else {
        p.speed = max(Fixed{}, p.speed - drag_per_tick_);
    }

    const Fixed step = p.speed * dt_;
    const FixedVec2 delta{cos(p.heading) * step, sin(p.heading) * step};

    // Axis-separated resolution: blocked on one axis still slides along the other.
    if (const FixedVec2 nx{p.position.x + delta.x, p.position.y}; body_fits(nx)) {
        p.position = nx;
    }
    if (const FixedVec2 ny{p.position.x, p.position.y + delta.y}; body_fits(ny)) {
        p.position = ny;
    }
}

void World::step(InputFrame input) {
    move_player(input);
    for (auto& w : wanderers_) {
        int nx = w.cell_x;
        int ny = w.cell_y;
        switch (rng_.next_below(4)) {
        case 0: ++nx; break;
        case 1: --nx; break;
        case 2: ++ny; break;
        default: --ny; break;
        }
        if (cell_passable(nx, ny, player_.level)) {
            w.cell_x = nx;
            w.cell_y = ny;
        }
    }
    ++tick_;
}

} // namespace g2n::sim
