#pragma once

#include "gta20/core/fixed.hpp"

namespace gta20 {

struct FixedVec2 {
    Fixed x;
    Fixed y;

    friend constexpr FixedVec2 operator+(FixedVec2 a, FixedVec2 b) { return {a.x + b.x, a.y + b.y}; }
    friend constexpr FixedVec2 operator-(FixedVec2 a, FixedVec2 b) { return {a.x - b.x, a.y - b.y}; }
    friend constexpr FixedVec2 operator*(FixedVec2 a, Fixed s) { return {a.x * s, a.y * s}; }
    friend constexpr bool operator==(FixedVec2, FixedVec2) = default;
};

struct Vec2f {
    float x = 0;
    float y = 0;

    friend constexpr Vec2f operator+(Vec2f a, Vec2f b) { return {a.x + b.x, a.y + b.y}; }
    friend constexpr Vec2f operator-(Vec2f a, Vec2f b) { return {a.x - b.x, a.y - b.y}; }
    friend constexpr Vec2f operator*(Vec2f a, float s) { return {a.x * s, a.y * s}; }
    friend constexpr bool operator==(Vec2f, Vec2f) = default;
};

[[nodiscard]] constexpr Vec2f to_vec2f(FixedVec2 v) {
    return {v.x.to_float(), v.y.to_float()};
}

[[nodiscard]] constexpr Vec2f lerp(Vec2f a, Vec2f b, float t) {
    return a + (b - a) * t;
}

} // namespace gta20
