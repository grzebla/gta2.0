#pragma once

#include <vector>

#include "g2n/core/types.hpp"

namespace g2n {

/// Peak resident set size of this process in bytes (0 if unavailable).
[[nodiscard]] u64 peak_rss_bytes();

struct DurationStats {
    u64 count = 0;
    double mean_ns = 0;
    u64 min_ns = 0;
    u64 p50_ns = 0;
    u64 p99_ns = 0;
    u64 max_ns = 0;
};

/// Summary statistics over recorded durations (sorts a copy; call off the hot path).
[[nodiscard]] DurationStats summarize(std::vector<u64> samples_ns);

} // namespace g2n
