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

    return true;
}

void Window::destroy() {
    if (m_window) {
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

void* Window::getNativeHandle() const {
    if (!m_window) {
        return nullptr;
    }

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);

    if (!SDL_GetWindowWMInfo(m_window, &wmInfo)) {

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
}

}
