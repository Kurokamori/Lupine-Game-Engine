/**
 * @file lc_theme.cpp
 * @brief Implementation of Lupine Engine C API - UI Theme
 */

#include "ui/lc_theme.h"
#include "../core/lc_internal.h"

#include <lupine/ui/ThemeManager.hpp>
#include <lupine/math/Color.hpp>

#include <cstring>
#include <string>

using lupine::math::Color;

namespace {

void SetThemeError(LCResult code, const char* message) {
    ::SetError(code, message);
}

lupine::ui::ThemeManager& Mgr() {
    return lupine::ui::ThemeManager::GetInstance();
}

LCColor FromEngineColor(const Color& color) {
    return LCColor{color.r, color.g, color.b, color.a};
}

Color ToEngineColor(const LCColor& color) {
    return Color(color.r, color.g, color.b, color.a);
}

} // anonymous namespace

LC_API LCResult lc_theme_set_active(const char* res_path) {
    if (!res_path) {
        SetThemeError(LC_ERROR_NULL_POINTER, "res_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        Mgr().SetActiveTheme(res_path);
        return LC_SUCCESS;
    } catch (...) {
        SetThemeError(LC_ERROR_INTERNAL_ERROR, "lc_theme_set_active failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_theme_get_color(const char* type, const char* variation,
                                   const char* entry, LCColor fallback,
                                   LCColor* out_color) {
    if (!type || !entry || !out_color) {
        SetThemeError(LC_ERROR_NULL_POINTER, "type, entry, or out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_color = FromEngineColor(Mgr().GetColor(type, variation ? variation : "",
                                                    entry, ToEngineColor(fallback)));
        return LC_SUCCESS;
    } catch (...) {
        SetThemeError(LC_ERROR_INTERNAL_ERROR, "lc_theme_get_color failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_theme_get_constant(const char* type, const char* variation,
                                      const char* entry, float fallback,
                                      float* out_value) {
    if (!type || !entry || !out_value) {
        SetThemeError(LC_ERROR_NULL_POINTER, "type, entry, or out_value is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_value = Mgr().GetConstant(type, variation ? variation : "", entry, fallback);
        return LC_SUCCESS;
    } catch (...) {
        SetThemeError(LC_ERROR_INTERNAL_ERROR, "lc_theme_get_constant failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_theme_set_palette_color(const char* key, LCColor color) {
    if (!key) {
        SetThemeError(LC_ERROR_NULL_POINTER, "key is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        Mgr().SetPaletteColor(key, ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetThemeError(LC_ERROR_INTERNAL_ERROR, "lc_theme_set_palette_color failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_theme_set_variable(const char* key, float value) {
    if (!key) {
        SetThemeError(LC_ERROR_NULL_POINTER, "key is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        Mgr().SetVariable(key, value);
        return LC_SUCCESS;
    } catch (...) {
        SetThemeError(LC_ERROR_INTERNAL_ERROR, "lc_theme_set_variable failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_theme_get_version(unsigned long long* out_version) {
    if (!out_version) {
        SetThemeError(LC_ERROR_NULL_POINTER, "out_version is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_version = (unsigned long long)Mgr().Version();
        return LC_SUCCESS;
    } catch (...) {
        SetThemeError(LC_ERROR_INTERNAL_ERROR, "lc_theme_get_version failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
