#pragma once

#include "gta20/core/types.hpp"

namespace gta20::sim {

/// Logical player controls (GTA 2 keyboard actions), independent of key bindings.
enum class Button : u8 {
    Forward,
    Backward,
    Left,
    Right,
    Attack,
    Jump,
    EnterExit,
    PreviousWeapon,
    NextWeapon,
    Special,
    Count
};

/// Buttons held during one simulation tick.
struct InputFrame {
    u16 buttons = 0;

    [[nodiscard]] constexpr bool held(Button b) const { return (buttons & bit(b)) != 0; }
    constexpr void set(Button b, bool down) {
        buttons = static_cast<u16>(down ? (buttons | bit(b)) : (buttons & ~bit(b)));
    }
    [[nodiscard]] static constexpr u16 bit(Button b) { return static_cast<u16>(1U << static_cast<unsigned>(b)); }
    friend constexpr bool operator==(InputFrame, InputFrame) = default;
};

} // namespace gta20::sim
