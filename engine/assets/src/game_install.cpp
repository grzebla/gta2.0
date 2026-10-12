#include "gta20/assets/game_install.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>

#include "gta20/assets/file_io.hpp"
#include "gta20/core/byte_reader.hpp"

namespace gta20::assets {

std::optional<std::string> pe_file_version(std::span<const std::byte> exe) {
    // VS_FIXEDFILEINFO starts with the signature 0xFEEF04BD, followed by the struct
    // version and the file version as two dwords (MS, LS).
    constexpr std::array<std::byte, 4> signature = {std::byte{0xBD}, std::byte{0x04}, std::byte{0xEF}, std::byte{0xFE}};
    const auto it = std::search(exe.begin(), exe.end(), signature.begin(), signature.end());
    if (it == exe.end()) {
        return std::nullopt;
    }
    ByteReader r(exe.subspan(static_cast<std::size_t>(it - exe.begin())));
    (void)r.u32le(); // signature
    (void)r.u32le(); // struct version
    const u32 ms = r.u32le();
    const u32 ls = r.u32le();
    if (r.failed()) {
        return std::nullopt;
    }
    return std::to_string(ms >> 16) + "." + std::to_string(ms & 0xFFFF) + "." + std::to_string(ls >> 16) + "." +
           std::to_string(ls & 0xFFFF);
}

Result<GameInstall> inspect_game_dir(const std::filesystem::path& root) {
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec)) {
        return make_error("game directory not found: " + root.string());
    }
    GameInstall install;
    install.root = root;
    if (auto exe = resolve_game_path(root, "gta2.exe")) {
        if (auto bytes = read_file(exe.value())) {
            install.exe_version = pe_file_version(bytes.value());
        }
    }
    auto data = resolve_game_path(root, "data");
    if (!data) {
        return make_error("no data/ directory in " + root.string());
    }
    for (const char* level : {"wil", "ste", "bil"}) {
        if (resolve_game_path(root, std::string("data/") + level + ".gmp")) {
            install.levels.push_back(std::string("data/") + level + ".gmp");
        }
    }
    if (install.levels.empty()) {
        return make_error("no single-player maps (wil/ste/bil .gmp) in " + data.value().string());
    }
    return install;
}

std::filesystem::path game_dir_from_environment() {
    const char* value = std::getenv("GTA20_GAME_DIR");
    if (value == nullptr || *value == '\0') {
        value = std::getenv("G2N_GAME_DIR"); // pre-rename name, still honoured
    }
    if (value == nullptr || *value == '\0') {
        return {};
    }
    std::string path = value;
    if (path.rfind("~/", 0) == 0) {
        const char* home = std::getenv("HOME");
        if (home == nullptr) {
            home = std::getenv("USERPROFILE");
        }
        if (home != nullptr) {
            path = std::string(home) + path.substr(1);
        }
    }
    return path;
}

} // namespace gta20::assets
