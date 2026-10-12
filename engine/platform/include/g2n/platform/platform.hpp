#pragma once

#include <memory>
#include <string>

#include "g2n/core/result.hpp"
#include "g2n/core/vec.hpp"

struct SDL_Window;

namespace g2n::platform {

/// SDL_Init / SDL_Quit scope. Create exactly one, before any other platform object.
class SdlRuntime {
public:
    /// SDL_GPU needs the video subsystem even without a window. With windowed=false on a
    /// Linux host without X11/Wayland, SDL's "offscreen" video driver is used (Vulkan only).
    static Result<std::unique_ptr<SdlRuntime>> create(bool windowed);
    ~SdlRuntime();
    SdlRuntime(const SdlRuntime&) = delete;
    SdlRuntime& operator=(const SdlRuntime&) = delete;
    SdlRuntime(SdlRuntime&&) = delete;
    SdlRuntime& operator=(SdlRuntime&&) = delete;

private:
    SdlRuntime() = default;
};

class Window {
public:
    static Result<std::unique_ptr<Window>> create(const std::string& title, int width, int height);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] SDL_Window* handle() const { return window_; }
    [[nodiscard]] Vec2f size_pixels() const;
    void set_title(const std::string& title);

private:
    explicit Window(SDL_Window* window) : window_(window) {}
    SDL_Window* window_;
};

/// Keyboard/window state relevant to the game, sampled once per frame.
struct InputSnapshot {
    bool quit = false;
    bool up = false, down = false, left = false, right = false;
    bool attack = false, jump = false, enter_exit = false;
    float zoom_steps = 0; // mouse wheel / +/- keys this frame
};

/// Drains the SDL event queue and samples the keyboard.
[[nodiscard]] InputSnapshot poll_input();

[[nodiscard]] u64 now_ns();

/// Writes RGBA8 pixels as a BMP file (uses SDL surfaces; no video subsystem needed).
Result<bool> save_rgba_bmp(const std::string& path, u32 width, u32 height, const u8* rgba);

} // namespace g2n::platform
