#pragma once

#include <vector>

#include "g2n/assets/gmp_map.hpp"

namespace g2n::assets {

/// Builds GBMP files in the documented DMAP layout. Used for SYNTHETIC test maps
/// and round-trip tests; it does not reproduce any original map.
class GmpWriter {
public:
    /// Sets the block at (x, y, z). Block 0 (empty) is implicit.
    void set_block(int x, int y, int z, const BlockInfo& block);
    void add_zone(MapZone zone) { zones_.push_back(std::move(zone)); }

    [[nodiscard]] std::vector<std::byte> build_dmap() const;

private:
    std::vector<BlockInfo> blocks_{BlockInfo{}};
    std::vector<u32> grid_ = std::vector<u32>(static_cast<std::size_t>(kMapWidth) * kMapHeight * kMapLevels, 0);
    std::vector<MapZone> zones_;
};

/// SYNTHETIC demo city (roads, pavements, walled buildings) used by the app and tests
/// when no original map is available. Not GTA 2 content.
[[nodiscard]] std::vector<std::byte> build_synthetic_test_map();

} // namespace g2n::assets
