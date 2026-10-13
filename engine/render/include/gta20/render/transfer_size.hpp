#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "gta20/core/result.hpp"
#include "gta20/core/types.hpp"
#include "gta20/scene/draw_list.hpp"

namespace gta20::render {

// SDL_GPU sizes buffers, transfers and draw counts in Uint32, so every byte count the renderer
// passes to it is computed here in 64 bits and checked first. Nothing here allocates.

/// Largest byte count of one GPU buffer or transfer.
inline constexpr u32 kMaxTransferBytes = UINT32_MAX;
/// Largest instance count whose bytes fit one transfer.
inline constexpr u32 kMaxInstances = kMaxTransferBytes / sizeof(scene::QuadInstance);
/// Instance capacity of the first instance buffer.
inline constexpr u32 kInitialInstanceCapacity = 1024;

/// Bytes of a tightly packed RGBA8 image (atlas upload, offscreen readback). Rejects a zero
/// dimension and images over kMaxTransferBytes; `what` names the image in the error.
[[nodiscard]] Result<u32> rgba8_byte_size(u32 width, u32 height, std::string_view what);

/// Bytes of `count` quad instances. Rejects counts over kMaxInstances.
[[nodiscard]] Result<u32> instance_byte_size(std::size_t count);

/// Capacity for at least `count` instances: `current` (at least kInitialInstanceCapacity) doubled
/// until it fits, clamped to kMaxInstances. Rejects counts over kMaxInstances.
[[nodiscard]] Result<u32> grow_instance_capacity(u32 current, std::size_t count);

} // namespace gta20::render
