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
        bool fullscreen = false;
        bool borderless = false;
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
     * Get window dimensions in logical/screen coordinates
     */
    void getSize(int& width, int& height) const;

    /**
     * Window title.
     */
    void setTitle(const std::string& title);
    std::string getTitle() const;

    /**
     * Set the window icon (taskbar + title bar) from raw RGBA8 pixel data.
     * Rows must be top-down; pixels are not retained after the call returns.
     */
    void setIcon(const unsigned char* rgbaPixels, int width, int height);

    /**
     * Borderless desktop fullscreen toggle and query.
     */
    void setFullscreen(bool fullscreen);
    bool isFullscreen() const;

    /**
     * Set the window client size in logical pixels.
     */
    void setSize(int width, int height);

    /**
     * Window state control.
     */
    void maximize();
    void minimize();
    void restore();

    /**
     * Resolution of the display the window is on, in pixels.
     */
    void getScreenSize(int& width, int& height) const;

    /**
     * Get drawable size in actual pixels (for high-DPI displays)
     * On high-DPI displays, this may be larger than getSize()
     */
    void getDrawableSize(int& width, int& height) const;

    /**
     * Get the DPI scale factor (drawable size / window size)
     * Returns 1.0 on standard displays, 2.0 on Retina/HiDPI displays, etc.
     */
    float getDPIScale() const;

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
