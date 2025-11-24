#pragma once

#include <SDL2/SDL.h>
#include <memory>
#include <string>
#include <functional>

namespace lupine {

/**
 * Window class using SDL2 for cross-platform window management
 */
class Window {
public:
    using EventCallback = std::function<bool(const SDL_Event&)>;
    
    struct Config {
        std::string title = "Lupine Runtime";
        int width = 1280;
        int height = 720;
        bool resizable = true;
        bool vsync = true;
    };

    Window();
    ~Window();

    // Delete copy
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    /**
     * Create and initialize the window
     */
    bool create(const Config& config);

    /**
     * Destroy the window
     */
    void destroy();

    /**
     * Process window events (returns false if window should close)
     */
    bool processEvents();

    /**
     * Set a callback for processing SDL events
     * The callback should return true if it handled the event
     */
    void setEventCallback(EventCallback callback) { m_eventCallback = callback; }

    /**
     * Swap buffers (present)
     * Note: This is now handled by GfxDevice::present() and kept for API compatibility
     */
    void swapBuffers();

    /**
     * Check if window is open
     */
    bool isOpen() const { return m_window != nullptr; }

    /**
     * Get window dimensions
     */
    void getSize(int& width, int& height) const;

    /**
     * Get native window handle for rendering
     */
    void* getNativeHandle() const;

    /**
     * Get SDL window pointer
     */
    SDL_Window* getSDLWindow() const { return m_window; }

private:
    SDL_Window* m_window = nullptr;
    Config m_config;
    EventCallback m_eventCallback;
};

} // namespace lupine
