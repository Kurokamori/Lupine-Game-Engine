/**
 * @file lc_theme.h
 * @brief Lupine Engine C API - UI Theme
 *
 * Resolve themed values (colors and constants) from the active UI theme, switch
 * the active theme, mutate theme tokens (palette colors / variables) at runtime,
 * and query the theme version.
 *
 * Resolution follows: type variation -> base type ('extends' chain) -> base theme
 * ('extends' chain) -> the caller-supplied fallback. The fallback is returned
 * unchanged when no theme is loaded or the entry is not defined.
 */

#ifndef LUPINE_CAPI_THEME_H
#define LUPINE_CAPI_THEME_H

#include "core/lc_core.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Active theme
 * ============================================================================ */

/**
 * @brief Load (if needed) and activate the theme at the given resource path.
 * @param res_path Theme resource path (e.g. res://default.uitheme).
 * @return LC_SUCCESS on success.
 */
LC_API LCResult lc_theme_set_active(const char* res_path);

/* ============================================================================
 * Resolution
 * ============================================================================ */

/**
 * @brief Resolve a themed color, returning the fallback when not defined.
 * @param type Component type (e.g. "Button").
 * @param variation Type variation (may be NULL for none).
 * @param entry Entry name (e.g. "font_color").
 * @param fallback Color returned when the entry is not defined.
 * @param out_color Receives the resolved color.
 * @return LC_SUCCESS on success.
 */
LC_API LCResult lc_theme_get_color(const char* type, const char* variation,
                                   const char* entry, LCColor fallback,
                                   LCColor* out_color);

/**
 * @brief Resolve a themed constant, returning the fallback when not defined.
 * @param type Component type (e.g. "Button").
 * @param variation Type variation (may be NULL for none).
 * @param entry Entry name (e.g. "corner_radius").
 * @param fallback Value returned when the entry is not defined.
 * @param out_value Receives the resolved value.
 * @return LC_SUCCESS on success.
 */
LC_API LCResult lc_theme_get_constant(const char* type, const char* variation,
                                      const char* entry, float fallback,
                                      float* out_value);

/* ============================================================================
 * Runtime token edits
 * ============================================================================ */

/**
 * @brief Set a palette color token on the active theme (bumps the version).
 */
LC_API LCResult lc_theme_set_palette_color(const char* key, LCColor color);

/**
 * @brief Set a variable token on the active theme (bumps the version).
 */
LC_API LCResult lc_theme_set_variable(const char* key, float value);

/* ============================================================================
 * Version
 * ============================================================================ */

/**
 * @brief Get the monotonically increasing active-theme version.
 * @param out_version Receives the current version.
 * @return LC_SUCCESS on success.
 */
LC_API LCResult lc_theme_get_version(unsigned long long* out_version);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_THEME_H */
