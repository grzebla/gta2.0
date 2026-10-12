#include "gta20/core/handle_pool.hpp"

namespace gta20 {

EntityId HandlePool::allocate() {
    if (!free_.empty()) {
        const u32 index = free_.back();
        free_.pop_back();
        u32& gen = generations_[index];
        ++gen; // even (free) -> odd (alive)
        return {index, gen};
    }
    generations_.push_back(1);
    return {static_cast<u32>(generations_.size() - 1), 1};
}

bool HandlePool::release(EntityId id) {
    if (!alive(id)) {
        return false;
    }
    ++generations_[id.index]; // odd -> even
    free_.push_back(id.index);
    return true;
}

bool HandlePool::alive(EntityId id) const {
    return id.valid() && id.index < generations_.size() && generations_[id.index] == id.generation &&
           (id.generation & 1U) != 0;
}

} // namespace gta20
