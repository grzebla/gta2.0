#pragma once

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "g2n/core/result.hpp"
#include "g2n/core/types.hpp"

namespace g2n::assets {

/// One chunk of a GTA 2 data file: 4-character type + payload (header excluded).
struct Chunk {
    std::string type;
    std::span<const std::byte> data;
};

/// Container used by GTA 2 data files (map "GBMP", style "GBST", ...):
/// char[4] file type, u16 version, then chunks of char[4] type + u32 size + payload.
/// Views into the caller's buffer, which must outlive the ChunkFile.
struct ChunkFile {
    std::string file_type;
    u16 version = 0;
    std::vector<Chunk> chunks;

    /// First chunk of the given type (chunks may appear in any order).
    [[nodiscard]] std::optional<Chunk> find(std::string_view type) const;
};

[[nodiscard]] Result<ChunkFile> parse_chunk_file(std::span<const std::byte> bytes);

} // namespace g2n::assets
