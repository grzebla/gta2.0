// Strict map inventory (gta20-headless inventory) against SYNTHETIC installations built in a
// temporary directory. No original GTA 2 data is used: maps come from GmpWriter, styles are
// minimal GBST containers and test1.seq / *.mmp are generated from the recorded associations.
#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "gta20/assets/file_io.hpp"
#include "gta20/assets/gmp_writer.hpp"
#include "gta20/assets/map_inventory.hpp"

using namespace gta20;
using namespace gta20::assets;

namespace {

std::vector<std::byte> text_bytes(std::string_view s) {
    std::vector<std::byte> out(s.size());
    std::transform(s.begin(), s.end(), out.begin(), [](char c) { return static_cast<std::byte>(c); });
    return out;
}

void append_u16(std::vector<std::byte>& out, u16 v) {
    out.push_back(static_cast<std::byte>(v & 0xFF));
    out.push_back(static_cast<std::byte>(v >> 8));
}

void append_chunk(std::vector<std::byte>& out, std::string_view type, std::size_t size) {
    for (const char c : type)
        out.push_back(static_cast<std::byte>(c));
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<std::byte>((size >> (8 * i)) & 0xFF));
    out.insert(out.end(), size, std::byte{0});
}

/// SYNTHETIC style: the smallest GBST container parse_sty accepts (palettes only, no tiles).
std::vector<std::byte> synthetic_style() {
    auto out = text_bytes("GBST");
    append_u16(out, 700);
    append_chunk(out, "PALX", 16384 * 2);
    append_chunk(out, "PPAL", 64 * 1024);
    append_chunk(out, "PALB", 16);
    return out;
}

/// SYNTHETIC map with known aggregate counts (see the passing-fixture test).
const std::vector<std::byte>& synthetic_map() {
    static const std::vector<std::byte> bytes = [] {
        GmpWriter w;
        const BlockInfo road{0, 0, 0, 0, 7, 0, static_cast<u8>(GroundType::Road)};
        const BlockInfo diagonal{0, 0, 0, 0, 1023, 0, static_cast<u8>((49 << 2) | 1)};
        const BlockInfo reserved{0x400 | 995, 0, 0, 0, 996, 0, static_cast<u8>(GroundType::Pavement)};
        w.set_block(1, 1, 0, road);
        w.set_block(2, 1, 0, road);
        w.set_block(3, 3, 2, diagonal);
        w.set_block(4, 4, 5, reserved);
        return w.build_dmap();
    }();
    return bytes;
}

/// A temporary SYNTHETIC installation with all 17 maps, their styles and configuration files.
class Fixture {
public:
    Fixture() {
        std::random_device rd;
        root_ = std::filesystem::temp_directory_path() /
                ("gta20-inventory-" + std::to_string(rd()) + "-" + std::to_string(rd()));
        std::filesystem::create_directories(root_ / "data");
        std::string seq;
        std::set<std::string_view> styles;
        for (const auto& a : original_map_styles()) {
            write(a.map, synthetic_map());
            styles.insert(a.style);
            const auto map = std::filesystem::path(a.map).filename().string();
            const auto style = std::filesystem::path(a.style).filename().string();
            if (a.source == "data/test1.seq") {
                seq += "MainOrBonus = MAIN\r\nGMPFile = " + map + "\r\nSTYFile = " + style + "\r\n\r\n";
            } else {
                write(a.source, text_bytes("[MapFiles]\r\nGMPFile = " + map + "\r\nSTYFile = " + style +
                                           "\r\nPlayerCount = 2\r\n"));
            }
        }
        write("data/test1.seq", text_bytes(seq));
        for (const auto style : styles)
            write(style, synthetic_style());
    }
    ~Fixture() {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }
    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }
    void write(std::string_view relative, const std::vector<std::byte>& bytes) const {
        REQUIRE(write_file(root_ / relative, bytes));
    }
    void remove(std::string_view relative) const { REQUIRE(std::filesystem::remove(root_ / relative)); }

private:
    std::filesystem::path root_;
};

bool mentions(const MapInventory& inv, std::string_view text) {
    return std::any_of(inv.problems.begin(), inv.problems.end(),
                       [&](const std::string& p) { return p.find(text) != std::string::npos; });
}

std::string problems_of(const MapInventory& inv) {
    std::string all;
    for (const auto& p : inv.problems)
        all += p + "\n";
    return all;
}

} // namespace

TEST_CASE("Map styles: all 17 original maps have a recorded style and source") {
    const auto styles = original_map_styles();
    CHECK(styles.size() == 17);
    std::set<std::string> maps;
    for (const auto& a : styles) {
        CAPTURE(a.map);
        CHECK(a.resolved());
        CHECK_FALSE(a.source.empty());
        maps.insert(std::string(a.map));
    }
    CHECK(maps.size() == 17);
    REQUIRE(find_map_style("data/MP1-COMP.gmp") != nullptr);
    CHECK(find_map_style("data/MP1-COMP.gmp")->style == "data/bil.sty");
    CHECK(find_map_style("/elsewhere/lorne2e.gmp")->style == "data/wil.sty");
    CHECK(find_map_style("data/ste.gmp")->style == "data/ste.sty");
    CHECK(find_map_style("data/synthetic.gmp") == nullptr);
}

TEST_CASE("Strict map inventory: missing input fails") {
    SUBCASE("no game directory (GTA20_GAME_DIR unset)") {
        const auto inv = take_map_inventory({});
        CHECK_FALSE(inv.passed());
        CHECK(mentions(inv, "GTA20_GAME_DIR"));
    }
    SUBCASE("directory does not exist") {
        const auto inv = take_map_inventory(std::filesystem::temp_directory_path() / "gta20-inventory-does-not-exist");
        CHECK_FALSE(inv.passed());
        CHECK(mentions(inv, "game directory not found"));
    }
    SUBCASE("no data directory") {
        Fixture f;
        std::filesystem::remove_all(f.root() / "data");
        const auto inv = take_map_inventory(f.root());
        CHECK_FALSE(inv.passed());
        CHECK(mentions(inv, "no data/ directory"));
    }
    SUBCASE("empty data directory") {
        Fixture f;
        std::filesystem::remove_all(f.root() / "data");
        std::filesystem::create_directories(f.root() / "data");
        const auto inv = take_map_inventory(f.root());
        CHECK_FALSE(inv.passed());
        CHECK(mentions(inv, "found 0 maps"));
        CHECK(mentions(inv, "missing map: data/wil.gmp"));
        CHECK(mentions(inv, "missing style: data/wil.sty"));
        CHECK(mentions(inv, "missing source data/test1.seq"));
        CHECK(inv.maps.empty());
    }
}

TEST_CASE("Strict map inventory: SYNTHETIC fixture with all 17 maps passes") {
    Fixture f;
    const auto inv = take_map_inventory(f.root());
    REQUIRE_MESSAGE(inv.passed(), problems_of(inv));
    REQUIRE(inv.maps.size() == 17);
    CHECK(inv.styles.size() == 3);
    for (const auto& m : inv.maps) {
        CAPTURE(m.map);
        CHECK(m.occupied_cells == 4);
        CHECK(m.unique_blocks == 4);
        CHECK(m.min_level == 0);
        CHECK(m.max_level == 5);
        CHECK(m.slope_cells[0] == 3);
        CHECK(m.slope_cells[49] == 1);
        CHECK(m.reserved_side_faces == 1);
        CHECK(m.reserved_lids == 1);
        CHECK(m.marker_side_faces == 0);
        CHECK(m.marker_lids == 1);
    }
    CHECK(inv.maps[13].map == "data/bil-multi.gmp");
    CHECK(inv.maps[13].style == "data/bil.sty");
    CHECK(inv.maps[13].source == "data/Industrial-2P.mmp");

    const auto report = format_map_inventory(inv);
    CHECK(report.find("maps: 17 inventoried") != std::string::npos);
    CHECK(report.find("result: PASS") != std::string::npos);
    CHECK(report.find("map data/MP5-comp.gmp: style data/ste.sty (source data/mp5-2p.mmp)") != std::string::npos);
    CHECK(report.find("levels 0..5") != std::string::npos);
    CHECK(report.find("slope codes (cells): 0=3 49=1\n") != std::string::npos);
    CHECK(report.find("side 992-1022=1 lid 992-1022=1 side 1023=0 lid 1023=1") != std::string::npos);
    CHECK(report.size() < 17 * 400); // aggregates only - bounded, no records or pixels
}

TEST_CASE("Strict map inventory: map file names are matched case-insensitively") {
    Fixture f;
    std::filesystem::rename(f.root() / "data/MP1-comp.gmp", f.root() / "data/mp1-COMP.GMP");
    const auto inv = take_map_inventory(f.root());
    CHECK_MESSAGE(inv.passed(), problems_of(inv));
}

TEST_CASE("Strict map inventory: malformed maps fail") {
    Fixture f;
    SUBCASE("garbage bytes") {
        f.write("data/ste.gmp", text_bytes("this is not a map"));
        const auto inv = take_map_inventory(f.root());
        CHECK_FALSE(inv.passed());
        CHECK(mentions(inv, "data/ste.gmp: truncated chunk"));
        CHECK(inv.maps.size() == 16);
    }
    SUBCASE("style container instead of a map") {
        f.write("data/lorne2m.gmp", synthetic_style());
        const auto inv = take_map_inventory(f.root());
        CHECK_FALSE(inv.passed());
        CHECK(mentions(inv, "data/lorne2m.gmp: not a GTA 2 map"));
    }
    SUBCASE("empty file") {
        f.write("data/mike1m.gmp", {});
        const auto inv = take_map_inventory(f.root());
        CHECK_FALSE(inv.passed());
        CHECK(mentions(inv, "data/mike1m.gmp:"));
    }
    SUBCASE("truncated map") {
        const auto& full = synthetic_map();
        f.write("data/wil.gmp", std::vector<std::byte>(full.begin(), full.begin() + 1000));
        const auto inv = take_map_inventory(f.root());
        CHECK_FALSE(inv.passed());
        CHECK(mentions(inv, "data/wil.gmp:"));
    }
    SUBCASE("map without blocks") {
        f.write("data/bil.gmp", GmpWriter{}.build_dmap());
        const auto inv = take_map_inventory(f.root());
        CHECK_FALSE(inv.passed());
        CHECK(mentions(inv, "data/bil.gmp: map holds no blocks"));
    }
}

TEST_CASE("Strict map inventory: incorrect map count fails") {
    Fixture f;
    SUBCASE("16 maps") {
        f.remove("data/mike2h.gmp");
        const auto inv = take_map_inventory(f.root());
        CHECK_FALSE(inv.passed());
        CHECK(mentions(inv, "found 16 maps (.gmp) in data/, expected exactly 17"));
        CHECK(mentions(inv, "missing map: data/mike2h.gmp"));
    }
    SUBCASE("18 maps") {
        f.write("data/extra.gmp", synthetic_map());
        const auto inv = take_map_inventory(f.root());
        CHECK_FALSE(inv.passed());
        CHECK(mentions(inv, "found 18 maps"));
        CHECK(mentions(inv, "unexpected map: data/extra.gmp"));
    }
    SUBCASE("17 maps, one of them unexpected") {
        f.remove("data/wil.gmp");
        f.write("data/wil2.gmp", synthetic_map());
        const auto inv = take_map_inventory(f.root());
        CHECK_FALSE(inv.passed());
        CHECK_FALSE(mentions(inv, "found"));
        CHECK(mentions(inv, "missing map: data/wil.gmp"));
        CHECK(mentions(inv, "unexpected map: data/wil2.gmp"));
    }
}

TEST_CASE("Strict map inventory: missing or malformed styles fail") {
    Fixture f;
    SUBCASE("missing") {
        f.remove("data/bil.sty");
        const auto inv = take_map_inventory(f.root());
        CHECK_FALSE(inv.passed());
        CHECK(mentions(inv, "missing style: data/bil.sty"));
    }
    SUBCASE("malformed") {
        f.write("data/ste.sty", text_bytes("GBST"));
        const auto inv = take_map_inventory(f.root());
        CHECK_FALSE(inv.passed());
        CHECK(mentions(inv, "data/ste.sty:"));
    }
}

TEST_CASE("Strict map inventory: configuration files must declare the recorded pairs") {
    Fixture f;
    SUBCASE("source file missing") {
        f.remove("data/test1.seq");
        const auto inv = take_map_inventory(f.root());
        CHECK_FALSE(inv.passed());
        CHECK(mentions(inv, "missing source data/test1.seq for data/wil.gmp"));
    }
    SUBCASE("source names a different style") {
        f.write("data/mp2-2p.mmp", text_bytes("[MapFiles]\r\nGMPFile = MP2-comp.gmp\r\nSTYFile = ste.sty\r\n"));
        const auto inv = take_map_inventory(f.root());
        CHECK_FALSE(inv.passed());
        CHECK(mentions(inv, "pairs mp2-comp.gmp with ste.sty, recorded style is wil.sty"));
        CHECK(mentions(inv, "data/mp2-2p.mmp does not declare data/MP2-comp.gmp with data/wil.sty"));
    }
    SUBCASE("another configuration file contradicts the record") {
        f.write("data/res-2p.mmp", text_bytes("GMPFile = ste.gmp\nSTYFile = bil.sty\n"));
        const auto inv = take_map_inventory(f.root());
        CHECK_FALSE(inv.passed());
        CHECK(mentions(inv, "data/res-2p.mmp pairs ste.gmp with bil.sty"));
    }
    SUBCASE("map without a style") {
        f.write("data/res-3p.mmp", text_bytes("GMPFile = ste.gmp\n"));
        const auto inv = take_map_inventory(f.root());
        CHECK_FALSE(inv.passed());
        CHECK(mentions(inv, "data/res-3p.mmp: map ste.gmp has no STYFile"));
    }
    SUBCASE("configuration names a map that is not expected") {
        f.write("data/res-4p.mmp", text_bytes("GMPFile = nowhere.gmp\nSTYFile = ste.sty\n"));
        const auto inv = take_map_inventory(f.root());
        CHECK_FALSE(inv.passed());
        CHECK(mentions(inv, "declares map nowhere.gmp, which is not an expected map"));
    }
}

TEST_CASE("Strict map inventory: an unresolved association fails") {
    Fixture f;
    std::vector<MapStyleAssociation> expected(original_map_styles().begin(), original_map_styles().end());
    expected[0].style = {};
    expected[0].source = "SYNTHETIC: unknown";
    const auto inv = take_map_inventory(f.root(), expected);
    CHECK_FALSE(inv.passed());
    CHECK(mentions(inv, "unresolved style for data/wil.gmp (SYNTHETIC: unknown)"));
    CHECK(format_map_inventory(inv).find("style UNRESOLVED") != std::string::npos);
}
