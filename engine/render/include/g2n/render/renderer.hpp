#pragma once

#include <memory>
#include <string>
#include <vector>

#include "g2n/core/camera2d.hpp"
#include "g2n/core/result.hpp"
#include "g2n/scene/draw_list.hpp"

struct SDL_Window;

namespace g2n::render {

struct ClearColor {
    float r = 0.08f, g = 0.08f, b = 0.1f, a = 1.0f;
};

struct RendererOptions {
    bool gpu_debug = false;                 // SDL_GPU validation (needs validation layers on Vulkan)
    const char* preferred_driver = nullptr; // nullptr = platform default (metal / direct3d12 / vulkan)
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

    /// Renders to the window's swapchain. Returns false when no swapchain image is
    /// available (e.g. minimised) - not an error.
    Result<bool> render_to_window(const scene::DrawList& list, const Camera2D& camera, ClearColor clear);

    /// Renders to an offscreen RGBA8 texture and reads the pixels back.
    Result<Image> render_offscreen(const scene::DrawList& list, const Camera2D& camera, u32 width, u32 height,
                                   ClearColor clear);

    struct Impl;

private:
    explicit Renderer(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace g2n::render
