#include "g2n/core/diagnostics.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

#if defined(_WIN32)
// clang-format off: <windows.h> must come before <psapi.h>
#include <windows.h>
#include <psapi.h>
// clang-format on
#elif defined(__APPLE__) || defined(__linux__)
#include <sys/resource.h>
#endif

namespace g2n {

u64 peak_rss_bytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        return static_cast<u64>(counters.PeakWorkingSetSize);
    }
    return 0;
#elif defined(__APPLE__)
    rusage usage{};
    return getrusage(RUSAGE_SELF, &usage) == 0 ? static_cast<u64>(usage.ru_maxrss) : 0; // bytes on macOS
#elif defined(__linux__)
    rusage usage{};
    return getrusage(RUSAGE_SELF, &usage) == 0 ? static_cast<u64>(usage.ru_maxrss) * 1024 : 0; // KiB on Linux
#else
    return 0;
#endif
}

DurationStats summarize(std::vector<u64> samples_ns) {
    DurationStats s;
    if (samples_ns.empty()) {
        return s;
    }
    std::sort(samples_ns.begin(), samples_ns.end());
    const auto at = [&](double q) {
        const auto idx = static_cast<std::size_t>(std::llround(q * static_cast<double>(samples_ns.size() - 1)));
        return samples_ns[idx];
    };
    s.count = samples_ns.size();
    s.mean_ns = static_cast<double>(std::accumulate(samples_ns.begin(), samples_ns.end(), u64{0})) /
                static_cast<double>(samples_ns.size());
    s.min_ns = samples_ns.front();
    s.p50_ns = at(0.50);
    s.p99_ns = at(0.99);
    s.max_ns = samples_ns.back();
    return s;
}

} // namespace g2n
