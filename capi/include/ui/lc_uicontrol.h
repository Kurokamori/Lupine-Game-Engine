/**
 * @file lc_uicontrol.h
 * @brief Lupine Engine C API - UIControl (shared UI layout base)
 *
 * Every UI component (ColorRect, Image2D, Panel, Button, Container, ...) derives
 * from the engine's UIControl base class, which owns the unified
 * layout model: size, anchors (anchorMin/anchorMax), offsets (offsetMin/offsetMax),
 * pivot, grow directions, container size flags, layout mode, and the UI-space flag.
 *
 * These functions operate on ANY UI component handle and provide a single,
 * uniform layout API across all UI component types.
 */

#ifndef LUPINE_CAPI_UICONTROL_H
#define LUPINE_CAPI_UICONTROL_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Anchor presets (mirror lupine::components::AnchorPreset).
 */
typedef enum LCAnchorPreset {
    LC_ANCHOR_PRESET_TOP_LEFT = 0,
    LC_ANCHOR_PRESET_TOP_CENTER = 1,
    LC_ANCHOR_PRESET_TOP_RIGHT = 2,
    LC_ANCHOR_PRESET_CENTER_LEFT = 3,
    LC_ANCHOR_PRESET_CENTER = 4,
    LC_ANCHOR_PRESET_CENTER_RIGHT = 5,
    LC_ANCHOR_PRESET_BOTTOM_LEFT = 6,
    LC_ANCHOR_PRESET_BOTTOM_CENTER = 7,
    LC_ANCHOR_PRESET_BOTTOM_RIGHT = 8,
    LC_ANCHOR_PRESET_LEFT_WIDE = 9,
    LC_ANCHOR_PRESET_TOP_WIDE = 10,
    LC_ANCHOR_PRESET_RIGHT_WIDE = 11,
    LC_ANCHOR_PRESET_BOTTOM_WIDE = 12,
    LC_ANCHOR_PRESET_VCENTER_WIDE = 13,
    LC_ANCHOR_PRESET_HCENTER_WIDE = 14,
    LC_ANCHOR_PRESET_FULL_RECT = 15,
    LC_ANCHOR_PRESET_CUSTOM = 16
} LCAnchorPreset;

/**
 * @brief Layout mode (mirror lupine::components::LayoutMode).
 */
typedef enum LCLayoutMode {
    LC_LAYOUT_MODE_POSITION = 0, /**< Free placement driven by the node position */
    LC_LAYOUT_MODE_ANCHORS = 1   /**< Driven by anchors + offsets */
} LCLayoutMode;

/**
 * @brief Grow direction (mirror lupine::components::GrowDirection).
 */
typedef enum LCGrowDirection {
    LC_GROW_DIRECTION_BEGIN = 0,
    LC_GROW_DIRECTION_END = 1,
    LC_GROW_DIRECTION_BOTH = 2
} LCGrowDirection;

/**
 * @brief Container size-flag bits (mirror lupine::components::SizeFlagBits).
 * Combine with bitwise OR. A value of 0 means "shrink to minimum at the begin edge".
 */
typedef enum LCSizeFlagBits {
    LC_SIZE_FLAG_SHRINK_BEGIN = 0,
    LC_SIZE_FLAG_FILL = 1,
    LC_SIZE_FLAG_EXPAND = 2,
    LC_SIZE_FLAG_SHRINK_CENTER = 4,
    LC_SIZE_FLAG_SHRINK_END = 8
} LCSizeFlagBits;

/**
 * @brief Mouse filter (mirror lupine::components::MouseFilter).
 *
 * Governs how a control reacts to the cursor. Only controls that expose the
 * "mouseFilter" property (ColorRect, Panel) are driven by it; every other control
 * reports LC_MOUSE_FILTER_IGNORE and rejects lc_uicontrol_set_mouse_filter with
 * LC_ERROR_INVALID_PARAMETER.
 */
typedef enum LCMouseFilter {
    LC_MOUSE_FILTER_STOP = 0,      /**< Handles the mouse AND blocks controls behind it */
    LC_MOUSE_FILTER_PROPAGATE = 1, /**< Handles the mouse but does not block controls behind it */
    LC_MOUSE_FILTER_IGNORE = 2     /**< Invisible to the cursor (default) */
} LCMouseFilter;

/**
 * @brief Mouse button indices used by the mouse-filter queries and signals.
 */
typedef enum LCUIMouseButton {
    LC_UI_MOUSE_BUTTON_LEFT = 0,
    LC_UI_MOUSE_BUTTON_RIGHT = 1,
    LC_UI_MOUSE_BUTTON_MIDDLE = 2
} LCUIMouseButton;

/* ============================================================================
 * Size
 * ============================================================================ */

LC_API LCResult lc_uicontrol_get_width(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_uicontrol_set_width(LCComponentHandle handle, float width);
LC_API LCResult lc_uicontrol_get_height(LCComponentHandle handle, float* outHeight);
LC_API LCResult lc_uicontrol_set_height(LCComponentHandle handle, float height);
LC_API LCResult lc_uicontrol_get_size(LCComponentHandle handle, LCVec2* outSize);
LC_API LCResult lc_uicontrol_set_size(LCComponentHandle handle, LCVec2 size);
LC_API LCResult lc_uicontrol_get_custom_min_size(LCComponentHandle handle, LCVec2* outSize);
LC_API LCResult lc_uicontrol_set_custom_min_size(LCComponentHandle handle, LCVec2 size);
LC_API LCResult lc_uicontrol_get_custom_max_size(LCComponentHandle handle, LCVec2* outSize);
LC_API LCResult lc_uicontrol_set_custom_max_size(LCComponentHandle handle, LCVec2 size);
LC_API LCResult lc_uicontrol_get_min_size(LCComponentHandle handle, LCVec2* outSize);
LC_API LCResult lc_uicontrol_get_max_size(LCComponentHandle handle, LCVec2* outSize);

/* ============================================================================
 * Anchors / offsets
 * ============================================================================ */

LC_API LCResult lc_uicontrol_get_anchor_min(LCComponentHandle handle, LCVec2* outAnchor);
LC_API LCResult lc_uicontrol_set_anchor_min(LCComponentHandle handle, LCVec2 anchor);
LC_API LCResult lc_uicontrol_get_anchor_max(LCComponentHandle handle, LCVec2* outAnchor);
LC_API LCResult lc_uicontrol_set_anchor_max(LCComponentHandle handle, LCVec2 anchor);
LC_API LCResult lc_uicontrol_get_offset_min(LCComponentHandle handle, LCVec2* outOffset);
LC_API LCResult lc_uicontrol_set_offset_min(LCComponentHandle handle, LCVec2 offset);
LC_API LCResult lc_uicontrol_get_offset_max(LCComponentHandle handle, LCVec2* outOffset);
LC_API LCResult lc_uicontrol_set_offset_max(LCComponentHandle handle, LCVec2 offset);

LC_API LCResult lc_uicontrol_get_anchor_preset(LCComponentHandle handle, int* outPreset);
LC_API LCResult lc_uicontrol_apply_anchor_preset(LCComponentHandle handle, int preset);

/* ============================================================================
 * Grow direction / layout mode
 *
 * REMOVED: lc_uicontrol_get_pivot / lc_uicontrol_set_pivot.
 * The engine's "pivot" property was never read by the layout solver -- every UI control is
 * center-pivoted -- so the pair only round-tripped a value that affected nothing. Callers
 * should delete their calls; there is no replacement, because there was never any behaviour.
 * ============================================================================ */

LC_API LCResult lc_uicontrol_get_layout_mode(LCComponentHandle handle, int* outMode);
LC_API LCResult lc_uicontrol_set_layout_mode(LCComponentHandle handle, int mode);
LC_API LCResult lc_uicontrol_get_grow_direction_h(LCComponentHandle handle, int* outDir);
LC_API LCResult lc_uicontrol_set_grow_direction_h(LCComponentHandle handle, int dir);
LC_API LCResult lc_uicontrol_get_grow_direction_v(LCComponentHandle handle, int* outDir);
LC_API LCResult lc_uicontrol_set_grow_direction_v(LCComponentHandle handle, int dir);

/* ============================================================================
 * Container size flags
 * ============================================================================ */

LC_API LCResult lc_uicontrol_get_size_flags_horizontal(LCComponentHandle handle, int* outFlags);
LC_API LCResult lc_uicontrol_set_size_flags_horizontal(LCComponentHandle handle, int flags);
LC_API LCResult lc_uicontrol_get_size_flags_vertical(LCComponentHandle handle, int* outFlags);
LC_API LCResult lc_uicontrol_set_size_flags_vertical(LCComponentHandle handle, int flags);
LC_API LCResult lc_uicontrol_get_size_flags_stretch_ratio(LCComponentHandle handle, float* outRatio);
LC_API LCResult lc_uicontrol_set_size_flags_stretch_ratio(LCComponentHandle handle, float ratio);

/* ============================================================================
 * UI space / resolved rect
 * ============================================================================ */

LC_API LCResult lc_uicontrol_get_ui_space(LCComponentHandle handle, bool* outUISpace);
LC_API LCResult lc_uicontrol_set_ui_space(LCComponentHandle handle, bool uiSpace);
LC_API LCResult lc_uicontrol_get_rect(LCComponentHandle handle, LCRect* outRect);

/* ============================================================================
 * Mouse filter / mouse state
 *
 * The mouse signals a filtered control emits are "mouse_entered", "mouse_exited",
 * "mouse_pressed" (button), "mouse_released" (button) and "clicked" (button); connect
 * to them through the signal API.
 * ============================================================================ */

LC_API LCResult lc_uicontrol_get_mouse_filter(LCComponentHandle handle, int* outFilter);
LC_API LCResult lc_uicontrol_set_mouse_filter(LCComponentHandle handle, int filter);
LC_API LCResult lc_uicontrol_is_mouse_hovered(LCComponentHandle handle, bool* outHovered);
LC_API LCResult lc_uicontrol_is_mouse_pressed(LCComponentHandle handle, int button, bool* outPressed);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_UICONTROL_H */
