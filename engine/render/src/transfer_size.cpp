#include "gta20/render/transfer_size.hpp"

#include <algorithm>
#include <string>

namespace gta20::render {

Result<u32> rgba8_byte_size(u32 width, u32 height, std::string_view what) {
    const std::string size = std::to_string(width) + "x" + std::to_string(height);
    if (width == 0 || height == 0) {
        return make_error(std::string(what) + ": zero image size " + size);
    }
    // Two u32 factors cannot overflow 64 bits, but the further x4 can (e.g. 2^31 x 2^31), so the
    // pixel count is checked before it is turned into bytes.
    const std::uint64_t pixels = std::uint64_t{width} * height;
    if (pixels > kMaxTransferBytes / 4) {
        return make_error(std::string(what) + ": " + size + " RGBA8 is " + std::to_string(pixels) +
                          " pixels x 4 bytes, over the GPU transfer limit of " + std::to_string(kMaxTransferBytes) +
                          " bytes");
    }
    return static_cast<u32>(pixels * 4);
}

Result<u32> instance_byte_size(std::size_t count) {
    if (count > kMaxInstances) {
        return make_error("draw list has " + std::to_string(count) + " quads, over the limit of " +
                          std::to_string(kMaxInstances) + " per frame");
    }
    return static_cast<u32>(count * sizeof(scene::QuadInstance));
}

Result<u32> grow_instance_capacity(u32 current, std::size_t count) {
    if (auto bytes = instance_byte_size(count); !bytes) {
        return bytes.error();
    }
    u32 capacity = std::clamp(current, kInitialInstanceCapacity, kMaxInstances);
    while (capacity < count) {
        capacity = capacity > kMaxInstances / 2 ? kMaxInstances : capacity * 2;
    }
    return capacity;
}

} // namespace gta20::render
