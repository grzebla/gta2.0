#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>

#include "g2n/assets/chunk_file.hpp"
#include "g2n/assets/file_io.hpp"
#include "g2n/assets/gmp_map.hpp"
#include "g2n/assets/gmp_writer.hpp"

using namespace g2n;
using namespace g2n::assets;

namespace {
std::vector<std::byte> bytes(std::initializer_list<int> v) {
    std::vector<std::byte> out;
    for (int b : v)
        out.push_back(static_cast<std::byte>(b));
    return out;
}
} // namespace

TEST_CASE("ChunkFile: header and chunks (SYNTHETIC bytes)") {
    const auto data =
        bytes({'T', 'E', 'S', 'T', 7, 0, 'A', 'A', 'A', 'A', 2, 0, 0, 0, 9, 8, 'B', 'B', 'B', 'B', 0, 0, 0, 0});
    auto file = parse_chunk_file(data);
    REQUIRE(file);
    CHECK(file.value().file_type == "TEST");
    CHECK(file.value().version == 7);
    REQUIRE(file.value().chunks.size() == 2);
    CHECK(file.value().find("AAAA")->data.size() == 2);
    CHECK(file.value().find("BBBB")->data.empty());
    CHECK_FALSE(file.value().find("ZZZZ").has_value());
}

TEST_CASE("ChunkFile: truncated input is an error, not a crash") {
    CHECK_FALSE(parse_chunk_file(bytes({'G', 'B'})));
    CHECK_FALSE(parse_chunk_file(bytes({'G', 'B', 'M', 'P', 0xF4, 1, 'D', 'M', 'A', 'P', 10, 0, 0, 0, 1})));
}

TEST_CASE("Face words decode per the GTA2 map format document") {
    // tile 1023, wall, bullet wall, flat, flip, rotation 270
    const SideFace side = SideFace::decode(0xFFFF);
    CHECK(side.tile == 1023);
    CHECK(side.wall);
    CHECK(side.bullet_wall);
    CHECK(side.flat);
    CHECK(side.flip);
    CHECK(side.rotation == Rotation::R270);
    const LidFace lid = LidFace::decode(static_cast<u16>(5 | (2 << 10) | (1 << 14)));
    CHECK(lid.tile == 5);
    CHECK(lid.lighting == 2);
    CHECK(lid.rotation == Rotation::R90);
    BlockInfo b;
    b.slope_type = static_cast<u8>((41 << 2) | 2);
    CHECK(b.ground() == GroundType::Pavement);
    CHECK(b.slope() == 41);
}

TEST_CASE("GMP: DMAP writer/parser round trip (SYNTHETIC map)") {
    GmpWriter w;
    const BlockInfo road{0, 0, 0, 0, 7, 0x11, static_cast<u8>(GroundType::Road)};
    const BlockInfo wall{0x401, 0x402, 0x403, 0x404, 9, 0, 0};
    w.set_block(10, 20, 0, road);
    w.set_block(10, 20, 3, wall); // gap at z = 1, 2
    w.set_block(255, 255, 7, road);
    w.add_zone({3, 1, 2, 3, 4, "TESTZONE"});
    const auto data = w.build_dmap();

    auto map = parse_gmp(data);
    REQUIRE_MESSAGE(map, (map ? "" : map.error().message));
    const GmpMap& m = map.value();
    CHECK(m.encoding() == MapEncoding::Compressed32);
    CHECK(m.block_at(10, 20, 0) == road);
    CHECK(m.block_at(10, 20, 1).empty());
    CHECK(m.block_at(10, 20, 3) == wall);
    CHECK(m.block_at(255, 255, 7) == road);
    CHECK(m.block_at(0, 0, 0).empty());
    CHECK(m.top_level(10, 20) == 3);
    CHECK(m.top_level(0, 0) == -1);
    CHECK(m.unique_blocks().size() == 3);
    REQUIRE(m.zones().size() == 1);
    CHECK(m.zones()[0].name == "TESTZONE");
    CHECK(m.zones()[0].h == 4);
    CHECK(m.block_at(-1, 0, 0).empty()); // out of range is air
}

TEST_CASE("GMP: rejects wrong type, wrong version and corrupt columns") {
    GmpWriter w;
    w.set_block(1, 1, 1, BlockInfo{0, 0, 0, 0, 1, 0, 1});
    auto data = w.build_dmap();

    auto wrong_type = data;
    wrong_type[0] = std::byte{'X'};
    CHECK_FALSE(parse_gmp(wrong_type));

    auto wrong_version = data;
    wrong_version[4] = std::byte{0x01};
    CHECK_FALSE(parse_gmp(wrong_version));

    // Point column (0,0) beyond the column data.
    auto bad_base = data;
    const std::size_t base0 = 6 + 8; // header + DMAP chunk header
    bad_base[base0] = std::byte{0xFF};
    bad_base[base0 + 1] = std::byte{0xFF};
    bad_base[base0 + 2] = std::byte{0xFF};
    CHECK_FALSE(parse_gmp(bad_base));

    auto truncated = data;
    truncated.resize(truncated.size() - 5);
    CHECK_FALSE(parse_gmp(truncated));
}

TEST_CASE("GMP: synthetic test map parses and has roads, fields and buildings") {
    auto map = parse_gmp(build_synthetic_test_map());
    REQUIRE(map);
    const auto& m = map.value();
    CHECK(m.block_at(97, 97, 1).ground() == GroundType::Road);
    CHECK(m.block_at(103, 103, 2).left_face().wall);
    CHECK(m.zones()[0].name == "SYNTHETIC");
}

TEST_CASE("[gamedata] original GTA 2 map parses (needs G2N_GAME_DIR)") {
    const char* dir = std::getenv("G2N_GAME_DIR");
    if (dir == nullptr) {
        MESSAGE("G2N_GAME_DIR not set - skipping original-data test");
        return;
    }
    for (const char* level : {"data/wil.gmp", "data/ste.gmp", "data/bil.gmp"}) {
        auto path = resolve_game_path(dir, level);
        REQUIRE_MESSAGE(path, level);
        auto file = read_file(path.value());
        REQUIRE(file);
        auto map = parse_gmp(file.value());
        REQUIRE_MESSAGE(map, (map ? "" : map.error().message));
        CHECK(map.value().unique_blocks().size() > 1);
    }
}
