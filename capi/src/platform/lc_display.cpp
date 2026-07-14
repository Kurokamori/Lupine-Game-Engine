
#include "platform/lc_display.h"
#include "../core/lc_internal.h"

#include <lupine/platform/DisplayServer.hpp>

#include <cstring>
#include <string>

namespace {

void SetDisplayError(LCResult code, const char* message) {
    ::SetError(code, message);
}

lupine::platform::MouseMode ToMouseMode(LCMouseMode mode) {
    return static_cast<lupine::platform::MouseMode>(static_cast<int>(mode));
}

LCMouseMode FromMouseMode(lupine::platform::MouseMode mode) {
    return static_cast<LCMouseMode>(static_cast<int>(mode));
}

} // anonymous namespace

/* ============================================================================
 * Window Title
 * ============================================================================ */

LC_API LCResult lc_display_set_window_title(const char* title) {
    if (!title) {
        SetDisplayError(LC_ERROR_NULL_POINTER, "title is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        lupine::platform::DisplayServer::Get().SetWindowTitle(std::string(title));
        return LC_SUCCESS;
    } catch (...) {
        SetDisplayError(LC_ERROR_INTERNAL_ERROR, "Failed to set window title");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_display_get_window_title(char* out_buffer, size_t buffer_size) {
    if (!out_buffer) {
        SetDisplayError(LC_ERROR_NULL_POINTER, "out_buffer is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (buffer_size == 0) {
        SetDisplayError(LC_ERROR_INVALID_PARAMETER, "buffer_size is 0");
        return LC_ERROR_INVALID_PARAMETER;
    }

    try {
        std::string title = lupine::platform::DisplayServer::Get().GetWindowTitle();
        CopyStringToBuffer(out_buffer, buffer_size, title.c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetDisplayError(LC_ERROR_INTERNAL_ERROR, "Failed to get window title");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Fullscreen / VSync
 * ============================================================================ */

LC_API void lc_display_set_fullscreen(bool fullscreen) {
    try {
        lupine::platform::DisplayServer::Get().SetFullscreen(fullscreen);
    } catch (...) {
    }
}

LC_API bool lc_display_is_fullscreen(void) {
    try {
        return lupine::platform::DisplayServer::Get().IsFullscreen();
    } catch (...) {
        return false;
    }
}

LC_API void lc_display_set_vsync(bool enabled) {
    try {
        lupine::platform::DisplayServer::Get().SetVSync(enabled);
    } catch (...) {
    }
}

LC_API bool lc_display_is_vsync(void) {
    try {
        return lupine::platform::DisplayServer::Get().IsVSync();
    } catch (...) {
        return false;
    }
}

/* ============================================================================
 * Window Size / State
 * ============================================================================ */

LC_API void lc_display_set_window_size(int width, int height) {
    try {
        lupine::platform::DisplayServer::Get().SetWindowSize(width, height);
    } catch (...) {
    }
}

LC_API LCResult lc_display_get_window_size(LCVec2* out_size) {
    if (!out_size) {
        SetDisplayError(LC_ERROR_NULL_POINTER, "out_size is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto size = lupine::platform::DisplayServer::Get().GetWindowSize();
        out_size->x = size.x;
        out_size->y = size.y;
        return LC_SUCCESS;
    } catch (...) {
        SetDisplayError(LC_ERROR_INTERNAL_ERROR, "Failed to get window size");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_display_get_screen_size(LCVec2* out_size) {
    if (!out_size) {
        SetDisplayError(LC_ERROR_NULL_POINTER, "out_size is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto size = lupine::platform::DisplayServer::Get().GetScreenSize();
        out_size->x = size.x;
        out_size->y = size.y;
        return LC_SUCCESS;
    } catch (...) {
        SetDisplayError(LC_ERROR_INTERNAL_ERROR, "Failed to get screen size");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API void lc_display_maximize_window(void) {
    try {
        lupine::platform::DisplayServer::Get().MaximizeWindow();
    } catch (...) {
    }
}

LC_API void lc_display_minimize_window(void) {
    try {
        lupine::platform::DisplayServer::Get().MinimizeWindow();
    } catch (...) {
    }
}

LC_API void lc_display_restore_window(void) {
    try {
        lupine::platform::DisplayServer::Get().RestoreWindow();
    } catch (...) {
    }
}

/* ============================================================================
 * Mouse Mode / Cursor
 * ============================================================================ */

LC_API void lc_display_set_mouse_mode(LCMouseMode mode) {
    try {
        lupine::platform::DisplayServer::Get().SetMouseMode(ToMouseMode(mode));
    } catch (...) {
    }
}

LC_API LCMouseMode lc_display_get_mouse_mode(void) {
    try {
        return FromMouseMode(lupine::platform::DisplayServer::Get().GetMouseMode());
    } catch (...) {
        return LC_MOUSE_MODE_VISIBLE;
    }
}

LC_API void lc_display_set_mouse_cursor_visible(bool visible) {
    try {
        lupine::platform::DisplayServer::Get().SetCursorVisible(visible);
    } catch (...) {
    }
}

LC_API bool lc_display_is_mouse_cursor_visible(void) {
    try {
        return lupine::platform::DisplayServer::Get().IsCursorVisible();
    } catch (...) {
        return false;
    }
}
