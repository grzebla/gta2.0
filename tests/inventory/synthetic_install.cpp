#include "synthetic_install.hpp"

#include <algorithm>
#include <set>
#include <string>

#include "gta20/assets/file_io.hpp"
#include "gta20/assets/gmp_writer.hpp"
#include "gta20/assets/map_inventory.hpp"

namespace gta20::testsupport {
namespace {

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

} // namespace

std::vector<std::byte> text_bytes(std::string_view s) {
    std::vector<std::byte> out(s.size());
    std::transform(s.begin(), s.end(), out.begin(), [](char c) { return static_cast<std::byte>(c); });
    return out;
}

const std::vector<std::byte>& synthetic_style() {
    static const std::vector<std::byte> bytes = [] {
        auto out = text_bytes("GBST");
        append_u16(out, 700);
        append_chunk(out, "PALX", 16384 * 2);
        append_chunk(out, "PPAL", 64 * 1024);
        append_chunk(out, "PALB", 16);
        return out;
    }();
    return bytes;
}

const std::vector<std::byte>& synthetic_map() {
    static const std::vector<std::byte> bytes = [] {
        using namespace gta20::assets;
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

Result<bool> write_synthetic_install(const std::filesystem::path& root) {
    std::error_code ec;
    std::filesystem::create_directories(root / "data", ec);
    if (ec) {
        return make_error("cannot create " + (root / "data").string() + ": " + ec.message());
    }
    std::string seq;
    std::set<std::string_view> styles;
    for (const auto& a : assets::original_map_styles()) {
        if (auto w = assets::write_file(root / a.map, synthetic_map()); !w) {
            return w;
        }
        styles.insert(a.style);
        const auto map = std::filesystem::path(a.map).filename().string();
        const auto style = std::filesystem::path(a.style).filename().string();
        if (a.source == "data/test1.seq") {
            seq += "MainOrBonus = MAIN\r\nGMPFile = " + map + "\r\nSTYFile = " + style + "\r\n\r\n";
        } else if (auto w = assets::write_file(root / a.source,
                                               text_bytes("[MapFiles]\r\nGMPFile = " + map + "\r\nSTYFile = " + style +
                                                          "\r\nPlayerCount = 2\r\n"));
                   !w) {
            return w;
        }
    }
    if (auto w = assets::write_file(root / "data/test1.seq", text_bytes(seq)); !w) {
        return w;
    }
    for (const auto style : styles) {
        if (auto w = assets::write_file(root / style, synthetic_style()); !w) {
            return w;
        }
    }
    return true;
}

} // namespace gta20::testsupport
