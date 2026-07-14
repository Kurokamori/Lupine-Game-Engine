/**
 * @file lc_container.h
 * @brief Lupine Engine C API - UI Container Components
 *
 * This header provides Container components for UI layout:
 * - Container: Base container with size, padding, margin, background, borders
 * - VerticalContainer: Arranges children vertically
 * - HorizontalContainer: Arranges children horizontally
 * - GridContainer: Arranges children in a grid
 * - CenterContainer: Centers children within bounds
 */

#ifndef LUPINE_CAPI_CONTAINER_H
#define LUPINE_CAPI_CONTAINER_H

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
 * @brief Size mode for containers
 */
typedef enum LCContainerSizeMode {
    LC_CONTAINER_SIZE_FIXED = 0,        /**< Use explicit width/height */
    LC_CONTAINER_SIZE_FIT_CHILDREN = 1, /**< Size to fit all children */
    LC_CONTAINER_SIZE_EXPAND = 2,       /**< Expand to fill available space */
    LC_CONTAINER_SIZE_MINIMUM = 3       /**< Use minimum required size */
} LCContainerSizeMode;

/**
 * @brief Horizontal alignment for VerticalContainer
 */
typedef enum LCVerticalContainerHAlign {
    LC_VCONTAINER_HALIGN_LEFT = 0,
    LC_VCONTAINER_HALIGN_CENTER = 1,
    LC_VCONTAINER_HALIGN_RIGHT = 2,
    LC_VCONTAINER_HALIGN_FILL = 3
} LCVerticalContainerHAlign;

/**
 * @brief Vertical alignment for VerticalContainer
 */
typedef enum LCVerticalContainerVAlign {
    LC_VCONTAINER_VALIGN_BEGIN = 0,
    LC_VCONTAINER_VALIGN_CENTER = 1,
    LC_VCONTAINER_VALIGN_END = 2,
    LC_VCONTAINER_VALIGN_FILL = 3
} LCVerticalContainerVAlign;

/**
 * @brief Vertical alignment for HorizontalContainer
 */
typedef enum LCHorizontalContainerVAlign {
    LC_HCONTAINER_VALIGN_TOP = 0,
    LC_HCONTAINER_VALIGN_CENTER = 1,
    LC_HCONTAINER_VALIGN_BOTTOM = 2,
    LC_HCONTAINER_VALIGN_FILL = 3
} LCHorizontalContainerVAlign;

/**
 * @brief Horizontal alignment for HorizontalContainer
 */
typedef enum LCHorizontalContainerHAlign {
    LC_HCONTAINER_HALIGN_BEGIN = 0,
    LC_HCONTAINER_HALIGN_CENTER = 1,
    LC_HCONTAINER_HALIGN_END = 2,
    LC_HCONTAINER_HALIGN_FILL = 3
} LCHorizontalContainerHAlign;

/**
 * @brief Cell sizing mode for GridContainer
 */
typedef enum LCGridCellSizingMode {
    LC_GRID_CELL_AUTOMATIC = 0,              /**< Fit largest child in each cell */
    LC_GRID_CELL_AUTO_HEIGHT_FIXED_WIDTH = 1,/**< Automatic height, fixed width */
    LC_GRID_CELL_AUTO_WIDTH_FIXED_HEIGHT = 2,/**< Automatic width, fixed height */
    LC_GRID_CELL_FIXED = 3                   /**< Fixed width and height */
} LCGridCellSizingMode;

/**
 * @brief Flow direction for GridContainer
 */
typedef enum LCGridFlowDirection {
    LC_GRID_FLOW_LEFT_TO_RIGHT = 0,
    LC_GRID_FLOW_RIGHT_TO_LEFT = 1,
    LC_GRID_FLOW_TOP_TO_BOTTOM = 2,
    LC_GRID_FLOW_BOTTOM_TO_TOP = 3
} LCGridFlowDirection;

/* ============================================================================
 * Container Base Component Functions
 * ============================================================================ */

/**
 * @brief Create a Container component on a node
 * @param node Node handle to attach the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_container_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Size Properties ----- */

LC_API LCResult lc_container_get_width(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_container_set_width(LCComponentHandle handle, float width);
LC_API LCResult lc_container_get_height(LCComponentHandle handle, float* outHeight);
LC_API LCResult lc_container_set_height(LCComponentHandle handle, float height);
LC_API LCResult lc_container_get_size(LCComponentHandle handle, LCVec2* outSize);
LC_API LCResult lc_container_set_size(LCComponentHandle handle, LCVec2 size);

LC_API LCResult lc_container_get_min_size(LCComponentHandle handle, LCVec2* outSize);
LC_API LCResult lc_container_set_min_size(LCComponentHandle handle, LCVec2 size);
LC_API LCResult lc_container_get_max_size(LCComponentHandle handle, LCVec2* outSize);
LC_API LCResult lc_container_set_max_size(LCComponentHandle handle, LCVec2 size);

LC_API LCResult lc_container_get_horizontal_size_mode(LCComponentHandle handle, LCContainerSizeMode* outMode);
LC_API LCResult lc_container_set_horizontal_size_mode(LCComponentHandle handle, LCContainerSizeMode mode);
LC_API LCResult lc_container_get_vertical_size_mode(LCComponentHandle handle, LCContainerSizeMode* outMode);
LC_API LCResult lc_container_set_vertical_size_mode(LCComponentHandle handle, LCContainerSizeMode mode);

/* ----- Padding ----- */

LC_API LCResult lc_container_get_padding_linked(LCComponentHandle handle, bool* outLinked);
LC_API LCResult lc_container_set_padding_linked(LCComponentHandle handle, bool linked);
LC_API LCResult lc_container_get_padding_left(LCComponentHandle handle, float* outPadding);
LC_API LCResult lc_container_set_padding_left(LCComponentHandle handle, float padding);
LC_API LCResult lc_container_get_padding_right(LCComponentHandle handle, float* outPadding);
LC_API LCResult lc_container_set_padding_right(LCComponentHandle handle, float padding);
LC_API LCResult lc_container_get_padding_top(LCComponentHandle handle, float* outPadding);
LC_API LCResult lc_container_set_padding_top(LCComponentHandle handle, float padding);
LC_API LCResult lc_container_get_padding_bottom(LCComponentHandle handle, float* outPadding);
LC_API LCResult lc_container_set_padding_bottom(LCComponentHandle handle, float padding);
LC_API LCResult lc_container_get_padding(LCComponentHandle handle, LCVec4* outPadding);
LC_API LCResult lc_container_set_padding(LCComponentHandle handle, LCVec4 padding);

/* ----- Margin ----- */

LC_API LCResult lc_container_get_margin_linked(LCComponentHandle handle, bool* outLinked);
LC_API LCResult lc_container_set_margin_linked(LCComponentHandle handle, bool linked);
LC_API LCResult lc_container_get_margin_left(LCComponentHandle handle, float* outMargin);
LC_API LCResult lc_container_set_margin_left(LCComponentHandle handle, float margin);
LC_API LCResult lc_container_get_margin_right(LCComponentHandle handle, float* outMargin);
LC_API LCResult lc_container_set_margin_right(LCComponentHandle handle, float margin);
LC_API LCResult lc_container_get_margin_top(LCComponentHandle handle, float* outMargin);
LC_API LCResult lc_container_set_margin_top(LCComponentHandle handle, float margin);
LC_API LCResult lc_container_get_margin_bottom(LCComponentHandle handle, float* outMargin);
LC_API LCResult lc_container_set_margin_bottom(LCComponentHandle handle, float margin);
LC_API LCResult lc_container_get_margin(LCComponentHandle handle, LCVec4* outMargin);
LC_API LCResult lc_container_set_margin(LCComponentHandle handle, LCVec4 margin);

/* ----- Background ----- */

LC_API LCResult lc_container_get_background_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_container_set_background_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_container_get_opacity(LCComponentHandle handle, float* outOpacity);
LC_API LCResult lc_container_set_opacity(LCComponentHandle handle, float opacity);
LC_API LCResult lc_container_get_draw_background(LCComponentHandle handle, bool* outDraw);
LC_API LCResult lc_container_set_draw_background(LCComponentHandle handle, bool draw);

/* ----- Border ----- */

LC_API LCResult lc_container_get_border_enabled(LCComponentHandle handle, bool* outEnabled);
LC_API LCResult lc_container_set_border_enabled(LCComponentHandle handle, bool enabled);
LC_API LCResult lc_container_get_border_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_container_set_border_color(LCComponentHandle handle, LCColor color);

LC_API LCResult lc_container_get_border_width_linked(LCComponentHandle handle, bool* outLinked);
LC_API LCResult lc_container_set_border_width_linked(LCComponentHandle handle, bool linked);
LC_API LCResult lc_container_get_border_width_left(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_container_set_border_width_left(LCComponentHandle handle, float width);
LC_API LCResult lc_container_get_border_width_right(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_container_set_border_width_right(LCComponentHandle handle, float width);
LC_API LCResult lc_container_get_border_width_top(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_container_set_border_width_top(LCComponentHandle handle, float width);
LC_API LCResult lc_container_get_border_width_bottom(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_container_set_border_width_bottom(LCComponentHandle handle, float width);

/* ----- Corner Radius ----- */

LC_API LCResult lc_container_get_corner_radius_linked(LCComponentHandle handle, bool* outLinked);
LC_API LCResult lc_container_set_corner_radius_linked(LCComponentHandle handle, bool linked);
LC_API LCResult lc_container_get_corner_radius_top_left(LCComponentHandle handle, float* outRadius);
LC_API LCResult lc_container_set_corner_radius_top_left(LCComponentHandle handle, float radius);
LC_API LCResult lc_container_get_corner_radius_top_right(LCComponentHandle handle, float* outRadius);
LC_API LCResult lc_container_set_corner_radius_top_right(LCComponentHandle handle, float radius);
LC_API LCResult lc_container_get_corner_radius_bottom_left(LCComponentHandle handle, float* outRadius);
LC_API LCResult lc_container_set_corner_radius_bottom_left(LCComponentHandle handle, float radius);
LC_API LCResult lc_container_get_corner_radius_bottom_right(LCComponentHandle handle, float* outRadius);
LC_API LCResult lc_container_set_corner_radius_bottom_right(LCComponentHandle handle, float radius);

/* ----- Layout ----- */

LC_API LCResult lc_container_get_clip_children(LCComponentHandle handle, bool* outClip);
LC_API LCResult lc_container_set_clip_children(LCComponentHandle handle, bool clip);
LC_API LCResult lc_container_get_separation(LCComponentHandle handle, float* outSeparation);
LC_API LCResult lc_container_set_separation(LCComponentHandle handle, float separation);
LC_API LCResult lc_container_invalidate_layout(LCComponentHandle handle);
LC_API LCResult lc_container_force_layout_update(LCComponentHandle handle);

/* ----- Rendering ----- */

LC_API LCResult lc_container_get_layer(LCComponentHandle handle, int* outLayer);
LC_API LCResult lc_container_set_layer(LCComponentHandle handle, int layer);
LC_API LCResult lc_container_get_sorting_order(LCComponentHandle handle, int* outOrder);
LC_API LCResult lc_container_set_sorting_order(LCComponentHandle handle, int order);
LC_API LCResult lc_container_get_use_ui_space(LCComponentHandle handle, bool* outUseUISpace);
LC_API LCResult lc_container_set_use_ui_space(LCComponentHandle handle, bool useUISpace);

/* ----- Child Info ----- */

LC_API LCResult lc_container_get_child_count(LCComponentHandle handle, int* outCount);

/* ============================================================================
 * VerticalContainer Component Functions
 * ============================================================================ */

/**
 * @brief Create a VerticalContainer component on a node
 * @param node Node handle to attach the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_vertical_container_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Alignment ----- */

LC_API LCResult lc_vertical_container_get_horizontal_alignment(LCComponentHandle handle, LCVerticalContainerHAlign* outAlign);
LC_API LCResult lc_vertical_container_set_horizontal_alignment(LCComponentHandle handle, LCVerticalContainerHAlign align);
LC_API LCResult lc_vertical_container_get_vertical_alignment(LCComponentHandle handle, LCVerticalContainerVAlign* outAlign);
LC_API LCResult lc_vertical_container_set_vertical_alignment(LCComponentHandle handle, LCVerticalContainerVAlign align);

/* ============================================================================
 * HorizontalContainer Component Functions
 * ============================================================================ */

/**
 * @brief Create a HorizontalContainer component on a node
 * @param node Node handle to attach the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_horizontal_container_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Alignment ----- */

LC_API LCResult lc_horizontal_container_get_vertical_alignment(LCComponentHandle handle, LCHorizontalContainerVAlign* outAlign);
LC_API LCResult lc_horizontal_container_set_vertical_alignment(LCComponentHandle handle, LCHorizontalContainerVAlign align);
LC_API LCResult lc_horizontal_container_get_horizontal_alignment(LCComponentHandle handle, LCHorizontalContainerHAlign* outAlign);
LC_API LCResult lc_horizontal_container_set_horizontal_alignment(LCComponentHandle handle, LCHorizontalContainerHAlign align);

/* ============================================================================
 * GridContainer Component Functions
 * ============================================================================ */

/**
 * @brief Create a GridContainer component on a node
 * @param node Node handle to attach the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_grid_container_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Grid Configuration ----- */

LC_API LCResult lc_grid_container_get_use_rows(LCComponentHandle handle, bool* outUseRows);
LC_API LCResult lc_grid_container_set_use_rows(LCComponentHandle handle, bool useRows);
LC_API LCResult lc_grid_container_get_row_count(LCComponentHandle handle, int* outCount);
LC_API LCResult lc_grid_container_set_row_count(LCComponentHandle handle, int count);
LC_API LCResult lc_grid_container_get_column_count(LCComponentHandle handle, int* outCount);
LC_API LCResult lc_grid_container_set_column_count(LCComponentHandle handle, int count);

/* ----- Cell Sizing ----- */

LC_API LCResult lc_grid_container_get_cell_sizing_mode(LCComponentHandle handle, LCGridCellSizingMode* outMode);
LC_API LCResult lc_grid_container_set_cell_sizing_mode(LCComponentHandle handle, LCGridCellSizingMode mode);
LC_API LCResult lc_grid_container_get_fixed_cell_width(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_grid_container_set_fixed_cell_width(LCComponentHandle handle, float width);
LC_API LCResult lc_grid_container_get_fixed_cell_height(LCComponentHandle handle, float* outHeight);
LC_API LCResult lc_grid_container_set_fixed_cell_height(LCComponentHandle handle, float height);

/* ----- Spacing ----- */

LC_API LCResult lc_grid_container_get_horizontal_spacing(LCComponentHandle handle, float* outSpacing);
LC_API LCResult lc_grid_container_set_horizontal_spacing(LCComponentHandle handle, float spacing);
LC_API LCResult lc_grid_container_get_vertical_spacing(LCComponentHandle handle, float* outSpacing);
LC_API LCResult lc_grid_container_set_vertical_spacing(LCComponentHandle handle, float spacing);

/* ----- Flow Direction ----- */

LC_API LCResult lc_grid_container_get_flow_direction(LCComponentHandle handle, LCGridFlowDirection* outDirection);
LC_API LCResult lc_grid_container_set_flow_direction(LCComponentHandle handle, LCGridFlowDirection direction);

/* ----- Cell Options ----- */

LC_API LCResult lc_grid_container_get_homogeneous_cells(LCComponentHandle handle, bool* outHomogeneous);
LC_API LCResult lc_grid_container_set_homogeneous_cells(LCComponentHandle handle, bool homogeneous);
LC_API LCResult lc_grid_container_get_size_children(LCComponentHandle handle, bool* outSizeChildren);
LC_API LCResult lc_grid_container_set_size_children(LCComponentHandle handle, bool sizeChildren);

/* ============================================================================
 * CenterContainer Component Functions
 * ============================================================================ */

/* Note: CenterContainer functions are defined in lc_center_container.h */
/* This file only provides forward-compatible declarations */

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_CONTAINER_H */
