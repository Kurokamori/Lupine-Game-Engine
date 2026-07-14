#include "lupine/platform/DisplayServer.hpp"

namespace lupine {
namespace platform {

DisplayServer& DisplayServer::Get() {
    static DisplayServer instance;
    return instance;
}

void DisplayServer::SetProvider(const Provider& provider) {
    m_Provider = provider;
    m_HasProvider = true;
}

void DisplayServer::ClearProvider() {
    m_Provider = Provider();
    m_HasProvider = false;
}

void DisplayServer::SetWindowTitle(const std::string& title) {
    m_Title = title;
    if (m_Provider.setTitle) {
        m_Provider.setTitle(title);
    }
}

std::string DisplayServer::GetWindowTitle() const {
    if (m_Provider.getTitle) {
        return m_Provider.getTitle();
    }
    return m_Title;
}

void DisplayServer::SetFullscreen(bool fullscreen) {
    m_Fullscreen = fullscreen;
    if (m_Provider.setFullscreen) {
        m_Provider.setFullscreen(fullscreen);
    }
}

bool DisplayServer::IsFullscreen() const {
    if (m_Provider.isFullscreen) {
        return m_Provider.isFullscreen();
    }
    return m_Fullscreen;
}

void DisplayServer::SetVSync(bool enabled) {
    m_VSync = enabled;
    if (m_Provider.setVSync) {
        m_Provider.setVSync(enabled);
    }
}

bool DisplayServer::IsVSync() const {
    if (m_Provider.isVSync) {
        return m_Provider.isVSync();
    }
    return m_VSync;
}

void DisplayServer::SetWindowSize(int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    m_WindowSize = math::Vec2(static_cast<float>(width), static_cast<float>(height));
    if (m_Provider.setWindowSize) {
        m_Provider.setWindowSize(width, height);
    }
}

math::Vec2 DisplayServer::GetWindowSize() const {
    if (m_Provider.getWindowSize) {
        return m_Provider.getWindowSize();
    }
    return m_WindowSize;
}

void DisplayServer::MaximizeWindow() {
    if (m_Provider.maximizeWindow) {
        m_Provider.maximizeWindow();
    }
}

void DisplayServer::MinimizeWindow() {
    if (m_Provider.minimizeWindow) {
        m_Provider.minimizeWindow();
    }
}

void DisplayServer::RestoreWindow() {
    if (m_Provider.restoreWindow) {
        m_Provider.restoreWindow();
    }
}

math::Vec2 DisplayServer::GetScreenSize() const {
    if (m_Provider.getScreenSize) {
        return m_Provider.getScreenSize();
    }
    return m_ScreenSize;
}

void DisplayServer::SetMouseMode(MouseMode mode) {
    m_MouseMode = mode;
    if (m_Provider.setMouseMode) {
        m_Provider.setMouseMode(mode);
    }
}

MouseMode DisplayServer::GetMouseMode() const {
    return m_MouseMode;
}

bool DisplayServer::OpenURL(const std::string& url) {
    if (url.empty()) {
        return false;
    }
    if (m_Provider.openURL) {
        return m_Provider.openURL(url);
    }
    return false;
}

void DisplayServer::SetCursorVisible(bool visible) {
    SetMouseMode(visible ? MouseMode::Visible : MouseMode::Hidden);
}

bool DisplayServer::IsCursorVisible() const {
    return m_MouseMode == MouseMode::Visible || m_MouseMode == MouseMode::Confined;
}

} // namespace platform
} // namespace lupine
