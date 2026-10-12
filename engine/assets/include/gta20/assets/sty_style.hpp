#pragma once

#include <array>
#include <span>
#include <vector>

#include "gta20/core/result.hpp"
#include "gta20/core/types.hpp"

namespace gta20::assets {

// GTA 2 style (.sty): tiles, sprites and palettes. Based on DMA Design's "GBH Style Format"
// document v4.6 (file type "GBST", version 600). The PC files of the 9.6 release are version
// 700 and add an undocumented SPEC chunk; see docs/formats/sty.md for verified details.

inline constexpr int kTileSize = 64;
inline constexpr int kStylePageSize = 256; // pages are 256x256 bytes (64 KiB)
inline constexpr int kTilesPerPage = 16;   // 4 x 4 tiles of 64x64
inline constexpr int kPaletteColours = 256;
inline constexpr int kPalettesPerPage = 64; // PPAL page: 64 palettes in dword-wide columns
inline constexpr std::size_t kPageBytes = static_cast<std::size_t>(kStylePageSize) * kStylePageSize;

struct Rgba8 {
    u8 r = 0, g = 0, b = 0, a = 0;
    friend constexpr bool operator==(Rgba8, Rgba8) = default;
};

/// PALB: virtual palette counts per graphic type, converted to cumulative bases.
struct PaletteBases {
    u16 tile = 0, sprite = 0, car_remap = 0, ped_remap = 0, code_obj_remap = 0, map_obj_remap = 0, user_remap = 0,
        font_remap = 0;
};

/// SPRB: sprite counts per type, converted to cumulative bases.
struct SpriteBases {
    u16 car = 0, ped = 0, code_obj = 0, map_obj = 0, user = 0, font = 0;
};

struct SpriteEntry {
    u32 offset = 0; // byte offset into SPRG; page = offset / 64K, x = offset % 256, y = (offset % 64K) / 256
    u8 width = 0;
    u8 height = 0;
};

class StyStyle {
public:
    [[nodiscard]] u16 version() const { return version_; }
    [[nodiscard]] std::size_t tile_count() const { return tiles_.size() / (kTileSize * kTileSize); }
    [[nodiscard]] std::size_t physical_palette_count() const { return palettes_.size() / (kPaletteColours * 4); }
    [[nodiscard]] std::size_t sprite_count() const { return sprites_.size(); }
    [[nodiscard]] const PaletteBases& palette_bases() const { return palette_bases_; }
    [[nodiscard]] const SpriteBases& sprite_bases() const { return sprite_bases_; }
    [[nodiscard]] std::span<const SpriteEntry> sprites() const { return sprites_; }

    /// Physical palette used by a tile: PALX[PALB.tile + tile].
    [[nodiscard]] u16 tile_palette(u16 tile) const;
    /// Colour `index` of a physical palette. Colour 0 is transparent in every palette (alpha 0).
    [[nodiscard]] Rgba8 palette_colour(u16 physical, u8 index) const;
    /// Raw 8-bit palette index of a tile pixel.
    [[nodiscard]] u8 tile_pixel(u16 tile, int x, int y) const;
    /// Writes the tile as 64x64 RGBA8 (row-major, 16 KiB) into `out`.
    void decode_tile(u16 tile, std::span<u8> out) const;
    /// Decodes sprite `index` to RGBA8 using palette PALX[PALB.sprite + index]; returns false if out of range.
    bool decode_sprite(u16 index, std::vector<u8>& rgba, int& width, int& height) const;

private:
    friend Result<StyStyle> parse_sty(std::span<const std::byte>);
    u16 version_ = 0;
    std::vector<u16> palette_index_;   // PALX: virtual -> physical
    std::vector<u8> palettes_;         // PPAL pages, raw BGRA
    std::vector<u8> tiles_;            // TILE pages, raw 8-bit
    std::vector<u8> sprite_pixels_;    // SPRG pages, raw 8-bit
    std::vector<SpriteEntry> sprites_; // SPRX
    PaletteBases palette_bases_;
    SpriteBases sprite_bases_;
};

/// Parses a GBST file (versions 600 and 700). Validates sizes and every cross-reference.
[[nodiscard]] Result<StyStyle> parse_sty(std::span<const std::byte> bytes);

} // namespace gta20::assets
