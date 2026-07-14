/**
 * @file lc_nine_slice_panel.cpp
 * @brief Implementation of NineSlicePanel C API
 */

#include "ui/lc_nine_slice_panel.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/NineSlicePanel.hpp>
#include <cstring>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

NineSlicePanel* GetNineSlicePanel(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<NineSlicePanel*>(comp.get());
}

} // anonymous namespace

/* ============================================================================
 * NineSlicePanel Creation
 * ============================================================================ */

LC_API LCResult lc_nine_slice_panel_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = name ? std::make_shared<NineSlicePanel>(name) : std::make_shared<NineSlicePanel>();
        comp->RegisterProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create NineSlicePanel component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Size Properties
 * ============================================================================ */

LC_API LCResult lc_nine_slice_panel_get_width(LCComponentHandle component, float* out_width) {
    if (!out_width) return LC_ERROR_NULL_POINTER;
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    *out_width = panel->GetWidth();
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_set_width(LCComponentHandle component, float width) {
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    panel->SetWidth(width);
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_get_height(LCComponentHandle component, float* out_height) {
    if (!out_height) return LC_ERROR_NULL_POINTER;
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    *out_height = panel->GetHeight();
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_set_height(LCComponentHandle component, float height) {
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    panel->SetHeight(height);
    return LC_SUCCESS;
}

/* ============================================================================
 * Texture Properties
 * ============================================================================ */

LC_API LCResult lc_nine_slice_panel_get_texture_path(LCComponentHandle component, char* out_buffer, size_t buffer_size) {
    if (!out_buffer) return LC_ERROR_NULL_POINTER;
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;

    std::string path = panel->GetTexturePath();
    CopyStringToBuffer(out_buffer, buffer_size, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_set_texture_path(LCComponentHandle component, const char* path) {
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    panel->SetTexturePath(path ? path : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_get_modulate(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) return LC_ERROR_NULL_POINTER;
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;

    auto color = panel->GetModulate();
    out_color->r = color.r;
    out_color->g = color.g;
    out_color->b = color.b;
    out_color->a = color.a;
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_set_modulate(LCComponentHandle component, LCColor color) {
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    panel->SetModulate(math::Color(color.r, color.g, color.b, color.a));
    return LC_SUCCESS;
}

/* ============================================================================
 * Nine-Slice Margins
 * ============================================================================ */

LC_API LCResult lc_nine_slice_panel_get_margin_left(LCComponentHandle component, float* out_margin) {
    if (!out_margin) return LC_ERROR_NULL_POINTER;
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    *out_margin = panel->GetMarginLeft();
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_set_margin_left(LCComponentHandle component, float margin) {
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    panel->SetMarginLeft(margin);
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_get_margin_right(LCComponentHandle component, float* out_margin) {
    if (!out_margin) return LC_ERROR_NULL_POINTER;
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    *out_margin = panel->GetMarginRight();
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_set_margin_right(LCComponentHandle component, float margin) {
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    panel->SetMarginRight(margin);
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_get_margin_top(LCComponentHandle component, float* out_margin) {
    if (!out_margin) return LC_ERROR_NULL_POINTER;
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    *out_margin = panel->GetMarginTop();
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_set_margin_top(LCComponentHandle component, float margin) {
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    panel->SetMarginTop(margin);
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_get_margin_bottom(LCComponentHandle component, float* out_margin) {
    if (!out_margin) return LC_ERROR_NULL_POINTER;
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    *out_margin = panel->GetMarginBottom();
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_set_margin_bottom(LCComponentHandle component, float margin) {
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    panel->SetMarginBottom(margin);
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_set_margins(LCComponentHandle component, float left, float right, float top, float bottom) {
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    panel->SetMarginLeft(left);
    panel->SetMarginRight(right);
    panel->SetMarginTop(top);
    panel->SetMarginBottom(bottom);
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_set_margins_uniform(LCComponentHandle component, float margin) {
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    panel->SetMarginLeft(margin);
    panel->SetMarginRight(margin);
    panel->SetMarginTop(margin);
    panel->SetMarginBottom(margin);
    return LC_SUCCESS;
}

/* ============================================================================
 * Rendering Properties
 * ============================================================================ */

LC_API LCResult lc_nine_slice_panel_get_layer(LCComponentHandle component, int* out_layer) {
    if (!out_layer) return LC_ERROR_NULL_POINTER;
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    *out_layer = panel->GetLayer();
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_set_layer(LCComponentHandle component, int layer) {
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    panel->SetLayer(layer);
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_get_sorting_order(LCComponentHandle component, int* out_order) {
    if (!out_order) return LC_ERROR_NULL_POINTER;
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    *out_order = panel->GetSortingOrder();
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_set_sorting_order(LCComponentHandle component, int order) {
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    panel->SetSortingOrder(order);
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_get_use_ui_space(LCComponentHandle component, bool* out_use_ui_space) {
    if (!out_use_ui_space) return LC_ERROR_NULL_POINTER;
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    *out_use_ui_space = panel->GetUseUISpace();
    return LC_SUCCESS;
}

LC_API LCResult lc_nine_slice_panel_set_use_ui_space(LCComponentHandle component, bool use_ui_space) {
    auto panel = GetNineSlicePanel(component);
    if (!panel) return LC_ERROR_INVALID_HANDLE;
    panel->SetUseUISpace(use_ui_space);
    return LC_SUCCESS;
}
