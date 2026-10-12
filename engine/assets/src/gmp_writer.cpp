#include "g2n/assets/gmp_writer.hpp"

#include <algorithm>

namespace g2n::assets {
namespace {

class ByteWriter {
public:
    void u8le(u8 v) { out_.push_back(static_cast<std::byte>(v)); }
    void u16le(u16 v) {
        u8le(static_cast<u8>(v & 0xFF));
        u8le(static_cast<u8>(v >> 8));
    }
    void u32le(u32 v) {
        u16le(static_cast<u16>(v & 0xFFFF));
        u16le(static_cast<u16>(v >> 16));
    }
    void ascii(std::string_view s) {
        for (const char c : s) {
            u8le(static_cast<u8>(c));
        }
    }
    void bytes(const std::vector<std::byte>& b) { out_.insert(out_.end(), b.begin(), b.end()); }
    std::vector<std::byte>& data() { return out_; }

private:
    std::vector<std::byte> out_;
};

void chunk(ByteWriter& file, std::string_view type, ByteWriter& payload) {
    file.ascii(type);
    file.u32le(static_cast<u32>(payload.data().size()));
    file.bytes(payload.data());
}

} // namespace

void GmpWriter::set_block(int x, int y, int z, const BlockInfo& block) {
    auto it = std::find(blocks_.begin(), blocks_.end(), block);
    u32 index = 0;
    if (it == blocks_.end()) {
        blocks_.push_back(block);
        index = static_cast<u32>(blocks_.size() - 1);
    } else {
        index = static_cast<u32>(it - blocks_.begin());
    }
    grid_[(static_cast<std::size_t>(z) * kMapHeight + static_cast<std::size_t>(y)) * kMapWidth +
          static_cast<std::size_t>(x)] = index;
}

std::vector<std::byte> GmpWriter::build_dmap() const {
    constexpr std::size_t columns = static_cast<std::size_t>(kMapWidth) * kMapHeight;
    std::vector<u32> base(columns);
    std::vector<u32> column_data;
    for (std::size_t c = 0; c < columns; ++c) {
        int offset = 0;
        int height = 0;
        for (int z = 0; z < kMapLevels; ++z) {
            if (grid_[static_cast<std::size_t>(z) * columns + c] != 0) {
                height = z + 1;
            }
        }
        while (offset < height && grid_[static_cast<std::size_t>(offset) * columns + c] == 0) {
            ++offset;
        }
        base[c] = static_cast<u32>(column_data.size());
        column_data.push_back(static_cast<u32>(height) |
                              (static_cast<u32>(offset) << 8)); // u8 height, u8 offset, u16 pad
        for (int z = offset; z < height; ++z) {
            column_data.push_back(grid_[static_cast<std::size_t>(z) * columns + c]);
        }
    }

    ByteWriter dmap;
    for (const u32 b : base) {
        dmap.u32le(b);
    }
    dmap.u32le(static_cast<u32>(column_data.size()));
    for (const u32 w : column_data) {
        dmap.u32le(w);
    }
    dmap.u32le(static_cast<u32>(blocks_.size()));
    for (const auto& b : blocks_) {
        dmap.u16le(b.left);
        dmap.u16le(b.right);
        dmap.u16le(b.top);
        dmap.u16le(b.bottom);
        dmap.u16le(b.lid);
        dmap.u8le(b.arrows);
        dmap.u8le(b.slope_type);
    }

    ByteWriter file;
    file.ascii("GBMP");
    file.u16le(kGmpVersion);
    chunk(file, "DMAP", dmap);
    if (!zones_.empty()) {
        ByteWriter zone;
        for (const auto& z : zones_) {
            zone.u8le(z.type);
            zone.u8le(z.x);
            zone.u8le(z.y);
            zone.u8le(z.w);
            zone.u8le(z.h);
            zone.u8le(static_cast<u8>(z.name.size()));
            zone.ascii(z.name);
        }
        chunk(file, "ZONE", zone);
    }
    return std::move(file.data());
}

std::vector<std::byte> build_synthetic_test_map() {
    // SYNTHETIC: a 64x64 block district in the middle of the map. Tile numbers are
    // arbitrary; only ground types, walls and lids matter for the prototype.
    GmpWriter w;
    constexpr int origin = 96;
    constexpr int size = 64;
    const BlockInfo road{0, 0, 0, 0, 1, 0, static_cast<u8>(GroundType::Road)};
    const BlockInfo pavement{0, 0, 0, 0, 2, 0, static_cast<u8>(GroundType::Pavement)};
    const BlockInfo field{0, 0, 0, 0, 3, 0, static_cast<u8>(GroundType::Field)};
    constexpr u16 wall_face = 4 | 0x400 | 0x800; // tile 4, wall, bullet wall
    const BlockInfo building{wall_face, wall_face, wall_face, wall_face, 5, 0, static_cast<u8>(GroundType::Air)};
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool road_x = x % 16 < 3;
            const bool road_y = y % 16 < 3;
            const bool pave = !road_x && !road_y && (x % 16 == 3 || x % 16 == 15 || y % 16 == 3 || y % 16 == 15);
            const BlockInfo& ground = (road_x || road_y) ? road : pave ? pavement : field;
            w.set_block(origin + x, origin + y, 1, ground);
            const bool in_block = !road_x && !road_y && !pave;
            if (in_block && (x / 16 + y / 16) % 2 == 0 && x % 16 > 5 && x % 16 < 13 && y % 16 > 5 && y % 16 < 13) {
                w.set_block(origin + x, origin + y, 2, building);
            }
        }
    }
    w.add_zone({1, origin, origin, size, size, "SYNTHETIC"});
    return w.build_dmap();
}

} // namespace g2n::assets
