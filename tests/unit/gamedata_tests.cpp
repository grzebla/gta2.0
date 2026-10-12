// Tests against ORIGINAL GTA 2 data. They need GTA20_GAME_DIR (a legally owned installation) and
// skip themselves otherwise - CI never has game data. Test names start with "[gamedata]".
// Facts asserted here were measured on the PC 9.6 release (Rockstar 2004) and 10.5 (Freeloader);
// their map and style files are byte-identical.
#include <doctest/doctest.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "gta20/assets/file_io.hpp"
#include "gta20/assets/game_install.hpp"
#include "gta20/assets/gmp_map.hpp"
#include "gta20/assets/sty_style.hpp"
#include "gta20/scene/draw_list.hpp"

using namespace gta20;
using namespace gta20::assets;

namespace {

std::optional<std::filesystem::path> game_dir() {
    auto dir = game_dir_from_environment();
    if (dir.empty()) {
        MESSAGE("GTA20_GAME_DIR not set - skipping original-data test");
        return std::nullopt;
    }
    return dir;
}

std::vector<std::byte> load(const std::filesystem::path& root, const std::string& relative) {
    auto path = resolve_game_path(root, relative);
    REQUIRE_MESSAGE(path, relative);
    auto bytes = read_file(path.value());
    REQUIRE(bytes);
    return std::move(bytes.value());
}

std::vector<std::string> data_files(const std::filesystem::path& root, const std::string& extension) {
    std::vector<std::string> out;
    auto data = resolve_game_path(root, "data");
    REQUIRE(data);
    for (const auto& e : std::filesystem::directory_iterator(data.value())) {
        auto ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
        if (ext == extension) {
            out.push_back("data/" + e.path().filename().string());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

bool solid(const BlockInfo& b) {
    return b.lid != 0 || b.left_face().wall || b.right_face().wall || b.top_face().wall || b.bottom_face().wall;
}

} // namespace

TEST_CASE("[gamedata] installation is recognised") {
    const auto dir = game_dir();
    if (!dir)
        return;
    auto install = inspect_game_dir(*dir);
    REQUIRE_MESSAGE(install, (install ? "" : install.error().message));
    REQUIRE(install.value().exe_version.has_value());
    const auto& v = *install.value().exe_version;
    MESSAGE("gta2.exe version " << v);
    CHECK((v == "9.6.0.0" || v == "10.5.0.0"));
    CHECK(install.value().levels == std::vector<std::string>{"data/wil.gmp", "data/ste.gmp", "data/bil.gmp"});
}

TEST_CASE("[gamedata] every original map parses with sane ranges") {
    const auto dir = game_dir();
    if (!dir)
        return;
    const auto maps = data_files(*dir, ".gmp");
    CHECK(maps.size() == 17);
    for (const auto& name : maps) {
        CAPTURE(name);
        const auto bytes = load(*dir, name);
        auto map = parse_gmp(bytes);
        REQUIRE_MESSAGE(map, (map ? "" : map.error().message));
        const auto& m = map.value();
        CHECK(m.encoding() == MapEncoding::Compressed32);
        CHECK(m.unique_blocks().size() > 100);
        CHECK(m.unique_blocks()[0].empty()); // block 0 is always air (format document)
        // Measured facts (docs/formats/gmp.md): tiles 992-1022 are engine-reserved and 1023 marks
        // 3-sided diagonal slopes (49-52). The three single-player cities follow this for side faces;
        // bonus levels (mike1*) use a few reserved numbers, and a handful of 1023 lids sit on other
        // slope codes (0, 45-47).
        const bool city = name == "data/wil.gmp" || name == "data/ste.gmp" || name == "data/bil.gmp";
        int lid_markers = 0, lid_markers_on_diagonals = 0;
        for (const auto& b : m.unique_blocks()) {
            for (const u16 face : {b.left, b.right, b.top, b.bottom}) {
                if (city) {
                    REQUIRE((face & 0x3FF) < 992);
                }
            }
            if ((b.lid & 0x3FF) == 1023) {
                ++lid_markers;
                lid_markers_on_diagonals += (b.slope() >= 49 && b.slope() <= 52) ? 1 : 0;
            }
        }
        if (city && lid_markers > 0) {
            CHECK(static_cast<double>(lid_markers_on_diagonals) / lid_markers >= 0.95);
        }
        CHECK_FALSE(m.zones().empty());
        for (const auto& z : m.zones()) {
            CHECK(static_cast<int>(z.x) + z.w <= kMapWidth);
            CHECK(static_cast<int>(z.y) + z.h <= kMapHeight);
            CHECK(std::all_of(z.name.begin(), z.name.end(), [](unsigned char c) { return c >= 0x20 && c < 0x7F; }));
        }
    }
}

TEST_CASE("[gamedata] original maps carry a legacy CMAP that differs from DMAP") {
    // Measured: every PC map has both chunks, but CMAP holds an older/different version of the
    // city (fewer blocks, different lids). The format document says PC maps use DMAP, so the
    // engine prefers DMAP; CMAP must still decode cleanly.
    const auto dir = game_dir();
    if (!dir)
        return;
    for (const char* name : {"data/wil.gmp", "data/mike1m.gmp"}) {
        CAPTURE(name);
        const auto bytes = load(*dir, name);
        auto d = parse_gmp(bytes);
        auto c = parse_gmp(bytes, MapEncoding::Compressed16);
        REQUIRE(d);
        REQUIRE_MESSAGE(c, (c ? "" : c.error().message));
        CHECK(d.value().encoding() == MapEncoding::Compressed32);
        CHECK(c.value().encoding() == MapEncoding::Compressed16);
        CHECK(d.value().unique_blocks().size() > c.value().unique_blocks().size());
    }
}

TEST_CASE("[gamedata] map axes: walls face empty cells in the documented direction") {
    // A wall on the left face of block (x, y) should usually border open space at x-1, not y-1.
    // Measured on wil.gmp: ~84-86 % in the documented direction vs ~20 % with swapped axes.
    const auto dir = game_dir();
    if (!dir)
        return;
    auto map = parse_gmp(load(*dir, "data/wil.gmp"));
    REQUIRE(map);
    const auto& m = map.value();
    struct Dir {
        int face, dx, dy, sx, sy;
    };
    const Dir dirs[] = {{0, -1, 0, 0, -1}, {1, 1, 0, 0, 1}, {2, 0, -1, -1, 0}, {3, 0, 1, 1, 0}};
    for (const auto& d : dirs) {
        int walls = 0, open_documented = 0, open_swapped = 0;
        for (int z = 0; z < kMapLevels; ++z) {
            for (int y = 1; y < kMapHeight - 1; ++y) {
                for (int x = 1; x < kMapWidth - 1; ++x) {
                    const auto& b = m.block_at(x, y, z);
                    const u16 faces[] = {b.left, b.right, b.top, b.bottom};
                    const u16 f = faces[d.face];
                    if ((f & 0x400) == 0 || (f & 0x3FF) == 0)
                        continue;
                    ++walls;
                    open_documented += solid(m.block_at(x + d.dx, y + d.dy, z)) ? 0 : 1;
                    open_swapped += solid(m.block_at(x + d.sx, y + d.sy, z)) ? 0 : 1;
                }
            }
        }
        REQUIRE(walls > 1000);
        CHECK(static_cast<double>(open_documented) / walls > 0.75);
        CHECK(static_cast<double>(open_swapped) / walls < 0.35);
    }
}

TEST_CASE("[gamedata] every original style parses and decodes") {
    const auto dir = game_dir();
    if (!dir)
        return;
    const auto styles = data_files(*dir, ".sty");
    CHECK(styles.size() == 4);
    for (const auto& name : styles) {
        CAPTURE(name);
        auto style = parse_sty(load(*dir, name));
        REQUIRE_MESSAGE(style, (style ? "" : style.error().message));
        const auto& s = style.value();
        CHECK(s.version() == 700);
        const bool city = name != "data/fstyle.sty";
        CHECK(s.tile_count() == (city ? 992U : 0U));
        CHECK(s.physical_palette_count() == (city ? 320U : 64U));
        CHECK(s.sprite_count() == (city ? 2089U : 1766U));
        std::vector<u8> rgba;
        int w = 0, h = 0;
        for (u16 i = 0; i < s.sprite_count(); ++i) {
            REQUIRE(s.decode_sprite(i, rgba, w, h));
            REQUIRE(w > 0);
            REQUIRE(h > 0);
        }
        if (city) {
            std::vector<u8> tile(64 * 64 * 4);
            std::size_t transparent = 0, opaque = 0;
            for (u16 t = 0; t < s.tile_count(); ++t) {
                s.decode_tile(t, tile);
                for (std::size_t p = 3; p < tile.size(); p += 4) {
                    (tile[p] == 0 ? transparent : opaque)++;
                }
            }
            CHECK(opaque > transparent); // most tile pixels are solid; colour 0 marks transparency
            CHECK(transparent > 0);
        }
    }
}

TEST_CASE("[gamedata] every tile a level map uses exists in its style") {
    const auto dir = game_dir();
    if (!dir)
        return;
    for (const char* level : {"wil", "ste", "bil"}) {
        CAPTURE(level);
        auto map = parse_gmp(load(*dir, std::string("data/") + level + ".gmp"));
        auto style = parse_sty(load(*dir, std::string("data/") + level + ".sty"));
        REQUIRE(map);
        REQUIRE(style);
        for (const auto& b : map.value().unique_blocks()) {
            for (const u16 face : {b.left, b.right, b.top, b.bottom, b.lid}) {
                const u16 tile = face & 0x3FF;
                REQUIRE((tile < style.value().tile_count() || tile == 1023)); // single-player cities
            }
        }
        const auto atlas = scene::build_tile_atlas(style.value());
        CHECK(atlas.tile_count == 992);
    }
}

TEST_CASE("[gamedata] corrupted and truncated original files are rejected without crashing") {
    const auto dir = game_dir();
    if (!dir)
        return;
    const auto map = load(*dir, "data/wil.gmp");
    for (std::size_t cut : {std::size_t{5}, std::size_t{6}, std::size_t{100}, map.size() / 3, map.size() - 1}) {
        CAPTURE(cut);
        const std::vector<std::byte> truncated(map.begin(), map.begin() + static_cast<std::ptrdiff_t>(cut));
        CHECK_FALSE(parse_gmp(truncated));
    }
    // Point the first DMAP column far outside the column data.
    auto corrupt = map;
    constexpr std::string_view kDmap = "DMAP";
    const auto dmap = std::search(corrupt.begin(), corrupt.end(), kDmap.begin(), kDmap.end(),
                                  [](std::byte a, char b) { return a == static_cast<std::byte>(b); });
    REQUIRE(dmap != corrupt.end());
    const auto base0 = (dmap - corrupt.begin()) + 8;
    for (int i = 0; i < 4; ++i)
        corrupt[static_cast<std::size_t>(base0 + i)] = std::byte{0xFF};
    CHECK_FALSE(parse_gmp(corrupt));

    const auto sty = load(*dir, "data/wil.sty");
    const std::vector<std::byte> half(sty.begin(), sty.begin() + static_cast<std::ptrdiff_t>(sty.size() / 2));
    CHECK_FALSE(parse_sty(half));
    auto bad_palx = sty;
    constexpr std::string_view kPalx = "PALX";
    const auto palx = std::search(bad_palx.begin(), bad_palx.end(), kPalx.begin(), kPalx.end(),
                                  [](std::byte a, char b) { return a == static_cast<std::byte>(b); });
    REQUIRE(palx != bad_palx.end());
    const auto entry0 = (palx - bad_palx.begin()) + 8;
    bad_palx[static_cast<std::size_t>(entry0)] = std::byte{0xFF}; // physical palette 0x..FF > 320
    bad_palx[static_cast<std::size_t>(entry0 + 1)] = std::byte{0x7F};
    CHECK_FALSE(parse_sty(bad_palx));
}

TEST_CASE("[gamedata] 9.6 and 10.5 share identical map and style data (needs GTA20_GAME_DIR_105)") {
    const auto dir = game_dir();
    const char* other = std::getenv("GTA20_GAME_DIR_105");
    if (other == nullptr) {
        other = std::getenv("G2N_GAME_DIR_105"); // pre-rename name
    }
    if (!dir || other == nullptr) {
        MESSAGE("GTA20_GAME_DIR_105 not set - skipping version comparison");
        return;
    }
    for (const char* ext : {".gmp", ".sty"}) {
        for (const auto& name : data_files(*dir, ext)) {
            CAPTURE(name);
            CHECK(load(*dir, name) == load(other, name));
        }
    }
}
