#pragma once

#include <array>
#include <span>
#include <string>
#include <vector>

#include "gta20/core/result.hpp"
#include "gta20/core/types.hpp"

namespace gta20::assets {

// GTA 2 map (.gmp) as described by DMA Design's "GTA2 map format" document v8.2
// (see docs/formats/gmp.md for the source and which parts are verified).

inline constexpr int kMapWidth = 256;  // x
inline constexpr int kMapHeight = 256; // y
inline constexpr int kMapLevels = 8;   // z
inline constexpr u16 kGmpVersion = 500;

enum class Rotation : u8 { R0 = 0, R90 = 1, R180 = 2, R270 = 3 };

/// Side face word (left/right/top/bottom).
struct SideFace {
    u16 tile = 0;                     // bits 0-9
    bool wall = false;                // bit 10: collide
    bool bullet_wall = false;         // bit 11
    bool flat = false;                // bit 12
    bool flip = false;                // bit 13
    Rotation rotation = Rotation::R0; // bits 14-15

    [[nodiscard]] static constexpr SideFace decode(u16 w) {
        return {static_cast<u16>(w & 0x3FF), (w & 0x400) != 0,  (w & 0x800) != 0,
                (w & 0x1000) != 0,           (w & 0x2000) != 0, static_cast<Rotation>((w >> 14) & 3)};
    }
};

/// Lid (top) face word.
struct LidFace {
    u16 tile = 0;      // bits 0-9
    u8 lighting = 0;   // bits 10-11
    bool flat = false; // bit 12
    bool flip = false; // bit 13
    Rotation rotation = Rotation::R0;

    [[nodiscard]] static constexpr LidFace decode(u16 w) {
        return {static_cast<u16>(w & 0x3FF), static_cast<u8>((w >> 10) & 3), (w & 0x1000) != 0, (w & 0x2000) != 0,
                static_cast<Rotation>((w >> 14) & 3)};
    }
};

enum class GroundType : u8 { Air = 0, Road = 1, Pavement = 2, Field = 3 };

/// One 12-byte block_info record, raw words kept for lossless round trips.
struct BlockInfo {
    u16 left = 0, right = 0, top = 0, bottom = 0, lid = 0;
    u8 arrows = 0;
    u8 slope_type = 0;

    [[nodiscard]] GroundType ground() const { return static_cast<GroundType>(slope_type & 3); }
    [[nodiscard]] u8 slope() const { return static_cast<u8>(slope_type >> 2); }
    [[nodiscard]] LidFace lid_face() const { return LidFace::decode(lid); }
    [[nodiscard]] SideFace left_face() const { return SideFace::decode(left); }
    [[nodiscard]] SideFace right_face() const { return SideFace::decode(right); }
    [[nodiscard]] SideFace top_face() const { return SideFace::decode(top); }
    [[nodiscard]] SideFace bottom_face() const { return SideFace::decode(bottom); }
    [[nodiscard]] bool empty() const {
        return left == 0 && right == 0 && top == 0 && bottom == 0 && lid == 0 && arrows == 0 && slope_type == 0;
    }
    friend bool operator==(const BlockInfo&, const BlockInfo&) = default;
};

struct MapZone {
    u8 type = 0;
    u8 x = 0, y = 0, w = 0, h = 0;
    std::string name;
};

enum class MapEncoding { Uncompressed, Compressed16, Compressed32 };

/// Decoded map: unique blocks + a 256x256x8 grid of indices into them.
class GmpMap {
public:
    GmpMap();

    [[nodiscard]] const BlockInfo& block_at(int x, int y, int z) const;
    [[nodiscard]] u32 block_index_at(int x, int y, int z) const;
    [[nodiscard]] std::span<const BlockInfo> unique_blocks() const { return blocks_; }
    [[nodiscard]] std::span<const MapZone> zones() const { return zones_; }
    [[nodiscard]] MapEncoding encoding() const { return encoding_; }
    /// Highest z with a non-empty block in column (x, y), or -1.
    [[nodiscard]] int top_level(int x, int y) const;

private:
    friend Result<GmpMap> parse_gmp(std::span<const std::byte>, MapEncoding);
    static std::size_t cell(int x, int y, int z) {
        return (static_cast<std::size_t>(z) * kMapHeight + static_cast<std::size_t>(y)) * kMapWidth +
               static_cast<std::size_t>(x);
    }

    std::vector<BlockInfo> blocks_; // blocks_[0] is the empty block
    std::vector<u32> grid_;         // kMapWidth * kMapHeight * kMapLevels indices into blocks_
    std::vector<MapZone> zones_;
    MapEncoding encoding_ = MapEncoding::Compressed32;
};

/// Parses a GBMP file. Uses the preferred encoding if present, else DMAP, CMAP, UMAP in that
/// order. Unknown chunks (e.g. EDIT, present in every original map) are ignored. Original PC maps
/// contain both DMAP and CMAP, but CMAP is a different (older) version of the city, so DMAP is the
/// default. The real-data test only checks wil.gmp and mike1m.gmp: both chunks decode and DMAP has
/// more unique blocks than CMAP; it does not compare cells (docs/formats/gmp.md).
[[nodiscard]] Result<GmpMap> parse_gmp(std::span<const std::byte> bytes,
                                       MapEncoding preferred = MapEncoding::Compressed32);

} // namespace gta20::assets
