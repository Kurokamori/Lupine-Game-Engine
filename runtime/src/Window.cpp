#include "lupine/runtime/Window.hpp"
#include "lupine/logger/Logger.hpp"
#include <SDL2/SDL_syswm.h>

namespace lupine {

Window::Window() = default;

Window::~Window() {
    destroy();
}

bool Window::create(const Config& config) {
    m_config = config;

    if (!SDL_WasInit(SDL_INIT_VIDEO)) {

        SDL_SetHint(SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4, "1");

        if (SDL_Init(SDL_INIT_VIDEO) != 0) {

            return false;
        }
    }

    uint32_t windowFlags = SDL_WINDOW_SHOWN;
    if (config.resizable) {
        windowFlags |= SDL_WINDOW_RESIZABLE;
    }
    if (config.fullscreen) {
        windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    }
    if (config.borderless) {
        windowFlags |= SDL_WINDOW_BORDERLESS;
    }

    // Enable high-DPI support for proper scaling on Retina/HiDPI displays
    // This makes SDL report window size in logical points, while rendering
    // uses the actual pixel size (which is larger on high-DPI displays)
    windowFlags |= SDL_WINDOW_ALLOW_HIGHDPI;

#ifdef __APPLE__
    // Metal requires SDL_WINDOW_METAL flag on macOS for proper CAMetalLayer support
    windowFlags |= SDL_WINDOW_METAL;
#endif

    m_window = SDL_CreateWindow(
        config.title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        config.width,
        config.height,
        windowFlags
    );

    if (!m_window) {

        SDL_Quit();
        return false;
    }

    // Enable OS text-input events (SDL_TEXTINPUT) so UI text fields receive
    // layout/IME-resolved characters. Harmless for games that don't use it.
    SDL_StartTextInput();

    return true;
}

void Window::destroy() {
    // Clear event callback to prevent callbacks during destruction
    m_eventCallback = nullptr;

    SDL_StopTextInput();

    if (m_window) {
#ifdef __APPLE__
        // On macOS, ensure any pending events are processed before destroying
        // This helps prevent crashes when Metal resources are being cleaned up
        SDL_PumpEvents();
#endif
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_Quit();
    }
}

bool Window::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {

        if (m_eventCallback && m_eventCallback(event)) {

            continue;
        }

        switch (event.type) {
            case SDL_QUIT:
                return false;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                    return false;
                }
                break;
        }
    }
    return true;
}

void Window::swapBuffers() {

}

void Window::getSize(int& width, int& height) const {
    if (m_window) {
        SDL_GetWindowSize(m_window, &width, &height);
    } else {
        width = 0;
        height = 0;
    }
}

void Window::setTitle(const std::string& title) {
    m_config.title = title;
    if (m_window) {
        SDL_SetWindowTitle(m_window, title.c_str());
    }
}

std::string Window::getTitle() const {
    if (m_window) {
        const char* title = SDL_GetWindowTitle(m_window);
        return title ? title : std::string();
    }
    return m_config.title;
}

void Window::setIcon(const unsigned char* rgbaPixels, int width, int height) {
    if (!m_window || !rgbaPixels || width <= 0 || height <= 0) {
        return;
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        const_cast<unsigned char*>(rgbaPixels),
        width, height, 32, width * 4,
        SDL_PIXELFORMAT_RGBA32);

    if (!surface) {
        LOG_WARN(LogCategory::Core, "Window: failed to create icon surface: {}", SDL_GetError());
        return;
    }

    SDL_SetWindowIcon(m_window, surface);
    SDL_FreeSurface(surface);
}

void Window::setFullscreen(bool fullscreen) {
    m_config.fullscreen = fullscreen;
    if (m_window) {
        SDL_SetWindowFullscreen(m_window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    }
}

bool Window::isFullscreen() const {
    if (m_window) {
        uint32_t flags = SDL_GetWindowFlags(m_window);
        return (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
    }
    return m_config.fullscreen;
}

void Window::setSize(int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    m_config.width = width;
    m_config.height = height;
    if (m_window) {
        SDL_SetWindowSize(m_window, width, height);
    }
}

void Window::maximize() {
    if (m_window) {
        SDL_MaximizeWindow(m_window);
    }
}

void Window::minimize() {
    if (m_window) {
        SDL_MinimizeWindow(m_window);
    }
}

void Window::restore() {
    if (m_window) {
        SDL_RestoreWindow(m_window);
    }
}

void Window::getScreenSize(int& width, int& height) const {
    SDL_DisplayMode mode;
    int displayIndex = m_window ? SDL_GetWindowDisplayIndex(m_window) : 0;
    if (displayIndex < 0) {
        displayIndex = 0;
    }
    if (SDL_GetCurrentDisplayMode(displayIndex, &mode) == 0) {
        width = mode.w;
        height = mode.h;
    } else {
        width = 0;
        height = 0;
    }
}

void Window::getDrawableSize(int& width, int& height) const {
    if (m_window) {
        // Get the actual drawable size in pixels (may differ from window size on high-DPI displays)
        // Note: SDL_GL_GetDrawableSize works for OpenGL contexts, but for other backends
        // we may need platform-specific handling. For now, use window size as fallback.
#if defined(SDL_VIDEO_DRIVER_COCOA) || defined(SDL_VIDEO_DRIVER_UIKIT)
        // On macOS/iOS, SDL_GL_GetDrawableSize gives actual pixel count
        SDL_GL_GetDrawableSize(m_window, &width, &height);
#elif defined(__EMSCRIPTEN__)
        // On web, the canvas element size is the drawable size
        SDL_GetWindowSize(m_window, &width, &height);
#else
        // On Windows/Linux, try GL drawable size, fall back to window size
        SDL_GL_GetDrawableSize(m_window, &width, &height);
        if (width == 0 || height == 0) {
            SDL_GetWindowSize(m_window, &width, &height);
        }
#endif
    } else {
        width = 0;
        height = 0;
    }
}

float Window::getDPIScale() const {
    int windowWidth, windowHeight;
    int drawableWidth, drawableHeight;

    getSize(windowWidth, windowHeight);
    getDrawableSize(drawableWidth, drawableHeight);

    if (windowWidth > 0 && drawableWidth > 0) {
        return static_cast<float>(drawableWidth) / static_cast<float>(windowWidth);
    }
    return 1.0f;
}

void* Window::getNativeHandle() const {
    if (!m_window) {
        LOG_ERROR(LogCategory::Core, "Window::getNativeHandle: m_window is null");
        return nullptr;
    }

#ifdef __EMSCRIPTEN__
    // Emscripten/WebGL doesn't use native window handles
    // The WebGL context is created directly on the canvas element
    // Return nullptr - WebGL device will create context on "#canvas"
    return nullptr;
#else
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);

    if (!SDL_GetWindowWMInfo(m_window, &wmInfo)) {
        LOG_ERROR(LogCategory::Core, "Window::getNativeHandle: SDL_GetWindowWMInfo failed: {}", SDL_GetError());
        return nullptr;
    }

#ifdef _WIN32
    return wmInfo.info.win.window;
#elif defined(__APPLE__)

    return wmInfo.info.cocoa.window;
#elif defined(__linux__)
    return reinterpret_cast<void*>(wmInfo.info.x11.window);
#else
    return nullptr;
#endif
#endif // __EMSCRIPTEN__
}

}
