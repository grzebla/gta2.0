#include "gta20/core/angle.hpp"

#include <array>

namespace gta20 {
namespace {

constexpr int kTableSize = 1024;
constexpr double kPi = 3.141592653589793238462643383279502884;

// Taylor series on [-pi/2, pi/2]; only +, -, *, / so constant evaluation is exact IEEE.
consteval double sin_series(double x) {
    double term = x;
    double sum = x;
    const double x2 = x * x;
    for (int n = 1; n < 14; ++n) {
        term = -term * x2 / static_cast<double>((2 * n) * (2 * n + 1));
        sum += term;
    }
    return sum;
}

consteval double sin_turn_fraction(int index) {
    // Reduce to [-pi/2, pi/2] using quadrant symmetry.
    const int quarter = kTableSize / 4;
    const int quadrant = index / quarter;
    const int within = index % quarter;
    const double x = static_cast<double>(within) / quarter * (kPi / 2);
    switch (quadrant) {
    case 0: return sin_series(x);
    case 1: return sin_series(kPi / 2 - x);
    case 2: return -sin_series(x);
    default: return -sin_series(kPi / 2 - x);
    }
}

consteval std::array<i32, kTableSize> make_table() {
    std::array<i32, kTableSize> table{};
    for (int i = 0; i < kTableSize; ++i) {
        const double scaled = sin_turn_fraction(i) * Fixed::kOneRaw;
        table[static_cast<std::size_t>(i)] = static_cast<i32>(scaled >= 0 ? scaled + 0.5 : scaled - 0.5);
    }
    return table;
}

constexpr std::array<i32, kTableSize> kSinTable = make_table();

} // namespace

Fixed sin(Angle a) {
    return Fixed::from_raw(kSinTable[static_cast<std::size_t>(a.units() >> 6)]);
}

Fixed cos(Angle a) {
    return sin(a + Angle::from_units(16384));
}

} // namespace gta20
