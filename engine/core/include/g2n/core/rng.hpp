#pragma once

#include "g2n/core/types.hpp"

namespace g2n {

/// SplitMix64 (Steele, Lea, Flood 2014). Fully specified and platform-independent;
/// every random decision in the simulation must come from an instance seeded by the scenario.
class SplitMix64 {
public:
    explicit constexpr SplitMix64(u64 seed) : state_(seed) {}

    constexpr u64 next() {
        u64 z = (state_ += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    /// Uniform in [0, bound) without modulo bias. bound must be > 0.
    constexpr u32 next_below(u32 bound) {
        const u64 b = bound;
        const u64 threshold = (0 - b) % b;
        for (;;) {
            const u64 r = next();
            if (r >= threshold) {
                return static_cast<u32>(r % b);
            }
        }
    }

    [[nodiscard]] constexpr u64 state() const { return state_; }

private:
    u64 state_;
};

} // namespace g2n
