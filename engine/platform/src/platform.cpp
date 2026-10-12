#include "gta20/platform/platform.hpp"

#include <SDL3/SDL.h>

namespace gta20::platform {
namespace {
std::string sdl_error(const char* what) {
    return std::string(what) + ": " + SDL_GetError();
}
} // namespace

Result<std::unique_ptr<SdlRuntime>> SdlRuntime::create(bool windowed) {
#if defined(__linux__)
    // Headless Linux (CI): no X11/Wayland server. SDL's offscreen video driver still
    // supports Vulkan; Metal (macOS) and D3D12 (Windows) need the native driver instead.
    if (!windowed && SDL_getenv("DISPLAY") == nullptr && SDL_getenv("WAYLAND_DISPLAY") == nullptr) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    }
#else
    (void)windowed;
#endif
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        return make_error(sdl_error("SDL_Init"));
    }
    return std::unique_ptr<SdlRuntime>(new SdlRuntime());
}

SdlRuntime::~SdlRuntime() {
    SDL_Quit();
}

Result<std::unique_ptr<Window>> Window::create(const std::string& title, int width, int height) {
    SDL_Window* window =
        SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) {
        return make_error(sdl_error("SDL_CreateWindow"));
    }
    return std::unique_ptr<Window>(new Window(window));
}

Window::~Window() {
    SDL_DestroyWindow(window_);
}

Vec2f Window::size_pixels() const {
    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(window_, &w, &h);
    return {static_cast<float>(w), static_cast<float>(h)};
}

void Window::set_title(const std::string& title) {
    SDL_SetWindowTitle(window_, title.c_str());
}

InputSnapshot poll_input() {
    InputSnapshot s;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT: s.quit = true; break;
        case SDL_EVENT_MOUSE_WHEEL: s.zoom_steps += event.wheel.y; break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.key == SDLK_ESCAPE)
                s.quit = true;
            if (event.key.key == SDLK_EQUALS || event.key.key == SDLK_KP_PLUS)
                s.zoom_steps += 1;
            if (event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_MINUS)
                s.zoom_steps -= 1;
            break;
        default: break;
        }
    }
    const bool* keys = SDL_GetKeyboardState(nullptr);
    s.up = keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W];
    s.down = keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S];
    s.left = keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A];
    s.right = keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D];
    s.attack = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL];
    s.jump = keys[SDL_SCANCODE_SPACE];
    s.enter_exit = keys[SDL_SCANCODE_RETURN];
    s.toggle_camera = keys[SDL_SCANCODE_TAB];
    return s;
}

u64 now_ns() {
    return SDL_GetTicksNS();
}

Result<bool> save_rgba_bmp(const std::string& path, u32 width, u32 height, const u8* rgba) {
    // RGBA byte order in memory == SDL_PIXELFORMAT_ABGR8888 on little-endian hosts.
    SDL_Surface* surface =
        SDL_CreateSurfaceFrom(static_cast<int>(width), static_cast<int>(height), SDL_PIXELFORMAT_RGBA32,
                              const_cast<u8*>(rgba), static_cast<int>(width * 4));
    if (surface == nullptr) {
        return make_error(sdl_error("SDL_CreateSurfaceFrom"));
    }
    const bool saved = SDL_SaveBMP(surface, path.c_str());
    SDL_DestroySurface(surface);
    return saved ? Result<bool>(true) : Result<bool>(make_error(sdl_error("SDL_SaveBMP")));
}

} // namespace gta20::platform
