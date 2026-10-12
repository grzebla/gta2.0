// g2n-headless: runs and compares conformance scenarios without SDL, a window or a GPU.
//
//   g2n-headless validate <scenario.json>
//   g2n-headless run <scenario.json> --out <trace.jsonl> [--game-dir <dir>]
//   g2n-headless compare <expected.jsonl> <actual.jsonl> --scenario <scenario.json> [--json f] [--markdown f]
//   g2n-headless bench <scenario.json> [--repeat N] [--game-dir <dir>]
//
// Exit codes: 0 ok / traces match, 1 traces diverge, 2 usage or input error.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "g2n/core/diagnostics.hpp"
#include "g2n/testing/compare.hpp"
#include "g2n/testing/runner.hpp"

namespace {

constexpr int kOk = 0;
constexpr int kDiverged = 1;
constexpr int kUsage = 2;

struct Args {
    std::vector<std::string> positional;
    std::map<std::string, std::string> named;

    static Args parse(int argc, char** argv, int first) {
        Args a;
        for (int i = first; i < argc; ++i) {
            std::string s = argv[i];
            if (s.rfind("--", 0) == 0 && i + 1 < argc) {
                a.named[s] = argv[++i];
            } else {
                a.positional.push_back(std::move(s));
            }
        }
        return a;
    }
    [[nodiscard]] std::string get(const std::string& k, const std::string& fallback = {}) const {
        const auto it = named.find(k);
        return it == named.end() ? fallback : it->second;
    }
};

int usage() {
    std::cerr << "usage:\n"
                 "  g2n-headless validate <scenario.json>\n"
                 "  g2n-headless run <scenario.json> --out <trace.jsonl> [--game-dir <dir>]\n"
                 "  g2n-headless compare <expected.jsonl> <actual.jsonl> --scenario <scenario.json> [--json f] "
                 "[--markdown f]\n"
                 "  g2n-headless bench <scenario.json> [--repeat N] [--game-dir <dir>]\n";
    return kUsage;
}

g2n::Result<g2n::testing::Scenario> load_valid(const std::string& path) {
    auto s = g2n::testing::load_scenario(path);
    if (!s)
        return s;
    if (const auto errors = g2n::testing::validate(s.value()); !errors.empty()) {
        std::string all;
        for (const auto& e : errors)
            all += "\n- " + e;
        return g2n::make_error(path + " is invalid:" + all);
    }
    return s;
}

std::string game_dir(const Args& a) {
    if (auto d = a.get("--game-dir"); !d.empty())
        return d;
    const char* env = std::getenv("G2N_GAME_DIR");
    return env != nullptr ? env : "";
}

} // namespace

int main(int argc, char** argv) try {
    if (argc < 2)
        return usage();
    const std::string command = argv[1];
    const Args args = Args::parse(argc, argv, 2);

    if (command == "validate" && args.positional.size() == 1) {
        auto s = load_valid(args.positional[0]);
        if (!s) {
            std::cerr << "error: " << s.error().message << "\n";
            return kUsage;
        }
        std::cout << "ok: " << s.value().id << " (" << g2n::testing::to_string(s.value().provenance) << ", "
                  << s.value().ticks << " ticks @ " << s.value().tick_rate_hz << " Hz)\n";
        return kOk;
    }

    if (command == "run" && args.positional.size() == 1 && !args.get("--out").empty()) {
        auto s = load_valid(args.positional[0]);
        if (!s) {
            std::cerr << "error: " << s.error().message << "\n";
            return kUsage;
        }
        auto adapter = g2n::testing::make_engine_adapter(game_dir(args));
        auto trace = g2n::testing::run_scenario(s.value(), *adapter);
        if (!trace) {
            std::cerr << "error: " << trace.error().message << "\n";
            return kUsage;
        }
        if (auto w = g2n::testing::write_trace_file(trace.value(), args.get("--out")); !w) {
            std::cerr << "error: " << w.error().message << "\n";
            return kUsage;
        }
        std::cout << "wrote " << trace.value().snapshots.size() << " snapshots to " << args.get("--out") << "\n";
        return kOk;
    }

    if (command == "compare" && args.positional.size() == 2 && !args.get("--scenario").empty()) {
        auto s = load_valid(args.get("--scenario"));
        auto expected = g2n::testing::read_trace_file(args.positional[0]);
        auto actual = g2n::testing::read_trace_file(args.positional[1]);
        for (const auto* r : {&expected, &actual}) {
            if (!*r) {
                std::cerr << "error: " << r->error().message << "\n";
                return kUsage;
            }
        }
        if (!s) {
            std::cerr << "error: " << s.error().message << "\n";
            return kUsage;
        }
        auto report = g2n::testing::compare_traces(expected.value(), actual.value(), s.value().tolerances);
        report.expected_source = args.positional[0];
        report.actual_source = args.positional[1];
        const auto md = g2n::testing::report_to_markdown(report);
        if (auto p = args.get("--json"); !p.empty())
            std::ofstream(p) << g2n::testing::report_to_json(report);
        if (auto p = args.get("--markdown"); !p.empty())
            std::ofstream(p) << md;
        std::cout << md;
        return report.passed() ? kOk : kDiverged;
    }

    if (command == "bench" && args.positional.size() == 1) {
        auto s = load_valid(args.positional[0]);
        if (!s) {
            std::cerr << "error: " << s.error().message << "\n";
            return kUsage;
        }
        const int repeat = std::max(1, std::stoi(args.get("--repeat", "20")));
        std::vector<g2n::u64> per_tick_ns;
        for (int r = 0; r < repeat; ++r) {
            auto adapter = g2n::testing::make_engine_adapter(game_dir(args));
            const auto t0 = std::chrono::steady_clock::now();
            auto trace = g2n::testing::run_scenario(s.value(), *adapter);
            const auto t1 = std::chrono::steady_clock::now();
            if (!trace) {
                std::cerr << "error: " << trace.error().message << "\n";
                return kUsage;
            }
            const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
            per_tick_ns.push_back(static_cast<g2n::u64>(ns) / static_cast<g2n::u64>(s.value().ticks));
        }
        const auto st = g2n::summarize(per_tick_ns);
        std::printf("bench %s: %d runs x %d ticks (incl. map load + capture)\n", s.value().id.c_str(), repeat,
                    s.value().ticks);
        std::printf("  ns/tick  mean %.0f  min %llu  p50 %llu  p99 %llu  max %llu\n", st.mean_ns,
                    static_cast<unsigned long long>(st.min_ns), static_cast<unsigned long long>(st.p50_ns),
                    static_cast<unsigned long long>(st.p99_ns), static_cast<unsigned long long>(st.max_ns));
        std::printf("  peak RSS %.1f MiB\n", static_cast<double>(g2n::peak_rss_bytes()) / (1024.0 * 1024.0));
        return kOk;
    }
    return usage();
} catch (const std::exception& e) {
    std::cerr << "fatal: " << e.what() << "\n";
    return kUsage;
}
