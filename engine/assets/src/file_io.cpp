#include "g2n/assets/file_io.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace g2n::assets {
namespace {

bool iequals(const std::string& a, const std::string& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
}

} // namespace

Result<std::vector<std::byte>> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return make_error("cannot open " + path.string());
    }
    in.seekg(0, std::ios::end);
    const auto size = in.tellg();
    if (size < 0) {
        return make_error("cannot determine size of " + path.string());
    }
    in.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!in) {
        return make_error("failed to read " + path.string());
    }
    return bytes;
}

Result<bool> write_file(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        return make_error("failed to write " + path.string());
    }
    return true;
}

Result<std::filesystem::path> resolve_game_path(const std::filesystem::path& game_dir,
                                                const std::filesystem::path& relative) {
    std::filesystem::path current = game_dir;
    for (const auto& part : relative) {
        const auto exact = current / part;
        std::error_code ec;
        if (std::filesystem::exists(exact, ec)) {
            current = exact;
            continue;
        }
        bool found = false;
        for (const auto& entry : std::filesystem::directory_iterator(current, ec)) {
            if (iequals(entry.path().filename().string(), part.string())) {
                current = entry.path();
                found = true;
                break;
            }
        }
        if (!found) {
            return make_error("not found: " + (game_dir / relative).string());
        }
    }
    return current;
}

} // namespace g2n::assets
