// gta2: vertical slice of the native GTA 2 engine.
//
//   gta2 [--game-dir <dir>] [--level data/wil.gmp] [--map <file.gmp> [--style <file.sty>]]
//         [--frames N] [--camera X,Y] [--zoom PIXELS_PER_BLOCK]
//         [--offscreen WxH --screenshot out.bmp] [--driver metal|direct3d12|vulkan] [--gpu-debug]
//
// The game directory defaults to GTA20_GAME_DIR. Without original data the SYNTHETIC test map
// is shown with flat debug colours. Simulation runs at a fixed 30 Hz (gta20::sim::World);
// rendering interpolates between the last two simulation states.
//
// Controls: arrows/WASD move the player (or pan in free camera), Tab toggles free camera,
// mouse wheel or +/- zooms, Esc quits.
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

#include "gta20/assets/file_io.hpp"
#include "gta20/assets/game_install.hpp"
#include "gta20/assets/gmp_writer.hpp"
#include "gta20/assets/map_inventory.hpp"
#include "gta20/assets/sty_style.hpp"
#include "gta20/core/clock.hpp"
#include "gta20/core/diagnostics.hpp"
#include "gta20/platform/platform.hpp"
#include "gta20/render/renderer.hpp"
#include "gta20/scene/draw_list.hpp"
#include "gta20/sim/world.hpp"

namespace {

using namespace gta20;

constexpr u32 kSimRateHz = 30;

struct Options {
    std::filesystem::path game_dir;
    std::string level = "data/wil.gmp";
    std::string map_path;
    std::string style_path;
    long frames = -1;
    std::optional<Vec2f> camera;
    float zoom = 32.0f;
    u32 offscreen_w = 0, offscreen_h = 0;
    std::string screenshot;
    const char* driver = nullptr;
    bool gpu_debug = false;
};

/// Parses "AxB" (sep 'x') or "A,B" (sep ',') as two non-negative numbers.
template <class T>
bool parse_pair(const std::string& text, char sep, T& a, T& b) {
    const auto at = text.find(sep);
    if (at == std::string::npos) {
        return false;
    }
    char* end_a = nullptr;
    char* end_b = nullptr;
    const double va = std::strtod(text.c_str(), &end_a);
    const double vb = std::strtod(text.c_str() + at + 1, &end_b);
    if (end_a != text.c_str() + at || *end_b != '\0' || !std::isfinite(va) || !std::isfinite(vb) || va < 0 || vb < 0) {
        return false;
    }
    a = static_cast<T>(va);
    b = static_cast<T>(vb);
    return true;
}

std::optional<Options> parse_options(int argc, char** argv) {
    Options o;
    o.game_dir = assets::game_dir_from_environment();
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const bool has_value = i + 1 < argc;
        if (a == "--game-dir" && has_value) {
            o.game_dir = argv[++i];
        } else if (a == "--level" && has_value) {
            o.level = argv[++i];
        } else if (a == "--map" && has_value) {
            o.map_path = argv[++i];
        } else if (a == "--style" && has_value) {
            o.style_path = argv[++i];
        } else if (a == "--frames" && has_value) {
            o.frames = std::strtol(argv[++i], nullptr, 10);
        } else if (a == "--camera" && has_value) {
            Vec2f c;
            if (!parse_pair(argv[++i], ',', c.x, c.y)) {
                return std::nullopt;
            }
            o.camera = c;
        } else if (a == "--zoom" && has_value) {
            o.zoom = std::clamp(static_cast<float>(std::strtod(argv[++i], nullptr)), 2.0f, 256.0f);
        } else if (a == "--screenshot" && has_value) {
            o.screenshot = argv[++i];
        } else if (a == "--driver" && has_value) {
            o.driver = argv[++i];
        } else if (a == "--gpu-debug") {
            o.gpu_debug = true;
        } else if (a == "--offscreen" && has_value) {
            if (!parse_pair(argv[++i], 'x', o.offscreen_w, o.offscreen_h) || o.offscreen_w == 0 || o.offscreen_h == 0 ||
                o.offscreen_w > 8192 || o.offscreen_h > 8192) {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    }
    if (o.offscreen_w > 0 && o.screenshot.empty()) {
        return std::nullopt;
    }
    return o;
}

struct LoadedWorld {
    assets::GmpMap map;
    std::optional<assets::StyStyle> style;
    std::string label;
    bool synthetic = false;
};

Result<std::vector<std::byte>> read_game_file(const Options& o, const std::string& explicit_path,
                                              const std::string& relative) {
    std::filesystem::path path = explicit_path;
    if (path.empty()) {
        auto resolved = assets::resolve_game_path(o.game_dir, relative);
        if (!resolved) {
            return resolved.error();
        }
        path = resolved.value();
    }
    return assets::read_file(path);
}

/// Original map + style when available; otherwise the clearly labelled SYNTHETIC map.
Result<LoadedWorld> load_world(const Options& o) {
    LoadedWorld w;
    const bool original = !o.map_path.empty() || !o.game_dir.empty();
    if (!original) {
        auto map = assets::parse_gmp(assets::build_synthetic_test_map());
        if (!map) {
            return map.error();
        }
        w.map = std::move(map.value());
        w.label = "SYNTHETIC test map";
        w.synthetic = true;
        return w;
    }
    auto map_bytes = read_game_file(o, o.map_path, o.level);
    if (!map_bytes) {
        return map_bytes.error();
    }
    auto map = assets::parse_gmp(map_bytes.value());
    if (!map) {
        return make_error("map: " + map.error().message);
    }
    w.map = std::move(map.value());
    w.label = std::filesystem::path(o.map_path.empty() ? o.level : o.map_path).filename().string();

    // Style: explicit, else the one the original game pairs with this map (lorne2e.gmp -> wil.sty),
    // else the map's sibling .sty for maps that are not part of the original release.
    const auto* paired = assets::find_map_style(o.map_path.empty() ? o.level : o.map_path);
    const auto default_style = [&](const std::filesystem::path& map) {
        auto style = std::filesystem::path(map).replace_extension(".sty");
        if (paired != nullptr && paired->resolved()) {
            style.replace_filename(std::filesystem::path(paired->style).filename());
        }
        return style;
    };
    const std::string style_relative = default_style(o.level).generic_string();
    std::string style_explicit = o.style_path;
    if (style_explicit.empty() && !o.map_path.empty()) {
        style_explicit = default_style(o.map_path).string();
    }
    if (auto style_bytes = read_game_file(o, style_explicit, style_relative)) {
        auto style = assets::parse_sty(style_bytes.value());
        if (!style) {
            return make_error("style: " + style.error().message);
        }
        w.style = std::move(style.value());
        w.label +=
            " + " + std::filesystem::path(style_explicit.empty() ? style_relative : style_explicit).filename().string();
    } else {
        std::fprintf(stderr, "warning: no style found (%s) - drawing debug colours\n",
                     style_bytes.error().message.c_str());
    }
    return w;
}

/// First road cell spiralling out from the map centre; spawn is in the air layer above it.
std::pair<FixedVec2, int> find_spawn(const assets::GmpMap& map) {
    for (int r = 0; r < 128; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) {
                    continue;
                }
                const int x = 128 + dx;
                const int y = 128 + dy;
                const int z = map.top_level(x, y);
                if (z >= 0 && z < assets::kMapLevels - 1 &&
                    map.block_at(x, y, z).ground() == assets::GroundType::Road) {
                    return {
                        {Fixed::from_int(x) + Fixed::from_ratio(1, 2), Fixed::from_int(y) + Fixed::from_ratio(1, 2)},
                        z + 1};
                }
            }
        }
    }
    return {{Fixed::from_int(128), Fixed::from_int(128)}, 2};
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
    const float d = std::remainder(b - a, 6.2831853f);
    return a + d * t;
}

void build_frame(scene::DrawList& list, const LoadedWorld& world, const RenderState& prev, const RenderState& curr,
                 float alpha, const Camera2D& camera, Vec2f viewport) {
    list.clear();
    const Rect visible = camera.visible_world(viewport);
    if (world.style) {
        scene::append_map_textured(list, world.map, visible, static_cast<u16>(world.style->tile_count()));
    } else {
        scene::append_map(list, world.map, visible);
    }
    std::vector<scene::EntityView> entities;
    for (std::size_t i = 0; i < curr.wanderers.size() && i < prev.wanderers.size(); ++i) {
        entities.push_back({lerp(prev.wanderers[i], curr.wanderers[i], alpha), 0.0f, 0.4f, 70, 170, 240});
    }
    entities.push_back(
        {lerp(prev.player, curr.player, alpha), lerp_angle(prev.heading, curr.heading, alpha), 0.6f, 240, 200, 40});
    scene::append_entities(list, entities);
}

int run(const Options& o) {
    const bool offscreen = o.offscreen_w > 0;
    auto sdl = platform::SdlRuntime::create(!offscreen);
    if (!sdl) {
        std::fprintf(stderr, "error: %s\n", sdl.error().message.c_str());
        return 1;
    }

    std::string version = "-";
    if (!o.game_dir.empty()) {
        if (auto install = assets::inspect_game_dir(o.game_dir)) {
            version = install.value().exe_version.value_or("unknown");
        }
    }
    auto loaded = load_world(o);
    if (!loaded) {
        std::fprintf(stderr, "error: %s\n", loaded.error().message.c_str());
        return 1;
    }
    const LoadedWorld& world_data = loaded.value();
    const scene::TileAtlas atlas =
        world_data.style ? scene::build_tile_atlas(*world_data.style) : scene::make_white_atlas();

    sim::WorldSettings settings;
    settings.tick_rate_hz = kSimRateHz;
    if (!world_data.synthetic) {
        const auto [spawn, level] = find_spawn(world_data.map);
        settings.spawn = spawn;
        settings.level = level;
    }
    sim::World world(&world_data.map, settings);
    Camera2D camera;
    camera.pixels_per_unit = o.zoom;
    scene::DrawList list;

    const auto upload_atlas = [&](render::Renderer& r) {
        return r.set_atlas(static_cast<u32>(scene::TileAtlas::kSize), static_cast<u32>(scene::TileAtlas::kSize),
                           atlas.rgba);
    };

    if (offscreen) {
        auto renderer = render::Renderer::create(nullptr, {o.gpu_debug, o.driver});
        if (!renderer) {
            std::fprintf(stderr, "error: %s\n", renderer.error().message.c_str());
            return 1;
        }
        if (auto a = upload_atlas(*renderer.value()); !a) {
            std::fprintf(stderr, "error: %s\n", a.error().message.c_str());
            return 1;
        }
        const auto state = RenderState::capture(world);
        camera.center = o.camera.value_or(state.player);
        const Vec2f viewport{static_cast<float>(o.offscreen_w), static_cast<float>(o.offscreen_h)};
        build_frame(list, world_data, state, state, 0.0f, camera, viewport);
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
        // Image statistics for automated checks: distinct colours and share of pixels that differ
        // from the clear colour (i.e. covered by map/entities).
        std::vector<u32> colours;
        colours.reserve(image.value().rgba.size() / 4);
        std::size_t covered = 0;
        const auto& px = image.value().rgba;
        for (std::size_t i = 0; i + 3 < px.size(); i += 4) {
            colours.push_back((static_cast<u32>(px[i]) << 16) | (static_cast<u32>(px[i + 1]) << 8) | px[i + 2]);
            const bool is_clear = std::abs(static_cast<int>(px[i]) - 20) < 3 &&
                                  std::abs(static_cast<int>(px[i + 1]) - 20) < 3 &&
                                  std::abs(static_cast<int>(px[i + 2]) - 26) < 3;
            covered += is_clear ? 0 : 1;
        }
        std::sort(colours.begin(), colours.end());
        const auto unique = static_cast<std::size_t>(std::unique(colours.begin(), colours.end()) - colours.begin());
        std::printf("image_stats unique_colours=%zu covered=%.3f\n", unique,
                    static_cast<double>(covered) / (static_cast<double>(px.size()) / 4.0));
        std::printf("backend %s | game %s | map %s | camera %.1f,%.1f zoom %.0f | %zu quads | atlas %u tiles -> %s\n",
                    renderer.value()->backend().c_str(), version.c_str(), world_data.label.c_str(),
                    static_cast<double>(camera.center.x), static_cast<double>(camera.center.y),
                    static_cast<double>(camera.pixels_per_unit), list.quads().size(),
                    static_cast<unsigned>(atlas.tile_count), o.screenshot.c_str());
        return 0;
    }

    auto window = platform::Window::create("GTA2.0", 1280, 720);
    if (!window) {
        std::fprintf(stderr, "error: %s\n", window.error().message.c_str());
        return 1;
    }
    auto renderer = render::Renderer::create(window.value()->handle(), {o.gpu_debug, o.driver});
    if (!renderer) {
        std::fprintf(stderr, "error: %s\n", renderer.error().message.c_str());
        return 1;
    }
    if (auto a = upload_atlas(*renderer.value()); !a) {
        std::fprintf(stderr, "error: %s\n", a.error().message.c_str());
        return 1;
    }
    const std::string backend = renderer.value()->backend();
    std::printf("gta2: backend %s, game %s, map %s\n", backend.c_str(), version.c_str(), world_data.label.c_str());

    FrameAccumulator accumulator(kSimRateHz, 5);
    RenderState prev = RenderState::capture(world);
    RenderState curr = prev;
    bool free_camera = o.camera.has_value();
    Vec2f free_center = o.camera.value_or(curr.player);
    bool tab_was_down = false;
    std::vector<u64> frame_ns;
    frame_ns.reserve(4096);
    u64 last = platform::now_ns();
    u64 title_at = last;
    long frame = 0;
    u32 frames_this_second = 0;
    u32 fps = 0;

    for (;;) {
        const auto input = platform::poll_input();
        if (input.quit || (o.frames >= 0 && frame >= o.frames)) {
            break;
        }
        if (input.toggle_camera && !tab_was_down) {
            free_camera = !free_camera;
            free_center = camera.center;
        }
        tab_was_down = input.toggle_camera;
        camera.pixels_per_unit = std::clamp(camera.pixels_per_unit * std::pow(1.15f, input.zoom_steps), 2.0f, 256.0f);

        const u64 now = platform::now_ns();
        const u64 elapsed = now - last;
        last = now;
        const sim::InputFrame sim_input = free_camera ? sim::InputFrame{} : to_sim_input(input);
        for (u32 steps = accumulator.add(elapsed); steps > 0; --steps) {
            prev = curr;
            world.step(sim_input);
            curr = RenderState::capture(world);
        }

        if (free_camera) {
            // Pan at ~half a screen per second regardless of zoom.
            const float speed = 640.0f / camera.pixels_per_unit * static_cast<float>(elapsed) * 1e-9f;
            free_center.x += (input.right ? speed : 0.0f) - (input.left ? speed : 0.0f);
            free_center.y += (input.down ? speed : 0.0f) - (input.up ? speed : 0.0f);
            camera.center = free_center;
        } else {
            camera.center = lerp(prev.player, curr.player, accumulator.alpha());
        }

        build_frame(list, world_data, prev, curr, accumulator.alpha(), camera, window.value()->size_pixels());
        if (auto r = renderer.value()->render_to_window(list, camera, {}); !r) {
            std::fprintf(stderr, "error: %s\n", r.error().message.c_str());
            return 1;
        }
        frame_ns.push_back(elapsed);
        ++frame;
        ++frames_this_second;
        if (now - title_at > 1'000'000'000ULL) {
            title_at = now;
            fps = frames_this_second;
            frames_this_second = 0;
            char title[256];
            std::snprintf(title, sizeof(title),
                          "GTA2.0 [%s] GTA2 %s | %s | %s cam %.1f,%.1f x%.0f | %u fps | %zu quads | tick %llu",
                          backend.c_str(), version.c_str(), world_data.label.c_str(), free_camera ? "free" : "follow",
                          static_cast<double>(camera.center.x), static_cast<double>(camera.center.y),
                          static_cast<double>(camera.pixels_per_unit), fps, list.quads().size(),
                          static_cast<unsigned long long>(world.tick()));
            window.value()->set_title(title);
        }
    }

    if (frame_ns.size() > 1) {
        frame_ns.erase(frame_ns.begin()); // first frame includes start-up
    }
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
        std::fprintf(stderr, "usage: gta2 [--game-dir dir] [--level data/wil.gmp] [--map f.gmp [--style f.sty]]\n"
                             "             [--frames N] [--camera X,Y] [--zoom PPU]\n"
                             "             [--offscreen WxH --screenshot out.bmp] [--driver metal|direct3d12|vulkan] "
                             "[--gpu-debug]\n");
        return 2;
    }
    return run(*options);
} catch (const std::exception& e) {
    std::fprintf(stderr, "fatal: %s\n", e.what());
    return 1;
}
