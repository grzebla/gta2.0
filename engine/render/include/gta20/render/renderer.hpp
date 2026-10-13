#pragma once

#include <memory>
#include <span>
#include <string>
#include <vector>

#include "gta20/core/camera2d.hpp"
#include "gta20/core/result.hpp"
#include "gta20/scene/draw_list.hpp"

struct SDL_Window;

namespace gta20::render {

struct ClearColor {
    float r = 0.08f, g = 0.08f, b = 0.1f, a = 1.0f;
};

struct RendererOptions {
    bool gpu_debug = false;                 // SDL_GPU validation (needs validation layers on Vulkan)
    const char* preferred_driver = nullptr; // nullptr = platform default (metal / direct3d12 / vulkan)
};

/// SDL_GPU calls whose failure the renderer handles. Tests make one of them fail on purpose
/// with Renderer::inject_fault to exercise the error paths without a faulty device.
enum class GpuFault : u8 {
    create_texture,         // SDL_CreateGPUTexture
    create_buffer,          // SDL_CreateGPUBuffer
    create_transfer_buffer, // SDL_CreateGPUTransferBuffer
    map_transfer_buffer,    // SDL_MapGPUTransferBuffer
    acquire_command_buffer, // SDL_AcquireGPUCommandBuffer
    begin_copy_pass,        // SDL_BeginGPUCopyPass
    begin_render_pass,      // SDL_BeginGPURenderPass
    wait,                   // SDL_WaitForGPUFences, SDL_WaitForGPUIdle
};

/// RGBA8 image read back from the GPU.
struct Image {
    u32 width = 0;
    u32 height = 0;
    std::vector<u8> rgba;
};

/// SDL_GPU renderer for instanced quads. Owns every GPU object it creates and
/// releases them in the destructor (RAII). One draw call per frame.
class Renderer {
public:
    /// window may be null for offscreen-only use (tests, screenshots, CI).
    static Result<std::unique_ptr<Renderer>> create(SDL_Window* window, const RendererOptions& options);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    /// "metal", "direct3d12" or "vulkan".
    [[nodiscard]] std::string backend() const;

    /// Uploads the RGBA8 texture atlas sampled by every quad (replaces the previous one).
    /// Until called, a 1x1 white texture is bound so untextured quads render as flat colours.
    /// On failure the previous atlas stays bound.
    Result<bool> set_atlas(u32 width, u32 height, std::span<const u8> rgba);

    /// Renders to the window's swapchain. Returns false when no swapchain image is
    /// available (e.g. minimised) - not an error.
    Result<bool> render_to_window(const scene::DrawList& list, const Camera2D& camera, ClearColor clear);

    /// Renders to an offscreen RGBA8 texture and reads the pixels back.
    Result<Image> render_offscreen(const scene::DrawList& list, const Camera2D& camera, u32 width, u32 height,
                                   ClearColor clear);

    /// Test seam: the (skip + 1)-th following call of `fault` fails as if SDL had failed (null or
    /// false, SDL error "injected failure"); the calls before it run normally. One fault is armed
    /// at a time; arming another replaces it.
    void inject_fault(GpuFault fault, u32 skip = 0);

    struct Impl;

private:
    explicit Renderer(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace gta20::render
