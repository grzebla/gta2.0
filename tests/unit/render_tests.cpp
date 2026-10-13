#include <SDL3/SDL.h>
#include <doctest/doctest.h>

#include "gta20/render/renderer.hpp"
#include "gta20/render/transfer_size.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace gta20;
using render::GpuFault;

namespace {

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

// --- GPU helpers: every test below that needs a device skips (passes with "SKIP:") without one.

constexpr u32 kSide = 16;

/// SDL_GPU needs the video subsystem (Metal also a display). Shut down again at exit so leak
/// checkers see a clean SDL.
struct SdlVideo {
    bool ok = SDL_InitSubSystem(SDL_INIT_VIDEO);
    std::string error = ok ? std::string() : std::string(SDL_GetError());

    SdlVideo() = default;
    ~SdlVideo() { SDL_Quit(); }
    SdlVideo(const SdlVideo&) = delete;
    SdlVideo& operator=(const SdlVideo&) = delete;
    SdlVideo(SdlVideo&&) = delete;
    SdlVideo& operator=(SdlVideo&&) = delete;
};

std::unique_ptr<render::Renderer> gpu_renderer() {
    static const SdlVideo video;
    if (!video.ok) {
        MESSAGE("SKIP: no GPU device (SDL video: " << video.error << ")");
        return nullptr;
    }
    auto renderer = render::Renderer::create(nullptr, {});
    if (!renderer) {
        MESSAGE("SKIP: no GPU device (" << renderer.error().message << ")");
        return nullptr;
    }
    return std::move(renderer).value();
}

/// `count` identical opaque quads covering the centre of a kSide x kSide frame.
scene::DrawList centred_quads(std::size_t count) {
    scene::DrawList list;
    scene::QuadInstance quad;
    quad.width = 0.25f;
    quad.height = 0.25f;
    for (std::size_t i = 0; i < count; ++i) {
        list.add(quad);
    }
    return list;
}

Result<render::Image> draw(render::Renderer& renderer, const scene::DrawList& list) {
    return renderer.render_offscreen(list, Camera2D{}, kSide, kSide, {});
}

std::array<u8, 4> centre_pixel(const render::Image& image) {
    const std::size_t i = (std::size_t{kSide / 2} * kSide + kSide / 2) * 4;
    return {image.rgba[i], image.rgba[i + 1], image.rgba[i + 2], image.rgba[i + 3]};
}

struct FaultCase {
    GpuFault fault;
    u32 skip;
    const char* context;
};

} // namespace

TEST_CASE("Render sizes: RGBA8 byte counts reject zero sizes and Uint32 overflow") {
    CHECK(render::rgba8_byte_size(1, 1, "atlas").value() == 4u);
    CHECK(render::rgba8_byte_size(2048, 2048, "atlas").value() == 16'777'216u);
    CHECK(render::rgba8_byte_size(65'535, 16'384, "atlas").value() == 4'294'901'760u);
    CHECK(render::rgba8_byte_size(1'073'741'823, 1, "atlas").value() == 4'294'967'292u);

    for (const auto& [w, h] : std::vector<std::pair<u32, u32>>{{0, 1}, {1, 0}, {0, 0}, {0, UINT32_MAX}}) {
        CAPTURE(w);
        CAPTURE(h);
        const auto bytes = render::rgba8_byte_size(w, h, "offscreen frame");
        REQUIRE_FALSE(bytes);
        CHECK(contains(bytes.error().message, "offscreen frame: zero image size"));
    }
    for (const auto& [w, h] : std::vector<std::pair<u32, u32>>{
             {65'536, 16'384}, {16'384, 65'536}, {1'073'741'824, 1}, {65'536, 65'536}, {UINT32_MAX, UINT32_MAX}}) {
        CAPTURE(w);
        CAPTURE(h);
        const auto bytes = render::rgba8_byte_size(w, h, "atlas");
        REQUIRE_FALSE(bytes);
        CHECK(contains(bytes.error().message, "atlas: "));
        CHECK(contains(bytes.error().message, "over the GPU transfer limit of 4294967295"));
    }
}

TEST_CASE("Render sizes: instance byte counts and capacity growth stay within Uint32") {
    CHECK(render::kMaxInstances == 107'374'182u);
    CHECK(render::instance_byte_size(0).value() == 0u);
    CHECK(render::instance_byte_size(1).value() == 40u);
    CHECK(render::instance_byte_size(render::kMaxInstances).value() == 4'294'967'280u);
    CHECK_FALSE(render::instance_byte_size(std::size_t{render::kMaxInstances} + 1));
    CHECK_FALSE(render::instance_byte_size(SIZE_MAX));
    CHECK(contains(render::instance_byte_size(SIZE_MAX).error().message, "over the limit of 107374182"));

    CHECK(render::grow_instance_capacity(0, 1).value() == 1024u);
    CHECK(render::grow_instance_capacity(0, 1024).value() == 1024u);
    CHECK(render::grow_instance_capacity(0, 1025).value() == 2048u);
    CHECK(render::grow_instance_capacity(1024, 5000).value() == 8192u);
    CHECK(render::grow_instance_capacity(8192, 10).value() == 8192u); // never shrinks
    // Doubling past the limit clamps to it instead of wrapping.
    CHECK(render::grow_instance_capacity(67'108'864, 67'108'865).value() == render::kMaxInstances);
    CHECK(render::grow_instance_capacity(0, render::kMaxInstances).value() == render::kMaxInstances);
    CHECK(render::grow_instance_capacity(UINT32_MAX, 1).value() == render::kMaxInstances);
    CHECK_FALSE(render::grow_instance_capacity(0, std::size_t{render::kMaxInstances} + 1));
    CHECK_FALSE(render::grow_instance_capacity(render::kMaxInstances, SIZE_MAX));
    CHECK(render::instance_byte_size(render::grow_instance_capacity(1024, render::kMaxInstances).value()));
}

TEST_CASE("Renderer: invalid sizes are rejected before any GPU allocation") {
    auto renderer = gpu_renderer();
    if (renderer == nullptr) {
        return;
    }
    const auto list = centred_quads(1);
    const auto baseline = draw(*renderer, list);
    REQUIRE(baseline);

    // An armed texture-creation fault survives every rejected call, so none of them reached
    // SDL_CreateGPUTexture (nor any allocation sized from the request).
    renderer->inject_fault(GpuFault::create_texture);
    for (const auto& [w, h] : std::vector<std::pair<u32, u32>>{{0, kSide}, {kSide, 0}, {65'536, 65'536}}) {
        CAPTURE(w);
        CAPTURE(h);
        CHECK(contains(renderer->render_offscreen(list, Camera2D{}, w, h, {}).error().message, "offscreen frame: "));
    }
    const std::vector<u8> small(16, 255);
    CHECK(contains(renderer->set_atlas(0, 4, small).error().message, "atlas: zero image size 0x4"));
    CHECK(contains(renderer->set_atlas(65'536, 65'536, small).error().message, "over the GPU transfer limit"));
    CHECK(contains(renderer->set_atlas(2, 2, std::vector<u8>(15, 255)).error().message, "(expected 16)"));
    CHECK(contains(renderer->set_atlas(1, 1, small).error().message, "(expected 4)"));

    const auto armed = draw(*renderer, list);
    REQUIRE_FALSE(armed);
    CHECK(contains(armed.error().message, "offscreen frame: SDL_CreateGPUTexture: injected failure"));

    const auto after = draw(*renderer, list);
    REQUIRE(after);
    CHECK(after.value().rgba == baseline.value().rgba);
}

TEST_CASE("Renderer: offscreen frame failures return contextual errors and leave the renderer usable") {
    auto renderer = gpu_renderer();
    if (renderer == nullptr) {
        return;
    }
    const auto list = centred_quads(1);
    const auto baseline = draw(*renderer, list);
    REQUIRE(baseline);
    CHECK(baseline.value().width == kSide);
    CHECK(baseline.value().height == kSide);
    CHECK(centre_pixel(baseline.value()) == std::array<u8, 4>{255, 255, 255, 255});

    // Call order in one frame: texture, download buffer, command buffer, instance map and copy
    // pass, render pass, readback copy pass, fence wait, readback map.
    const std::array<FaultCase, 9> cases = {{
        {GpuFault::create_texture, 0, "offscreen frame: SDL_CreateGPUTexture"},
        {GpuFault::create_transfer_buffer, 0, "offscreen frame: SDL_CreateGPUTransferBuffer"},
        {GpuFault::acquire_command_buffer, 0, "offscreen frame: SDL_AcquireGPUCommandBuffer"},
        {GpuFault::map_transfer_buffer, 0, "offscreen frame: instance upload: SDL_MapGPUTransferBuffer"},
        {GpuFault::begin_copy_pass, 0, "offscreen frame: instance upload: SDL_BeginGPUCopyPass"},
        {GpuFault::begin_render_pass, 0, "offscreen frame: SDL_BeginGPURenderPass"},
        {GpuFault::begin_copy_pass, 1, "offscreen readback: SDL_BeginGPUCopyPass"},
        {GpuFault::wait, 0, "offscreen readback: SDL_WaitForGPUFences"},
        {GpuFault::map_transfer_buffer, 1, "offscreen readback: SDL_MapGPUTransferBuffer"},
    }};
    for (const auto& c : cases) {
        CAPTURE(c.context);
        renderer->inject_fault(c.fault, c.skip);
        const auto failed = draw(*renderer, list);
        REQUIRE_FALSE(failed);
        CHECK(failed.error().message == std::string(c.context) + ": injected failure");

        const auto recovered = draw(*renderer, list);
        REQUIRE(recovered);
        CHECK(recovered.value().rgba == baseline.value().rgba);
    }
}

TEST_CASE("Renderer: a failed atlas replacement keeps the previous atlas") {
    auto renderer = gpu_renderer();
    if (renderer == nullptr) {
        return;
    }
    const std::vector<u8> red = {255, 0, 0, 255};
    const std::vector<u8> green = {0, 255, 0, 255};
    REQUIRE(renderer->set_atlas(1, 1, red));
    const auto list = centred_quads(1);
    const auto baseline = draw(*renderer, list);
    REQUIRE(baseline);
    CHECK(centre_pixel(baseline.value()) == std::array<u8, 4>{255, 0, 0, 255});

    const std::array<FaultCase, 6> cases = {{
        {GpuFault::create_texture, 0, "atlas: SDL_CreateGPUTexture"},
        {GpuFault::create_transfer_buffer, 0, "atlas: SDL_CreateGPUTransferBuffer"},
        {GpuFault::map_transfer_buffer, 0, "atlas: SDL_MapGPUTransferBuffer"},
        {GpuFault::acquire_command_buffer, 0, "atlas: SDL_AcquireGPUCommandBuffer"},
        {GpuFault::begin_copy_pass, 0, "atlas: SDL_BeginGPUCopyPass"},
        {GpuFault::wait, 0, "atlas: SDL_WaitForGPUIdle"},
    }};
    for (const auto& c : cases) {
        CAPTURE(c.context);
        renderer->inject_fault(c.fault, c.skip);
        const auto failed = renderer->set_atlas(1, 1, green);
        REQUIRE_FALSE(failed);
        CHECK(failed.error().message == std::string(c.context) + ": injected failure");

        const auto still_red = draw(*renderer, list);
        REQUIRE(still_red);
        CHECK(still_red.value().rgba == baseline.value().rgba);
    }

    REQUIRE(renderer->set_atlas(1, 1, green));
    const auto replaced = draw(*renderer, list);
    REQUIRE(replaced);
    CHECK(centre_pixel(replaced.value()) == std::array<u8, 4>{0, 255, 0, 255});
}

TEST_CASE("Renderer: instance buffers stay usable after a failed growth") {
    auto renderer = gpu_renderer();
    if (renderer == nullptr) {
        return;
    }
    const auto small = centred_quads(1);
    const auto large = centred_quads(render::kInitialInstanceCapacity + 500); // needs a second growth
    const auto baseline = draw(*renderer, small);                             // first frame: capacity 1024
    REQUIRE(baseline);

    // Growth runs inside the frame after the download buffer was created, hence skip 1 for the
    // transfer buffer.
    const std::array<FaultCase, 2> cases = {{
        {GpuFault::create_buffer, 0, "offscreen frame: growing the instance buffer: SDL_CreateGPUBuffer"},
        {GpuFault::create_transfer_buffer, 1,
         "offscreen frame: growing the instance buffer: SDL_CreateGPUTransferBuffer"},
    }};
    for (const auto& c : cases) {
        CAPTURE(c.context);
        renderer->inject_fault(c.fault, c.skip);
        const auto failed = draw(*renderer, large);
        REQUIRE_FALSE(failed);
        CHECK(failed.error().message == std::string(c.context) + ": injected failure");

        const auto still_fits = draw(*renderer, small); // the previous buffers are still there
        REQUIRE(still_fits);
        CHECK(still_fits.value().rgba == baseline.value().rgba);
    }

    // Identical opaque quads drawn on top of each other give the single-quad image.
    const auto grown = draw(*renderer, large);
    REQUIRE(grown);
    CHECK(grown.value().rgba == baseline.value().rgba);
    const auto shrunk = draw(*renderer, small);
    REQUIRE(shrunk);
    CHECK(shrunk.value().rgba == baseline.value().rgba);
}
