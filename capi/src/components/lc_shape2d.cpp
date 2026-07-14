/**
 * @file lc_shape2d.cpp
 * @brief Implementation of Shape2D C API
 */

#include "components/lc_shape2d.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/Shape2D.hpp>

#include <cstring>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

Shape2D::ShapeType ToEngineShapeType(LCShape2DType type) {
    return static_cast<Shape2D::ShapeType>(type);
}

LCShape2DType FromEngineShapeType(Shape2D::ShapeType type) {
    return static_cast<LCShape2DType>(type);
}

Shape2D* GetShape2D(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<Shape2D*>(comp.get());
}

} // anonymous namespace

/* ============================================================================
 * Shape2D Creation
 * ============================================================================ */

LC_API LCResult lc_shape2d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = name ? std::make_shared<Shape2D>(name) : std::make_shared<Shape2D>();
        comp->RegisterProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create Shape2D component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Shape Type
 * ============================================================================ */

LC_API LCResult lc_shape2d_get_shape_type(LCComponentHandle component, LCShape2DType* out_type) {
    if (!out_type) return LC_ERROR_NULL_POINTER;
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    *out_type = FromEngineShapeType(shape->GetShapeType());
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_set_shape_type(LCComponentHandle component, LCShape2DType type) {
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    shape->SetShapeType(ToEngineShapeType(type));
    return LC_SUCCESS;
}

/* ============================================================================
 * Fill Properties
 * ============================================================================ */

LC_API LCResult lc_shape2d_get_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) return LC_ERROR_NULL_POINTER;
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    auto color = shape->GetColor();
    out_color->r = color.r;
    out_color->g = color.g;
    out_color->b = color.b;
    out_color->a = color.a;
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_set_color(LCComponentHandle component, LCColor color) {
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    shape->SetColor(math::Color(color.r, color.g, color.b, color.a));
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_get_filled(LCComponentHandle component, bool* out_filled) {
    if (!out_filled) return LC_ERROR_NULL_POINTER;
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    *out_filled = shape->GetFilled();
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_set_filled(LCComponentHandle component, bool filled) {
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    shape->SetFilled(filled);
    return LC_SUCCESS;
}

/* ============================================================================
 * Size Properties
 * ============================================================================ */

LC_API LCResult lc_shape2d_get_size(LCComponentHandle component, float* out_size) {
    if (!out_size) return LC_ERROR_NULL_POINTER;
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    *out_size = shape->GetSize();
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_set_size(LCComponentHandle component, float size) {
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    shape->SetSize(size);
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_get_width(LCComponentHandle component, float* out_width) {
    if (!out_width) return LC_ERROR_NULL_POINTER;
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    *out_width = shape->GetWidth();
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_set_width(LCComponentHandle component, float width) {
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    shape->SetWidth(width);
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_get_height(LCComponentHandle component, float* out_height) {
    if (!out_height) return LC_ERROR_NULL_POINTER;
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    *out_height = shape->GetHeight();
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_set_height(LCComponentHandle component, float height) {
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    shape->SetHeight(height);
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_get_radius(LCComponentHandle component, float* out_radius) {
    if (!out_radius) return LC_ERROR_NULL_POINTER;
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    *out_radius = shape->GetRadius();
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_set_radius(LCComponentHandle component, float radius) {
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    shape->SetRadius(radius);
    return LC_SUCCESS;
}

/* ============================================================================
 * Border Properties
 * ============================================================================ */

LC_API LCResult lc_shape2d_get_border_enabled(LCComponentHandle component, bool* out_enabled) {
    if (!out_enabled) return LC_ERROR_NULL_POINTER;
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    *out_enabled = shape->GetBorderEnabled();
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_set_border_enabled(LCComponentHandle component, bool enabled) {
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    shape->SetBorderEnabled(enabled);
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_get_border_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) return LC_ERROR_NULL_POINTER;
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    auto color = shape->GetBorderColor();
    out_color->r = color.r;
    out_color->g = color.g;
    out_color->b = color.b;
    out_color->a = color.a;
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_set_border_color(LCComponentHandle component, LCColor color) {
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    shape->SetBorderColor(math::Color(color.r, color.g, color.b, color.a));
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_get_border_width(LCComponentHandle component, float* out_width) {
    if (!out_width) return LC_ERROR_NULL_POINTER;
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    *out_width = shape->GetBorderWidth();
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_set_border_width(LCComponentHandle component, float width) {
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    shape->SetBorderWidth(width);
    return LC_SUCCESS;
}

/* ============================================================================
 * Circle Properties
 * ============================================================================ */

LC_API LCResult lc_shape2d_get_circle_segments(LCComponentHandle component, int* out_segments) {
    if (!out_segments) return LC_ERROR_NULL_POINTER;
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    *out_segments = shape->GetCircleSegments();
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_set_circle_segments(LCComponentHandle component, int segments) {
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    shape->SetCircleSegments(segments);
    return LC_SUCCESS;
}

/* ============================================================================
 * Rendering Properties
 * ============================================================================ */

LC_API LCResult lc_shape2d_get_layer(LCComponentHandle component, int* out_layer) {
    if (!out_layer) return LC_ERROR_NULL_POINTER;
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    *out_layer = shape->GetLayer();
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_set_layer(LCComponentHandle component, int layer) {
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    shape->SetLayer(layer);
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_get_sorting_order(LCComponentHandle component, int* out_order) {
    if (!out_order) return LC_ERROR_NULL_POINTER;
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    *out_order = shape->GetSortingOrder();
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_set_sorting_order(LCComponentHandle component, int order) {
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    shape->SetSortingOrder(order);
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_get_ui_space(LCComponentHandle component, bool* out_ui_space) {
    if (!out_ui_space) return LC_ERROR_NULL_POINTER;
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    *out_ui_space = shape->GetUISpace();
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_set_ui_space(LCComponentHandle component, bool ui_space) {
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    shape->SetUISpace(ui_space);
    return LC_SUCCESS;
}

/* ============================================================================
 * Shape2D Custom Shader (.lsh)
 * ============================================================================ */

LC_API LCResult lc_shape2d_get_shader(LCComponentHandle component, char* out_path, uint32_t max_length) {
    if (!out_path || max_length == 0) return LC_ERROR_NULL_POINTER;
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    const std::string& path = shape->GetShader();
    CopyStringToBuffer(out_path, max_length, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_set_shader(LCComponentHandle component, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    shape->SetShader(path);
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_get_shader_parameters(LCComponentHandle component, char* out_json, uint32_t max_length) {
    if (!out_json || max_length == 0) return LC_ERROR_NULL_POINTER;
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    const std::string& params = shape->GetShaderParameters();
    CopyStringToBuffer(out_json, max_length, params.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_shape2d_set_shader_parameters(LCComponentHandle component, const char* json) {
    if (!json) return LC_ERROR_NULL_POINTER;
    auto shape = GetShape2D(component);
    if (!shape) return LC_ERROR_INVALID_HANDLE;
    shape->SetShaderParameters(json);
    return LC_SUCCESS;
}
