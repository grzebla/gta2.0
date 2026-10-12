#include "gta20/assets/chunk_file.hpp"

#include "gta20/core/byte_reader.hpp"

namespace gta20::assets {

std::optional<Chunk> ChunkFile::find(std::string_view type) const {
    for (const auto& chunk : chunks) {
        if (chunk.type == type) {
            return chunk;
        }
    }
    return std::nullopt;
}

Result<ChunkFile> parse_chunk_file(std::span<const std::byte> bytes) {
    ByteReader reader(bytes);
    ChunkFile file;
    file.file_type = reader.fixed_string(4);
    file.version = reader.u16le();
    if (reader.failed()) {
        return make_error("file too short for a chunk-file header (6 bytes)");
    }

    while (reader.remaining() > 0) {
        const std::size_t at = reader.position();
        Chunk chunk;
        chunk.type = reader.fixed_string(4);
        const u32 size = reader.u32le();
        chunk.data = reader.take(size);
        if (reader.failed()) {
            return make_error("truncated chunk header or payload at offset " + std::to_string(at));
        }
        file.chunks.push_back(std::move(chunk));
    }
    return file;
}

} // namespace gta20::assets
