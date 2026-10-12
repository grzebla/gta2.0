#include "gta20/assets/gmp_map.hpp"

#include "gta20/assets/chunk_file.hpp"
#include "gta20/core/byte_reader.hpp"

namespace gta20::assets {
namespace {

BlockInfo read_block(ByteReader& r) {
    BlockInfo b;
    b.left = r.u16le();
    b.right = r.u16le();
    b.top = r.u16le();
    b.bottom = r.u16le();
    b.lid = r.u16le();
    b.arrows = r.u8le();
    b.slope_type = r.u8le();
    return b;
}

constexpr std::size_t kColumns = static_cast<std::size_t>(kMapWidth) * kMapHeight;

// Compressed map (DMAP: 32-bit, CMAP: 16-bit). Column layout per the format document:
// u8 height, u8 offset, [u16 pad in DMAP], then block numbers for z = offset .. height-1.
// NOTE: the document sizes blockd as "variable size - see height"; we read height - offset
// entries (one per non-empty level). Not yet verified against an original map file.
Result<bool> decode_compressed(std::span<const std::byte> data, bool wide, std::vector<BlockInfo>& blocks,
                               std::vector<u32>& grid) {
    ByteReader r(data);
    std::vector<u32> base(kColumns);
    for (auto& b : base) {
        b = wide ? r.u32le() : r.u16le();
    }
    const u32 column_words = wide ? r.u32le() : r.u16le();
    const auto column_bytes = r.take(static_cast<std::size_t>(column_words) * (wide ? 4 : 2));
    const u32 num_blocks = wide ? r.u32le() : r.u16le();
    if (r.failed()) {
        return make_error("compressed map header truncated");
    }
    if (static_cast<u64>(num_blocks) * 12 > r.remaining()) {
        return make_error("compressed map declares " + std::to_string(num_blocks) + " blocks but data is shorter");
    }
    blocks.resize(num_blocks);
    for (auto& b : blocks) {
        b = read_block(r);
    }
    if (blocks.empty()) {
        blocks.emplace_back(); // always provide the empty block 0
    }

    const std::size_t word = wide ? 4 : 2;
    for (std::size_t c = 0; c < kColumns; ++c) {
        const std::size_t byte_offset = static_cast<std::size_t>(base[c]) * word;
        if (byte_offset >= column_bytes.size()) {
            return make_error("column " + std::to_string(c) + " offset outside column data");
        }
        ByteReader col(column_bytes.subspan(byte_offset));
        const u8 height = col.u8le();
        const u8 offset = col.u8le();
        if (wide) {
            (void)col.u16le(); // pad
        }
        if (height > kMapLevels || offset > height) {
            return make_error("column " + std::to_string(c) + " has invalid height/offset " + std::to_string(height) +
                              "/" + std::to_string(offset));
        }
        for (int z = offset; z < height; ++z) {
            const u32 block = wide ? col.u32le() : col.u16le();
            if (col.failed()) {
                return make_error("column " + std::to_string(c) + " truncated");
            }
            if (block >= blocks.size()) {
                return make_error("column " + std::to_string(c) + " references block " + std::to_string(block));
            }
            grid[static_cast<std::size_t>(z) * kColumns + c] = block;
        }
    }
    return true;
}

Result<bool> decode_uncompressed(std::span<const std::byte> data, std::vector<BlockInfo>& blocks,
                                 std::vector<u32>& grid) {
    // block_info city_scape[8][256][256]: z outermost, then y (rows), then x.
    // The document lists dimensions as [z][x][y] with x,y 0..255; we store row-major y,x.
    if (data.size() != kColumns * kMapLevels * 12) {
        return make_error("UMAP size " + std::to_string(data.size()) + " != 8*256*256*12");
    }
    ByteReader r(data);
    blocks.assign(1, BlockInfo{});
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const BlockInfo b = read_block(r);
        if (!b.empty()) {
            blocks.push_back(b);
            grid[i] = static_cast<u32>(blocks.size() - 1);
        }
    }
    return true;
}

Result<std::vector<MapZone>> decode_zones(std::span<const std::byte> data) {
    ByteReader r(data);
    std::vector<MapZone> zones;
    while (r.remaining() > 0) {
        MapZone z;
        z.type = r.u8le();
        z.x = r.u8le();
        z.y = r.u8le();
        z.w = r.u8le();
        z.h = r.u8le();
        const u8 length = r.u8le();
        const auto name = r.take(length);
        if (r.failed()) {
            return make_error("ZONE chunk truncated");
        }
        z.name.assign(reinterpret_cast<const char*>(name.data()), name.size());
        zones.push_back(std::move(z));
    }
    return zones;
}

} // namespace

GmpMap::GmpMap() : blocks_(1), grid_(kColumns * kMapLevels, 0) {
}

u32 GmpMap::block_index_at(int x, int y, int z) const {
    if (x < 0 || y < 0 || z < 0 || x >= kMapWidth || y >= kMapHeight || z >= kMapLevels) {
        return 0;
    }
    return grid_[cell(x, y, z)];
}

const BlockInfo& GmpMap::block_at(int x, int y, int z) const {
    return blocks_[block_index_at(x, y, z)];
}

int GmpMap::top_level(int x, int y) const {
    for (int z = kMapLevels - 1; z >= 0; --z) {
        if (block_index_at(x, y, z) != 0 && !block_at(x, y, z).empty()) {
            return z;
        }
    }
    return -1;
}

Result<GmpMap> parse_gmp(std::span<const std::byte> bytes, MapEncoding preferred) {
    auto container = parse_chunk_file(bytes);
    if (!container) {
        return container.error();
    }
    const ChunkFile& file = container.value();
    if (file.file_type != "GBMP") {
        return make_error("not a GTA 2 map: file type '" + file.file_type + "'");
    }
    if (file.version != kGmpVersion) {
        return make_error("unsupported GBMP version " + std::to_string(file.version));
    }

    GmpMap map;
    Result<bool> decoded = make_error("no map data chunk (DMAP, CMAP or UMAP)");
    const bool want_cmap = preferred == MapEncoding::Compressed16 && file.find("CMAP").has_value();
    const bool want_umap = preferred == MapEncoding::Uncompressed && file.find("UMAP").has_value();
    if (const auto dmap = file.find("DMAP"); dmap && !want_cmap && !want_umap) {
        map.encoding_ = MapEncoding::Compressed32;
        decoded = decode_compressed(dmap->data, true, map.blocks_, map.grid_);
    } else if (const auto cmap = file.find("CMAP"); cmap && !want_umap) {
        map.encoding_ = MapEncoding::Compressed16;
        decoded = decode_compressed(cmap->data, false, map.blocks_, map.grid_);
    } else if (const auto umap = file.find("UMAP")) {
        map.encoding_ = MapEncoding::Uncompressed;
        decoded = decode_uncompressed(umap->data, map.blocks_, map.grid_);
    }
    if (!decoded) {
        return decoded.error();
    }

    if (const auto zone = file.find("ZONE")) {
        auto zones = decode_zones(zone->data);
        if (!zones) {
            return zones.error();
        }
        map.zones_ = std::move(zones.value());
    }
    return map;
}

} // namespace gta20::assets
