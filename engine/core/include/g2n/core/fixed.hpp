#pragma once

#include <compare>

#include "g2n/core/types.hpp"

namespace g2n {

/// Signed Q16.16 fixed-point number.
///
/// The simulation uses fixed point so that results are bit-identical on every
/// compiler, CPU and OS (no floating-point contraction, no libm differences).
/// Range: about +/-32768 with a resolution of 1/65536. Arithmetic wraps on
/// overflow (well-defined: the 64-bit intermediate is converted modulo 2^32).
/// This is our engine's representation, not a claim about GTA 2 internals.
class Fixed {
public:
    static constexpr int kFractionBits = 16;
    static constexpr i32 kOneRaw = i32{1} << kFractionBits;

    constexpr Fixed() = default;

    [[nodiscard]] static constexpr Fixed from_raw(i32 raw) {
        Fixed f;
        f.raw_ = raw;
        return f;
    }

    [[nodiscard]] static constexpr Fixed from_int(i32 value) {
        return from_raw(static_cast<i32>(static_cast<i64>(value) * kOneRaw));
    }

    /// Exact num/den, truncated toward zero.
    [[nodiscard]] static constexpr Fixed from_ratio(i32 num, i32 den) {
        return from_raw(static_cast<i32>((static_cast<i64>(num) * kOneRaw) / den));
    }

    [[nodiscard]] constexpr i32 raw() const { return raw_; }

    /// Largest integer <= value.
    [[nodiscard]] constexpr i32 floor_int() const { return raw_ >> kFractionBits; }

    /// For export/rendering only - never feed back into the simulation.
    [[nodiscard]] constexpr double to_double() const { return static_cast<double>(raw_) / kOneRaw; }
    [[nodiscard]] constexpr float to_float() const { return static_cast<float>(raw_) / static_cast<float>(kOneRaw); }

    constexpr Fixed operator-() const { return from_raw(static_cast<i32>(-static_cast<i64>(raw_))); }

    friend constexpr Fixed operator+(Fixed a, Fixed b) {
        return from_raw(static_cast<i32>(static_cast<i64>(a.raw_) + b.raw_));
    }
    friend constexpr Fixed operator-(Fixed a, Fixed b) {
        return from_raw(static_cast<i32>(static_cast<i64>(a.raw_) - b.raw_));
    }
    friend constexpr Fixed operator*(Fixed a, Fixed b) {
        return from_raw(static_cast<i32>((static_cast<i64>(a.raw_) * b.raw_) >> kFractionBits));
    }
    /// Truncates toward zero. Division by zero is a precondition violation.
    friend constexpr Fixed operator/(Fixed a, Fixed b) {
        return from_raw(static_cast<i32>((static_cast<i64>(a.raw_) * kOneRaw) / b.raw_));
    }

    constexpr Fixed& operator+=(Fixed o) { return *this = *this + o; }
    constexpr Fixed& operator-=(Fixed o) { return *this = *this - o; }
    constexpr Fixed& operator*=(Fixed o) { return *this = *this * o; }

    friend constexpr auto operator<=>(Fixed, Fixed) = default;

private:
    i32 raw_ = 0;
};

[[nodiscard]] constexpr Fixed min(Fixed a, Fixed b) {
    return a < b ? a : b;
}
[[nodiscard]] constexpr Fixed max(Fixed a, Fixed b) {
    return a < b ? b : a;
}
[[nodiscard]] constexpr Fixed clamp(Fixed v, Fixed lo, Fixed hi) {
    return min(max(v, lo), hi);
}

namespace fixed_literals {
[[nodiscard]] consteval Fixed operator""_fx(unsigned long long v) {
    return Fixed::from_int(static_cast<i32>(v));
}
} // namespace fixed_literals

} // namespace g2n
