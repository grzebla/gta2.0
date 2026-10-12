#include "g2n/assets/sty_style.hpp"

#include <algorithm>
#include <cstring>
#include <string>

#include "g2n/assets/chunk_file.hpp"
#include "g2n/core/byte_reader.hpp"

namespace g2n::assets {
namespace {

std::vector<u8> copy_bytes(std::span<const std::byte> s) {
    std::vector<u8> v(s.size());
    if (!s.empty()) {
        std::memcpy(v.data(), s.data(), s.size());
    }
    return v;
}

} // namespace

u16 StyStyle::tile_palette(u16 tile) const {
    const std::size_t v = static_cast<std::size_t>(palette_bases_.tile) + tile;
    return v < palette_index_.size() ? palette_index_[v] : 0;
}

Rgba8 StyStyle::palette_colour(u16 physical, u8 index) const {
    if (index == 0 || physical >= physical_palette_count()) {
        return {};
    }
    // Palettes are stored in dword-wide columns down each 64 KiB page: colour i of the
    // palette in column c sits at row i (row = 256 bytes).
    const std::size_t page = physical / kPalettesPerPage;
    const std::size_t column = physical % kPalettesPerPage;
    const std::size_t at = page * kPageBytes + static_cast<std::size_t>(index) * kStylePageSize + column * 4;
    return {palettes_[at + 2], palettes_[at + 1], palettes_[at], 255}; // stored as BGRA
}

u8 StyStyle::tile_pixel(u16 tile, int x, int y) const {
    const std::size_t page = tile / kTilesPerPage;
    const std::size_t slot = tile % kTilesPerPage;
    const std::size_t px = (slot % 4) * kTileSize + static_cast<std::size_t>(x);
    const std::size_t py = (slot / 4) * kTileSize + static_cast<std::size_t>(y);
    return tiles_[page * kPageBytes + py * kStylePageSize + px];
}

void StyStyle::decode_tile(u16 tile, std::span<u8> out) const {
    if (tile >= tile_count() || out.size() < static_cast<std::size_t>(kTileSize) * kTileSize * 4) {
        return;
    }
    const u16 palette = tile_palette(tile);
    for (int y = 0; y < kTileSize; ++y) {
        for (int x = 0; x < kTileSize; ++x) {
            const Rgba8 c = palette_colour(palette, tile_pixel(tile, x, y));
            const std::size_t at = (static_cast<std::size_t>(y) * kTileSize + static_cast<std::size_t>(x)) * 4;
            out[at] = c.r;
            out[at + 1] = c.g;
            out[at + 2] = c.b;
            out[at + 3] = c.a;
        }
    }
}

bool StyStyle::decode_sprite(u16 index, std::vector<u8>& rgba, int& width, int& height) const {
    if (index >= sprites_.size()) {
        return false;
    }
    const SpriteEntry& e = sprites_[index];
    const std::size_t v = static_cast<std::size_t>(palette_bases_.sprite) + index;
    const u16 palette = v < palette_index_.size() ? palette_index_[v] : 0;
    width = e.width;
    height = e.height;
    rgba.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4, 0);
    const std::size_t page = e.offset / kPageBytes;
    const std::size_t x0 = e.offset % kStylePageSize;
    const std::size_t y0 = (e.offset % kPageBytes) / kStylePageSize;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const u8 p = sprite_pixels_[page * kPageBytes + (y0 + static_cast<std::size_t>(y)) * kStylePageSize + x0 +
                                        static_cast<std::size_t>(x)];
            const Rgba8 c = palette_colour(palette, p);
            const std::size_t at =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4;
            rgba[at] = c.r;
            rgba[at + 1] = c.g;
            rgba[at + 2] = c.b;
            rgba[at + 3] = c.a;
        }
    }
    return true;
}

Result<StyStyle> parse_sty(std::span<const std::byte> bytes) {
    auto container = parse_chunk_file(bytes);
    if (!container) {
        return container.error();
    }
    const ChunkFile& file = container.value();
    if (file.file_type != "GBST") {
        return make_error("not a GTA 2 style: file type '" + file.file_type + "'");
    }
    if (file.version != 600 && file.version != 700) {
        return make_error("unsupported GBST version " + std::to_string(file.version) + " (known: 600, 700)");
    }

    StyStyle style;
    style.version_ = file.version;

    const auto palx = file.find("PALX");
    const auto ppal = file.find("PPAL");
    const auto palb = file.find("PALB");
    if (!palx || !ppal || !palb) {
        return make_error("style lacks PALX, PPAL or PALB");
    }
    if (palx->data.size() != 16384 * 2) {
        return make_error("PALX must hold 16384 u16 entries");
    }
    if (ppal->data.empty() || ppal->data.size() % kPageBytes != 0) {
        return make_error("PPAL size must be a non-zero multiple of 64 KiB");
    }
    if (palb->data.size() != 16) {
        return make_error("PALB must be 16 bytes");
    }

    ByteReader rx(palx->data);
    style.palette_index_.resize(16384);
    for (auto& v : style.palette_index_) {
        v = rx.u16le();
    }
    style.palettes_ = copy_bytes(ppal->data);

    // Counts -> cumulative bases ("each value is set to the total of the values before it").
    ByteReader rb(palb->data);
    std::array<u16, 8> counts{};
    for (auto& c : counts) {
        c = rb.u16le();
    }
    std::array<u16, 8> bases{};
    u32 total = 0;
    for (std::size_t i = 0; i < counts.size(); ++i) {
        bases[i] = static_cast<u16>(total);
        total += counts[i];
    }
    if (total > 16384) {
        return make_error("PALB counts exceed the 16384 virtual palettes");
    }
    style.palette_bases_ = {bases[0], bases[1], bases[2], bases[3], bases[4], bases[5], bases[6], bases[7]};
    const auto physical = style.physical_palette_count();
    for (u32 v = 0; v < total; ++v) {
        if (style.palette_index_[v] >= physical) {
            return make_error("PALX entry " + std::to_string(v) + " references physical palette " +
                              std::to_string(style.palette_index_[v]) + " but only " + std::to_string(physical) +
                              " exist");
        }
    }

    if (const auto tile = file.find("TILE")) {
        if (tile->data.size() % kPageBytes != 0) {
            return make_error("TILE size must be a multiple of 64 KiB");
        }
        style.tiles_ = copy_bytes(tile->data);
        if (style.tile_count() > counts[0] && counts[0] != 0) {
            // tile pages are padded to whole pages; only counts[0] tiles have palettes
        }
    }

    if (const auto sprx = file.find("SPRX")) {
        const auto sprg = file.find("SPRG");
        const auto sprb = file.find("SPRB");
        if (!sprg || !sprb || sprx->data.size() % 8 != 0 || sprb->data.size() != 12 ||
            sprg->data.size() % kPageBytes != 0) {
            return make_error("inconsistent sprite chunks (SPRX/SPRG/SPRB)");
        }
        style.sprite_pixels_ = copy_bytes(sprg->data);
        ByteReader rs(sprx->data);
        while (rs.remaining() > 0) {
            SpriteEntry e;
            e.offset = rs.u32le();
            e.width = rs.u8le();
            e.height = rs.u8le();
            (void)rs.u16le(); // pad
            const std::size_t x = e.offset % kStylePageSize;
            const std::size_t y = (e.offset % kPageBytes) / kStylePageSize;
            const std::size_t page = e.offset / kPageBytes;
            if (page * kPageBytes >= style.sprite_pixels_.size() || x + e.width > kStylePageSize ||
                y + e.height > kStylePageSize) {
                return make_error("sprite " + std::to_string(style.sprites_.size()) + " lies outside SPRG");
            }
            style.sprites_.push_back(e);
        }
        ByteReader rsb(sprb->data);
        std::array<u16, 6> sc{};
        for (auto& c : sc) {
            c = rsb.u16le();
        }
        u32 sprite_total = 0;
        std::array<u16, 6> sb{};
        for (std::size_t i = 0; i < sc.size(); ++i) {
            sb[i] = static_cast<u16>(sprite_total);
            sprite_total += sc[i];
        }
        if (sprite_total != style.sprites_.size()) {
            return make_error("SPRB counts (" + std::to_string(sprite_total) + ") do not match SPRX entries (" +
                              std::to_string(style.sprites_.size()) + ")");
        }
        style.sprite_bases_ = {sb[0], sb[1], sb[2], sb[3], sb[4], sb[5]};
    }
    return style;
}

} // namespace g2n::assets
