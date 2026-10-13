#include "gta20/assets/map_inventory.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>

#include "gta20/assets/file_io.hpp"
#include "gta20/assets/gmp_map.hpp"
#include "gta20/assets/sty_style.hpp"

namespace gta20::assets {
namespace {

// Measured on the PC 9.6 release (byte-identical in 10.5): data/test1.seq declares the twelve
// single-player maps (MainOrBonus/GMPFile/STYFile records), and every *.mmp declares one
// multiplayer map; all 2p-6p variants of a map name the same style.
constexpr std::array<MapStyleAssociation, 17> kOriginalMapStyles = {{
    {"data/wil.gmp", "data/wil.sty", "data/test1.seq"},
    {"data/lorne2e.gmp", "data/wil.sty", "data/test1.seq"},
    {"data/lorne2m.gmp", "data/wil.sty", "data/test1.seq"},
    {"data/lorne2h.gmp", "data/wil.sty", "data/test1.seq"},
    {"data/ste.gmp", "data/ste.sty", "data/test1.seq"},
    {"data/mike1e.gmp", "data/ste.sty", "data/test1.seq"},
    {"data/mike1m.gmp", "data/ste.sty", "data/test1.seq"},
    {"data/mike1h.gmp", "data/ste.sty", "data/test1.seq"},
    {"data/bil.gmp", "data/bil.sty", "data/test1.seq"},
    {"data/mike2e.gmp", "data/bil.sty", "data/test1.seq"},
    {"data/mike2m.gmp", "data/bil.sty", "data/test1.seq"},
    {"data/mike2h.gmp", "data/bil.sty", "data/test1.seq"},
    {"data/wil-multi.gmp", "data/wil.sty", "data/downtown-2p.mmp"},
    {"data/bil-multi.gmp", "data/bil.sty", "data/Industrial-2P.mmp"},
    {"data/MP1-comp.gmp", "data/bil.sty", "data/mp1-2p.mmp"},
    {"data/MP2-comp.gmp", "data/wil.sty", "data/mp2-2p.mmp"},
    {"data/MP5-comp.gmp", "data/ste.sty", "data/mp5-2p.mmp"},
}};

constexpr u16 kFirstReservedTile = 992;
constexpr u16 kMarkerTile = 1023;
constexpr std::size_t kMaxConfigBytes = 64 * 1024;

std::string lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
    return out;
}

std::string_view file_name(std::string_view path) {
    const auto at = path.find_last_of("/\\");
    return at == std::string_view::npos ? path : path.substr(at + 1);
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

bool has_extension(const std::filesystem::path& p, std::string_view ext) {
    return lower(p.extension().string()) == ext;
}

/// Map -> style pairs (lower-case file names) declared by one GTA 2 configuration file
/// ("Key = Value" lines; a GMPFile line is paired with the next STYFile line).
std::vector<std::pair<std::string, std::string>> declared_pairs(std::string_view text, const std::string& source,
                                                                std::vector<std::string>& problems) {
    std::vector<std::pair<std::string, std::string>> pairs;
    std::string pending_map;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = std::min(text.find('\n', start), text.size());
        const auto line = trim(text.substr(start, end - start));
        start = end + 1;
        const auto eq = line.find('=');
        if (eq == std::string_view::npos) {
            continue;
        }
        const auto key = lower(trim(line.substr(0, eq)));
        const auto value = lower(file_name(trim(line.substr(eq + 1))));
        if (key == "gmpfile") {
            if (!pending_map.empty()) {
                problems.push_back(source + ": map " + pending_map + " has no STYFile");
            }
            pending_map = value;
        } else if (key == "styfile") {
            if (pending_map.empty()) {
                problems.push_back(source + ": STYFile " + value + " without a preceding GMPFile");
            } else {
                pairs.emplace_back(pending_map, value);
                pending_map.clear();
            }
        }
    }
    if (!pending_map.empty()) {
        problems.push_back(source + ": map " + pending_map + " has no STYFile");
    }
    return pairs;
}

void count_map(const GmpMap& m, MapInventoryEntry& e) {
    std::vector<u64> uses(m.unique_blocks().size(), 0);
    for (int z = 0; z < kMapLevels; ++z) {
        for (int y = 0; y < kMapHeight; ++y) {
            for (int x = 0; x < kMapWidth; ++x) {
                const u32 index = m.block_index_at(x, y, z);
                if (m.unique_blocks()[index].empty()) {
                    continue;
                }
                ++uses[index];
                e.min_level = e.min_level < 0 ? z : std::min(e.min_level, z);
                e.max_level = std::max(e.max_level, z);
            }
        }
    }
    for (std::size_t i = 0; i < uses.size(); ++i) {
        if (uses[i] == 0) {
            continue;
        }
        const auto& b = m.unique_blocks()[i];
        const auto n = static_cast<u32>(uses[i]);
        e.occupied_cells += n;
        e.slope_cells[b.slope()] += n;
        for (const u16 face : {b.left, b.right, b.top, b.bottom}) {
            const u16 tile = face & 0x3FF;
            e.reserved_side_faces += (tile >= kFirstReservedTile && tile < kMarkerTile) ? n : 0;
            e.marker_side_faces += tile == kMarkerTile ? n : 0;
        }
        const u16 lid = b.lid & 0x3FF;
        e.reserved_lids += (lid >= kFirstReservedTile && lid < kMarkerTile) ? n : 0;
        e.marker_lids += lid == kMarkerTile ? n : 0;
    }
}

} // namespace

std::span<const MapStyleAssociation> original_map_styles() {
    return kOriginalMapStyles;
}

const MapStyleAssociation* find_map_style(std::string_view map) {
    const auto name = lower(file_name(map));
    for (const auto& a : kOriginalMapStyles) {
        if (lower(file_name(a.map)) == name) {
            return &a;
        }
    }
    return nullptr;
}

MapInventory take_map_inventory(const std::filesystem::path& root, std::span<const MapStyleAssociation> expected) {
    MapInventory inv;
    inv.root = root;
    auto& problems = inv.problems;
    if (root.empty()) {
        problems.emplace_back("no game directory: set GTA20_GAME_DIR (or pass --game-dir) to an original GTA 2 "
                              "installation");
        return inv;
    }
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec)) {
        problems.push_back("game directory not found: " + root.string());
        return inv;
    }
    const auto data = resolve_game_path(root, "data");
    if (!data || !std::filesystem::is_directory(data.value(), ec)) {
        problems.push_back("no data/ directory in " + root.string());
        return inv;
    }

    // The set of maps must match exactly.
    std::map<std::string, std::filesystem::path> found; // lower-case "data/x.gmp" -> path
    std::vector<std::filesystem::path> configs;
    for (const auto& entry : std::filesystem::directory_iterator(data.value(), ec)) {
        if (!entry.is_regular_file(ec)) {
            continue;
        }
        if (has_extension(entry.path(), ".gmp")) {
            found.emplace("data/" + lower(entry.path().filename().string()), entry.path());
        } else if (has_extension(entry.path(), ".mmp") || has_extension(entry.path(), ".seq")) {
            configs.push_back(entry.path());
        }
    }
    std::set<std::string> wanted;
    for (const auto& a : expected) {
        wanted.insert(lower(a.map));
    }
    if (found.size() != expected.size()) {
        problems.push_back("found " + std::to_string(found.size()) + " maps (.gmp) in data/, expected exactly " +
                           std::to_string(expected.size()));
    }
    for (const auto& a : expected) {
        if (!found.contains(lower(a.map))) {
            problems.push_back("missing map: " + std::string(a.map));
        }
    }
    for (const auto& [name, path] : found) {
        if (!wanted.contains(name)) {
            problems.push_back("unexpected map: " + name);
        }
    }

    // Styles named by the associations must exist and parse.
    std::set<std::string> style_names;
    for (const auto& a : expected) {
        if (a.resolved()) {
            style_names.insert(std::string(a.style));
        } else {
            problems.push_back("unresolved style for " + std::string(a.map) + " (" + std::string(a.source) + ")");
        }
    }
    for (const auto& name : style_names) {
        const auto path = resolve_game_path(root, name);
        if (!path) {
            problems.push_back("missing style: " + name);
            continue;
        }
        const auto bytes = read_file(path.value());
        if (!bytes) {
            problems.push_back(name + ": " + bytes.error().message);
            continue;
        }
        const auto style = parse_sty(bytes.value());
        if (!style) {
            problems.push_back(name + ": " + style.error().message);
            continue;
        }
        inv.styles.push_back({name, style.value().version(), style.value().tile_count()});
    }

    // The game's configuration files must declare every recorded pair and contradict none.
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> declared; // source -> pairs
    for (const auto& path : configs) {
        const auto source = "data/" + path.filename().string();
        const auto bytes = read_file(path);
        if (!bytes) {
            problems.push_back(source + ": " + bytes.error().message);
            continue;
        }
        if (bytes.value().size() > kMaxConfigBytes) {
            problems.push_back(source + ": larger than " + std::to_string(kMaxConfigBytes) + " bytes");
            continue;
        }
        const std::string_view text(reinterpret_cast<const char*>(bytes.value().data()), bytes.value().size());
        declared[lower(source)] = declared_pairs(text, source, problems);
    }
    std::map<std::string, std::string> recorded; // map file name -> style file name (lower case)
    for (const auto& a : expected) {
        if (a.resolved()) {
            recorded[lower(file_name(a.map))] = lower(file_name(a.style));
        }
    }
    for (const auto& [source, pairs] : declared) {
        for (const auto& [map, style] : pairs) {
            const auto it = recorded.find(map);
            if (it == recorded.end()) {
                if (!wanted.contains("data/" + map)) {
                    problems.push_back(source + " declares map " + map + ", which is not an expected map");
                }
            } else if (it->second != style) {
                problems.push_back(source + " pairs " + map + " with " + style + ", recorded style is " + it->second);
            }
        }
    }
    for (const auto& a : expected) {
        if (!a.resolved()) {
            continue;
        }
        const auto it = declared.find(lower(a.source));
        if (it == declared.end()) {
            problems.push_back("missing source " + std::string(a.source) + " for " + std::string(a.map));
            continue;
        }
        const std::pair<std::string, std::string> pair{lower(file_name(a.map)), lower(file_name(a.style))};
        if (std::find(it->second.begin(), it->second.end(), pair) == it->second.end()) {
            problems.push_back(std::string(a.source) + " does not declare " + std::string(a.map) + " with " +
                               std::string(a.style));
        }
    }

    // Every expected map that is present must parse and hold blocks.
    for (const auto& a : expected) {
        const auto it = found.find(lower(a.map));
        if (it == found.end()) {
            continue;
        }
        MapInventoryEntry e;
        e.map = std::string(a.map);
        e.style = std::string(a.style);
        e.source = std::string(a.source);
        const auto bytes = read_file(it->second);
        if (!bytes) {
            problems.push_back(e.map + ": " + bytes.error().message);
            continue;
        }
        e.bytes = bytes.value().size();
        const auto map = parse_gmp(bytes.value());
        if (!map) {
            problems.push_back(e.map + ": " + map.error().message);
            continue;
        }
        e.unique_blocks = map.value().unique_blocks().size();
        count_map(map.value(), e);
        if (e.occupied_cells == 0) {
            problems.push_back(e.map + ": map holds no blocks");
        }
        inv.maps.push_back(std::move(e));
    }
    return inv;
}

std::string format_map_inventory(const MapInventory& inv) {
    std::ostringstream out;
    out << "GTA 2 strict map inventory: " << (inv.root.empty() ? std::string("(no game directory)") : inv.root.string())
        << "\n";
    for (const auto& s : inv.styles) {
        out << "style " << s.style << ": GBST " << s.version << ", " << s.tiles << " tiles\n";
    }
    for (const auto& m : inv.maps) {
        out << "map " << m.map << ": style " << (m.style.empty() ? std::string("UNRESOLVED") : m.style) << " (source "
            << m.source << "), " << m.bytes << " bytes, " << m.unique_blocks << " unique blocks, " << m.occupied_cells
            << " occupied cells, levels ";
        if (m.min_level < 0) {
            out << "none\n";
        } else {
            out << m.min_level << ".." << m.max_level << "\n";
        }
        out << "  slope codes (cells):";
        for (std::size_t code = 0; code < m.slope_cells.size(); ++code) {
            if (m.slope_cells[code] != 0) {
                out << " " << code << "=" << m.slope_cells[code];
            }
        }
        out << "\n  reserved faces (cells): side 992-1022=" << m.reserved_side_faces
            << " lid 992-1022=" << m.reserved_lids << " side 1023=" << m.marker_side_faces
            << " lid 1023=" << m.marker_lids << "\n";
    }
    out << "maps: " << inv.maps.size() << " inventoried\n";
    if (inv.passed()) {
        out << "result: PASS\n";
    } else {
        out << "problems:\n";
        for (const auto& p : inv.problems) {
            out << "  - " << p << "\n";
        }
        out << "result: FAIL (" << inv.problems.size() << " problems)\n";
    }
    return out.str();
}

} // namespace gta20::assets
