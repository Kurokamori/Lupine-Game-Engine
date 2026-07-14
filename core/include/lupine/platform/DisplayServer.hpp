#pragma once

#include "lupine/math/Vec2.hpp"
#include <functional>
#include <string>

namespace lupine {
namespace platform {

/**
 * Mouse interaction mode (Godot-style).
 *
 * Visible        - normal cursor, free movement.
 * Hidden         - cursor hidden, free movement.
 * Captured       - cursor hidden and locked to the window centre; motion is
 *                  reported as relative deltas (first-person / mouse-look).
 * Confined       - cursor visible but clamped to the window bounds.
 * ConfinedHidden - cursor hidden and clamped to the window bounds.
 */
enum class MouseMode {
    Visible = 0,
    Hidden = 1,
    Captured = 2,
    Confined = 3,
    ConfinedHidden = 4
};

/**
 * DisplayServer - engine-facing window/display control.
 *
 * Window and display management is inherently platform-specific (SDL on desktop,
 * the browser on web, etc.), so the platform/runtime layer installs a provider
 * and core stays free of any windowing dependency - mirroring the clipboard and
 * vibration provider pattern in input::InputManager.
 *
 * When no provider is installed every setter is a no-op and every getter returns
 * the last cached value (or a sensible default), so scripts and the editor behave
 * predictably in headless contexts.
 */
class DisplayServer {
public:
    static DisplayServer& Get();

    /**
     * The set of platform callbacks the runtime installs. Any individual callback
     * may be left empty; the corresponding operation then falls back to the cached
     * state. Installed together via SetProvider().
     */
    struct Provider {
        std::function<void(const std::string&)> setTitle;
        std::function<std::string()> getTitle;

        std::function<void(bool)> setFullscreen;
        std::function<bool()> isFullscreen;

        std::function<void(bool)> setVSync;
        std::function<bool()> isVSync;

        std::function<void(int, int)> setWindowSize;
        std::function<math::Vec2()> getWindowSize;

        std::function<void()> maximizeWindow;
        std::function<void()> minimizeWindow;
        std::function<void()> restoreWindow;

        std::function<math::Vec2()> getScreenSize;

        std::function<void(MouseMode)> setMouseMode;

        std::function<bool(const std::string&)> openURL;
    };

    /**
     * Install (or clear, with a default-constructed Provider) the platform backend.
     * Called by the runtime during initialization and cleared at shutdown so any
     * captured pointers cannot dangle past the backend's lifetime.
     */
    void SetProvider(const Provider& provider);
    void ClearProvider();
    bool HasProvider() const { return m_HasProvider; }

    // Window title.
    void SetWindowTitle(const std::string& title);
    std::string GetWindowTitle() const;

    // Fullscreen (borderless desktop fullscreen).
    void SetFullscreen(bool fullscreen);
    bool IsFullscreen() const;

    // Vertical sync. On backends where this requires a swapchain rebuild the
    // provider performs it; the call is synchronous.
    void SetVSync(bool enabled);
    bool IsVSync() const;

    // Window client size in logical pixels.
    void SetWindowSize(int width, int height);
    math::Vec2 GetWindowSize() const;

    // Window state.
    void MaximizeWindow();
    void MinimizeWindow();
    void RestoreWindow();

    // Primary display resolution in pixels.
    math::Vec2 GetScreenSize() const;

    // Mouse interaction mode (see MouseMode).
    void SetMouseMode(MouseMode mode);
    MouseMode GetMouseMode() const;

    // Open a URL/URI in the user's default handler (browser, mail client, ...).
    // Returns false when no backend is installed or the platform refused it.
    bool OpenURL(const std::string& url);

    // Convenience cursor visibility wrappers over the mouse mode. Showing the
    // cursor restores Visible; hiding it selects Hidden (both leave any
    // confine/capture intent unchanged only insofar as they map to these modes).
    void SetCursorVisible(bool visible);
    bool IsCursorVisible() const;

private:
    DisplayServer() = default;

    Provider m_Provider;
    bool m_HasProvider = false;

    // Cached state - the source of truth when no provider is installed and the
    // value reported by getters whose provider callback is empty.
    std::string m_Title = "Lupine";
    bool m_Fullscreen = false;
    bool m_VSync = true;
    MouseMode m_MouseMode = MouseMode::Visible;
    math::Vec2 m_WindowSize = math::Vec2(0.0f, 0.0f);
    math::Vec2 m_ScreenSize = math::Vec2(0.0f, 0.0f);
};

} // namespace platform
} // namespace lupine
