#pragma once

#include "gta20/core/types.hpp"

namespace gta20 {

/// Simulation time that only advances in whole ticks. Elapsed time is derived
/// from the tick count, so it never drifts and is identical on every machine.
class FixedStepClock {
public:
    explicit constexpr FixedStepClock(u32 tick_rate_hz) : rate_(tick_rate_hz == 0 ? 1 : tick_rate_hz) {}

    [[nodiscard]] constexpr u32 tick_rate_hz() const { return rate_; }
    [[nodiscard]] constexpr u64 tick() const { return tick_; }
    [[nodiscard]] constexpr u64 elapsed_ns() const { return tick_ * 1'000'000'000ULL / rate_; }
    /// Length of the next tick in ns; consecutive deltas sum exactly to elapsed_ns().
    [[nodiscard]] constexpr u64 next_delta_ns() const { return (tick_ + 1) * 1'000'000'000ULL / rate_ - elapsed_ns(); }
    constexpr void advance() { ++tick_; }

private:
    u32 rate_;
    u64 tick_ = 0;
};

/// Converts variable real frame time into a whole number of fixed simulation steps
/// (classic "fix your timestep"). Accumulates in ns*rate units: no rounding error.
class FrameAccumulator {
public:
    constexpr FrameAccumulator(u32 tick_rate_hz, u32 max_steps_per_frame)
        : rate_(tick_rate_hz == 0 ? 1 : tick_rate_hz), max_steps_(max_steps_per_frame) {}

    /// Adds real elapsed time and returns how many simulation steps to run now.
    /// If the app falls behind by more than max_steps_per_frame, the excess is
    /// dropped (the game slows down instead of spiralling).
    constexpr u32 add(u64 real_elapsed_ns) {
        accumulated_ += real_elapsed_ns * rate_;
        u32 steps = 0;
        while (accumulated_ >= kNsPerSecond && steps < max_steps_) {
            accumulated_ -= kNsPerSecond;
            ++steps;
        }
        if (accumulated_ >= kNsPerSecond) {
            dropped_steps_ += accumulated_ / kNsPerSecond;
            accumulated_ %= kNsPerSecond;
        }
        return steps;
    }

    /// Fraction [0, 1) of the next step already elapsed - for render interpolation.
    [[nodiscard]] constexpr float alpha() const {
        return static_cast<float>(static_cast<double>(accumulated_) / static_cast<double>(kNsPerSecond));
    }
    [[nodiscard]] constexpr u64 dropped_steps() const { return dropped_steps_; }

private:
    static constexpr u64 kNsPerSecond = 1'000'000'000ULL;
    u32 rate_;
    u32 max_steps_;
    u64 accumulated_ = 0;
    u64 dropped_steps_ = 0;
};

} // namespace gta20
