// gta2n: vertical slice of the native GTA 2 engine.
//
//   gta2n [--map <file.gmp> | --game-dir <dir> --level data/wil.gmp] [--frames N]
//         [--offscreen WxH --screenshot out.bmp] [--driver metal|direct3d12|vulkan] [--gpu-debug]
//
// Simulation runs at a fixed 30 Hz (g2n::sim::World, no SDL/GPU inside); rendering runs
// as fast as vsync allows and interpolates between the last two simulation states.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "g2n/assets/file_io.hpp"
#include "g2n/assets/gmp_writer.hpp"
#include "g2n/core/clock.hpp"
#include "g2n/core/diagnostics.hpp"
#include "g2n/platform/platform.hpp"
#include "g2n/render/renderer.hpp"
#include "g2n/scene/draw_list.hpp"
#include "g2n/sim/world.hpp"

namespace {

using namespace g2n;

constexpr u32 kSimRateHz = 30;

struct Options {
    std::string map_path;
    std::string game_dir;
    std::string level = "data/wil.gmp";
    long frames = -1;
    u32 offscreen_w = 0, offscreen_h = 0;
    std::string screenshot;
    const char* driver = nullptr;
    bool gpu_debug = false;
};

/// Parses "WxH" with 1..8192 per side.
bool parse_size(const std::string& text, u32& width, u32& height) {
    const auto x = text.find('x');
    if (x == std::string::npos) {
        return false;
    }
    char* end_w = nullptr;
    char* end_h = nullptr;
    const unsigned long w = std::strtoul(text.c_str(), &end_w, 10);
    const unsigned long h = std::strtoul(text.c_str() + x + 1, &end_h, 10);
    if (end_w != text.c_str() + x || *end_h != '\0' || w == 0 || h == 0 || w > 8192 || h > 8192) {
        return false;
    }
    width = static_cast<u32>(w);
    height = static_cast<u32>(h);
    return true;
}

std::optional<Options> parse_options(int argc, char** argv) {
    Options o;
    if (const char* env = std::getenv("G2N_GAME_DIR"))
        o.game_dir = env;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const bool has_value = i + 1 < argc;
        if (a == "--map" && has_value)
            o.map_path = argv[++i];
        else if (a == "--game-dir" && has_value)
            o.game_dir = argv[++i];
        else if (a == "--level" && has_value)
            o.level = argv[++i];
        else if (a == "--frames" && has_value)
            o.frames = std::strtol(argv[++i], nullptr, 10);
        else if (a == "--screenshot" && has_value)
            o.screenshot = argv[++i];
        else if (a == "--driver" && has_value)
            o.driver = argv[++i];
        else if (a == "--gpu-debug")
            o.gpu_debug = true;
        else if (a == "--offscreen" && has_value) {
            if (!parse_size(argv[++i], o.offscreen_w, o.offscreen_h))
                return std::nullopt;
        } else {
            return std::nullopt;
        }
    }
    if (o.offscreen_w > 0 && o.screenshot.empty())
        return std::nullopt;
    return o;
}

/// Loads the requested original map, else the SYNTHETIC test map. Returns a label for the title.
Result<std::pair<assets::GmpMap, std::string>> load_map(const Options& o) {
    std::vector<std::byte> bytes;
    std::string label;
    if (!o.map_path.empty() || !o.game_dir.empty()) {
        std::filesystem::path path = o.map_path;
        if (path.empty()) {
            auto resolved = assets::resolve_game_path(o.game_dir, o.level);
            if (!resolved)
                return resolved.error();
            path = resolved.value();
        }
        auto file = assets::read_file(path);
        if (!file)
            return file.error();
        bytes = std::move(file.value());
        label = path.filename().string();
    } else {
        bytes = assets::build_synthetic_test_map();
        label = "SYNTHETIC test map";
    }
    auto map = assets::parse_gmp(bytes);
    if (!map)
        return map.error();
    return std::make_pair(std::move(map.value()), label);
}

sim::InputFrame to_sim_input(const platform::InputSnapshot& s) {
    sim::InputFrame f;
    f.set(sim::Button::Forward, s.up);
    f.set(sim::Button::Backward, s.down);
    f.set(sim::Button::Left, s.left);
    f.set(sim::Button::Right, s.right);
    f.set(sim::Button::Attack, s.attack);
    f.set(sim::Button::Jump, s.jump);
    f.set(sim::Button::EnterExit, s.enter_exit);
    return f;
}

/// Render-side copy of the parts of the world we draw (floats; never fed back).
struct RenderState {
    Vec2f player;
    float heading = 0;
    std::vector<Vec2f> wanderers;

    static RenderState capture(const sim::World& w) {
        RenderState r;
        r.player = to_vec2f(w.player().position);
        r.heading = static_cast<float>(w.player().heading.to_radians());
        for (const auto& wd : w.wanderers()) {
            r.wanderers.push_back({static_cast<float>(wd.cell_x) + 0.5f, static_cast<float>(wd.cell_y) + 0.5f});
        }
        return r;
    }
};

float lerp_angle(float a, float b, float t) {
    float d = std::remainder(b - a, 6.2831853f);
    return a + d * t;
}

void build_frame(scene::DrawList& list, const assets::GmpMap& map, const RenderState& prev, const RenderState& curr,
                 float alpha, Camera2D& camera, Vec2f viewport) {
    const Vec2f player = lerp(prev.player, curr.player, alpha);
    camera.center = player;
    list.clear();
    scene::append_map(list, map, camera.visible_world(viewport));
    std::vector<scene::EntityView> entities;
    for (std::size_t i = 0; i < curr.wanderers.size() && i < prev.wanderers.size(); ++i) {
        entities.push_back({lerp(prev.wanderers[i], curr.wanderers[i], alpha), 0.0f, 0.4f, 70, 170, 240});
    }
    entities.push_back({player, lerp_angle(prev.heading, curr.heading, alpha), 0.6f, 240, 200, 40});
    scene::append_entities(list, entities);
}

int run(const Options& o) {
    const bool offscreen = o.offscreen_w > 0;
    auto sdl = platform::SdlRuntime::create(!offscreen);
    if (!sdl) {
        std::fprintf(stderr, "error: %s\n", sdl.error().message.c_str());
        return 1;
    }

    auto map = load_map(o);
    if (!map) {
        std::fprintf(stderr, "error: %s\n", map.error().message.c_str());
        return 1;
    }
    const auto& [gmp, map_label] = map.value();

    sim::WorldSettings settings;
    settings.tick_rate_hz = kSimRateHz;
    sim::World world(&gmp, settings);
    Camera2D camera;
    scene::DrawList list;

    if (offscreen) {
        auto renderer = render::Renderer::create(nullptr, {o.gpu_debug, o.driver});
        if (!renderer) {
            std::fprintf(stderr, "error: %s\n", renderer.error().message.c_str());
            return 1;
        }
        // Deterministic content: a few scripted ticks, then one frame.
        sim::InputFrame forward;
        forward.set(sim::Button::Forward, true);
        for (int i = 0; i < 20; ++i)
            world.step(forward);
        const auto state = RenderState::capture(world);
        const Vec2f viewport{static_cast<float>(o.offscreen_w), static_cast<float>(o.offscreen_h)};
        build_frame(list, gmp, state, state, 0.0f, camera, viewport);
        auto image = renderer.value()->render_offscreen(list, camera, o.offscreen_w, o.offscreen_h, {});
        if (!image) {
            std::fprintf(stderr, "error: %s\n", image.error().message.c_str());
            return 1;
        }
        if (auto saved = platform::save_rgba_bmp(o.screenshot, image.value().width, image.value().height,
                                                 image.value().rgba.data());
            !saved) {
            std::fprintf(stderr, "error: %s\n", saved.error().message.c_str());
            return 1;
        }
        std::printf("backend %s, %zu quads, map: %s -> %s\n", renderer.value()->backend().c_str(), list.quads().size(),
                    map_label.c_str(), o.screenshot.c_str());
        return 0;
    }

    auto window = platform::Window::create("gta2-native", 1280, 720);
    if (!window) {
        std::fprintf(stderr, "error: %s\n", window.error().message.c_str());
        return 1;
    }
    auto renderer = render::Renderer::create(window.value()->handle(), {o.gpu_debug, o.driver});
    if (!renderer) {
        std::fprintf(stderr, "error: %s\n", renderer.error().message.c_str());
        return 1;
    }
    const std::string backend = renderer.value()->backend();
    std::printf("gta2-native: backend %s, map: %s\n", backend.c_str(), map_label.c_str());

    FrameAccumulator accumulator(kSimRateHz, 5);
    RenderState prev = RenderState::capture(world);
    RenderState curr = prev;
    std::vector<u64> frame_ns;
    frame_ns.reserve(4096);
    u64 last = platform::now_ns();
    u64 title_at = last;
    long frame = 0;

    for (;;) {
        const auto input = platform::poll_input();
        if (input.quit || (o.frames >= 0 && frame >= o.frames))
            break;
        camera.pixels_per_unit = std::clamp(camera.pixels_per_unit * std::pow(1.15f, input.zoom_steps), 4.0f, 128.0f);

        const u64 now = platform::now_ns();
        const u64 elapsed = now - last;
        last = now;
        // Sample input once per frame and feed it to every fixed tick run this frame.
        const sim::InputFrame sim_input = to_sim_input(input);
        for (u32 steps = accumulator.add(elapsed); steps > 0; --steps) {
            prev = curr;
            world.step(sim_input);
            curr = RenderState::capture(world);
        }

        build_frame(list, gmp, prev, curr, accumulator.alpha(), camera, window.value()->size_pixels());
        if (auto r = renderer.value()->render_to_window(list, camera, {}); !r) {
            std::fprintf(stderr, "error: %s\n", r.error().message.c_str());
            return 1;
        }
        frame_ns.push_back(elapsed);
        ++frame;
        if (now - title_at > 1'000'000'000ULL) {
            title_at = now;
            char title[160];
            std::snprintf(title, sizeof(title), "gta2-native [%s] %s  tick %llu  %zu quads", backend.c_str(),
                          map_label.c_str(), static_cast<unsigned long long>(world.tick()), list.quads().size());
            window.value()->set_title(title);
        }
    }

    if (frame_ns.size() > 1)
        frame_ns.erase(frame_ns.begin()); // first frame includes start-up
    const auto st = summarize(frame_ns);
    std::printf("frames %llu  frame ms mean %.2f p50 %.2f p99 %.2f max %.2f  sim ticks %llu (dropped %llu)  peak RSS "
                "%.1f MiB\n",
                static_cast<unsigned long long>(st.count), st.mean_ns / 1e6, static_cast<double>(st.p50_ns) / 1e6,
                static_cast<double>(st.p99_ns) / 1e6, static_cast<double>(st.max_ns) / 1e6,
                static_cast<unsigned long long>(world.tick()),
                static_cast<unsigned long long>(accumulator.dropped_steps()),
                static_cast<double>(peak_rss_bytes()) / (1024.0 * 1024.0));
    return 0;
}

} // namespace

int main(int argc, char** argv) try {
    const auto options = parse_options(argc, argv);
    if (!options) {
        std::fprintf(
            stderr,
            "usage: gta2n [--map file.gmp | --game-dir dir --level data/wil.gmp] [--frames N]\n"
            "             [--offscreen WxH --screenshot out.bmp] [--driver metal|direct3d12|vulkan] [--gpu-debug]\n");
        return 2;
    }
    return run(*options);
} catch (const std::exception& e) {
    std::fprintf(stderr, "fatal: %s\n", e.what());
    return 1;
}
