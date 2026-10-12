#pragma once

#include <vector>

#include "gta20/core/types.hpp"

namespace gta20 {

/// Generational object identifier. A stale handle (object freed, slot reused)
/// is detected because its generation no longer matches.
struct EntityId {
    u32 index = 0;
    u32 generation = 0; // 0 = never valid

    [[nodiscard]] constexpr bool valid() const { return generation != 0; }
    friend constexpr bool operator==(EntityId, EntityId) = default;
};

/// Allocates EntityIds with slot reuse (LIFO free list). No per-object heap allocation.
class HandlePool {
public:
    [[nodiscard]] EntityId allocate();
    /// Returns false for invalid or stale handles.
    bool release(EntityId id);
    [[nodiscard]] bool alive(EntityId id) const;
    [[nodiscard]] std::size_t live_count() const { return generations_.size() - free_.size(); }
    [[nodiscard]] std::size_t capacity() const { return generations_.size(); }

private:
    std::vector<u32> generations_; // odd = alive, even = free
    std::vector<u32> free_;
};

} // namespace gta20
