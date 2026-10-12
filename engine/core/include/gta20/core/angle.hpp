#pragma once

#include <compare>

#include "gta20/core/fixed.hpp"

namespace gta20 {

/// Binary angle: 65536 units = one full turn, wraps naturally on overflow.
/// 0 points along +x, angles grow counter-clockwise in a y-up frame
/// (clockwise on screen, where y grows downwards).
class Angle {
public:
    static constexpr u32 kFullTurn = 65536;

    constexpr Angle() = default;
    [[nodiscard]] static constexpr Angle from_units(u16 units) {
        Angle a;
        a.units_ = units;
        return a;
    }
    [[nodiscard]] static constexpr Angle from_degrees(i32 degrees) {
        const i64 wrapped = ((static_cast<i64>(degrees) % 360) + 360) % 360;
        return from_units(static_cast<u16>((wrapped * kFullTurn) / 360));
    }

    [[nodiscard]] constexpr u16 units() const { return units_; }
    [[nodiscard]] constexpr double to_degrees() const { return static_cast<double>(units_) * 360.0 / kFullTurn; }
    [[nodiscard]] constexpr double to_radians() const {
        return static_cast<double>(units_) * 6.283185307179586 / kFullTurn;
    }

    friend constexpr Angle operator+(Angle a, Angle b) { return from_units(static_cast<u16>(a.units_ + b.units_)); }
    friend constexpr Angle operator-(Angle a, Angle b) { return from_units(static_cast<u16>(a.units_ - b.units_)); }
    friend constexpr auto operator<=>(Angle, Angle) = default;

private:
    u16 units_ = 0;
};

/// Deterministic sine/cosine from a 1024-entry table (no libm at run time).
/// The table is computed at compile time with exact IEEE arithmetic, so it is
/// identical for every compiler. Resolution: 1/1024 turn (nearest lower entry).
[[nodiscard]] Fixed sin(Angle a);
[[nodiscard]] Fixed cos(Angle a);

} // namespace gta20
