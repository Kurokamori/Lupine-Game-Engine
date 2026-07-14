/**
 * @file lc_panel.cpp
 * @brief Implementation of Lupine Engine C API - Panel and Panel3D Components
 */

#include "ui/lc_panel.h"
#include "../core/lc_internal.h"

#include <lupine/components/Panel.hpp>
#include <lupine/components/Panel3D.hpp>
#include <lupine/core/Node.hpp>

#include <cstring>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetPanelError(LCResult code, const char* message) {
    ::SetError(code, message);
}

Panel* GetPanel(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<Panel*>(component.get());
}

Panel3D* GetPanel3D(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<Panel3D*>(component.get());
}

LCColor FromEngineColor(const Color& color) {
    return LCColor{color.r, color.g, color.b, color.a};
}

Color ToEngineColor(const LCColor& color) {
    return Color(color.r, color.g, color.b, color.a);
}

LCVec2 FromEngineVec2(const Vec2& vec) {
    return LCVec2{vec.x, vec.y};
}

Vec2 ToEngineVec2(const LCVec2& vec) {
    return Vec2(vec.x, vec.y);
}

} // anonymous namespace

/* ============================================================================
 * Panel Component
 * ============================================================================ */

LC_API LCResult lc_panel_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetPanelError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto component = std::make_shared<Panel>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetPanelError(LC_ERROR_INTERNAL_ERROR, "Failed to create Panel component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Size Properties ----- */

LC_API LCResult lc_panel_get_width(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetPanelError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = panel->GetWidth(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_width(LCComponentHandle handle, float width) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetWidth(width); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_height(LCComponentHandle handle, float* outHeight) {
    if (!outHeight) { SetPanelError(LC_ERROR_NULL_POINTER, "outHeight is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outHeight = panel->GetHeight(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_height(LCComponentHandle handle, float height) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetHeight(height); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Layer Properties ----- */

LC_API LCResult lc_panel_get_layer(LCComponentHandle handle, int* outLayer) {
    if (!outLayer) { SetPanelError(LC_ERROR_NULL_POINTER, "outLayer is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLayer = panel->GetLayer(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetLayer failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_layer(LCComponentHandle handle, int layer) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetLayer(layer); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetLayer failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_sorting_order(LCComponentHandle handle, int* outOrder) {
    if (!outOrder) { SetPanelError(LC_ERROR_NULL_POINTER, "outOrder is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outOrder = panel->GetSortingOrder(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetSortingOrder failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_sorting_order(LCComponentHandle handle, int order) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetSortingOrder(order); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetSortingOrder failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- UI Space ----- */

LC_API LCResult lc_panel_get_use_ui_space(LCComponentHandle handle, bool* outUseUISpace) {
    if (!outUseUISpace) { SetPanelError(LC_ERROR_NULL_POINTER, "outUseUISpace is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outUseUISpace = panel->GetUseUISpace(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetUseUISpace failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_use_ui_space(LCComponentHandle handle, bool useUISpace) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetUseUISpace(useUISpace); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetUseUISpace failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- StyleBox File ----- */

LC_API LCResult lc_panel_load_style(LCComponentHandle handle, const char* filepath) {
    if (!filepath) { SetPanelError(LC_ERROR_NULL_POINTER, "filepath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        if (panel->LoadStyleFromFile(filepath)) return LC_SUCCESS;
        SetPanelError(LC_ERROR_FILE_NOT_FOUND, "Failed to load style file");
        return LC_ERROR_FILE_NOT_FOUND;
    } catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "LoadStyleFromFile failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_save_style(LCComponentHandle handle, const char* filepath) {
    if (!filepath) { SetPanelError(LC_ERROR_NULL_POINTER, "filepath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        if (panel->SaveStyleToFile(filepath)) return LC_SUCCESS;
        SetPanelError(LC_ERROR_INTERNAL_ERROR, "Failed to save style file");
        return LC_ERROR_INTERNAL_ERROR;
    } catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SaveStyleToFile failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_style_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetPanelError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = panel->GetStylePath();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetStylePath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_style_path(LCComponentHandle handle, const char* path) {
    if (!path) { SetPanelError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetStylePath(path); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetStylePath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Background ----- */

LC_API LCResult lc_panel_get_background_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetPanelError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(panel->GetBackgroundColor()); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_background_color(LCComponentHandle handle, LCColor color) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetBackgroundColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_opacity(LCComponentHandle handle, float* outOpacity) {
    if (!outOpacity) { SetPanelError(LC_ERROR_NULL_POINTER, "outOpacity is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outOpacity = panel->GetOpacity(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetOpacity failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_opacity(LCComponentHandle handle, float opacity) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetOpacity(opacity); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetOpacity failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Border Width ----- */

LC_API LCResult lc_panel_get_border_width_linked(LCComponentHandle handle, bool* outLinked) {
    if (!outLinked) { SetPanelError(LC_ERROR_NULL_POINTER, "outLinked is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLinked = panel->GetBorderWidthLinked(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidthLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_border_width_linked(LCComponentHandle handle, bool linked) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetBorderWidthLinked(linked); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_border_width_left(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetPanelError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = panel->GetBorderWidthLeft(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidthLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_border_width_left(LCComponentHandle handle, float width) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetBorderWidthLeft(width); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_border_width_right(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetPanelError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = panel->GetBorderWidthRight(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidthRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_border_width_right(LCComponentHandle handle, float width) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetBorderWidthRight(width); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_border_width_top(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetPanelError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = panel->GetBorderWidthTop(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidthTop failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_border_width_top(LCComponentHandle handle, float width) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetBorderWidthTop(width); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthTop failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_border_width_bottom(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetPanelError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = panel->GetBorderWidthBottom(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidthBottom failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_border_width_bottom(LCComponentHandle handle, float width) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetBorderWidthBottom(width); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthBottom failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Border Enabled/Color ----- */

LC_API LCResult lc_panel_get_border_enabled(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) { SetPanelError(LC_ERROR_NULL_POINTER, "outEnabled is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outEnabled = panel->GetBorderEnabled(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetBorderEnabled failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_border_enabled(LCComponentHandle handle, bool enabled) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetBorderEnabled(enabled); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetBorderEnabled failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_border_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetPanelError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(panel->GetBorderColor()); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetBorderColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_border_color(LCComponentHandle handle, LCColor color) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetBorderColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetBorderColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Corner Radius ----- */

LC_API LCResult lc_panel_get_corner_radius_linked(LCComponentHandle handle, bool* outLinked) {
    if (!outLinked) { SetPanelError(LC_ERROR_NULL_POINTER, "outLinked is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLinked = panel->GetCornerRadiusLinked(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadiusLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_corner_radius_linked(LCComponentHandle handle, bool linked) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetCornerRadiusLinked(linked); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_corner_radius_top_left(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) { SetPanelError(LC_ERROR_NULL_POINTER, "outRadius is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outRadius = panel->GetCornerRadiusTopLeft(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadiusTopLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_corner_radius_top_left(LCComponentHandle handle, float radius) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetCornerRadiusTopLeft(radius); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusTopLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_corner_radius_top_right(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) { SetPanelError(LC_ERROR_NULL_POINTER, "outRadius is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outRadius = panel->GetCornerRadiusTopRight(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadiusTopRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_corner_radius_top_right(LCComponentHandle handle, float radius) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetCornerRadiusTopRight(radius); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusTopRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_corner_radius_bottom_left(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) { SetPanelError(LC_ERROR_NULL_POINTER, "outRadius is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outRadius = panel->GetCornerRadiusBottomLeft(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadiusBottomLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_corner_radius_bottom_left(LCComponentHandle handle, float radius) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetCornerRadiusBottomLeft(radius); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusBottomLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_corner_radius_bottom_right(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) { SetPanelError(LC_ERROR_NULL_POINTER, "outRadius is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outRadius = panel->GetCornerRadiusBottomRight(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadiusBottomRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_corner_radius_bottom_right(LCComponentHandle handle, float radius) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetCornerRadiusBottomRight(radius); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusBottomRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Corner Detail & Anti-Aliasing ----- */

LC_API LCResult lc_panel_get_corner_detail(LCComponentHandle handle, int* outDetail) {
    if (!outDetail) { SetPanelError(LC_ERROR_NULL_POINTER, "outDetail is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outDetail = panel->GetCornerDetail(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetCornerDetail failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_corner_detail(LCComponentHandle handle, int detail) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetCornerDetail(detail); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetCornerDetail failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_anti_aliasing(LCComponentHandle handle, bool* outAA) {
    if (!outAA) { SetPanelError(LC_ERROR_NULL_POINTER, "outAA is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outAA = panel->GetAntiAliasing(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetAntiAliasing failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_anti_aliasing(LCComponentHandle handle, bool aa) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetAntiAliasing(aa); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetAntiAliasing failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_anti_aliasing_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) { SetPanelError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = panel->GetAntiAliasingSize(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetAntiAliasingSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_anti_aliasing_size(LCComponentHandle handle, float size) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetAntiAliasingSize(size); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetAntiAliasingSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Shadow ----- */

LC_API LCResult lc_panel_get_shadow_enabled(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) { SetPanelError(LC_ERROR_NULL_POINTER, "outEnabled is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outEnabled = panel->GetShadowEnabled(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetShadowEnabled failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_shadow_enabled(LCComponentHandle handle, bool enabled) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetShadowEnabled(enabled); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetShadowEnabled failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_shadow_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetPanelError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(panel->GetShadowColor()); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetShadowColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_shadow_color(LCComponentHandle handle, LCColor color) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetShadowColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetShadowColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_shadow_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) { SetPanelError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = panel->GetShadowSize(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetShadowSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_shadow_size(LCComponentHandle handle, float size) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetShadowSize(size); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetShadowSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_shadow_offset(LCComponentHandle handle, LCVec2* outOffset) {
    if (!outOffset) { SetPanelError(LC_ERROR_NULL_POINTER, "outOffset is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outOffset = FromEngineVec2(panel->GetShadowOffset()); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetShadowOffset failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_shadow_offset(LCComponentHandle handle, LCVec2 offset) {
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetShadowOffset(ToEngineVec2(offset)); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetShadowOffset failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Custom Shader ----- */

LC_API LCResult lc_panel_get_custom_shader_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetPanelError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = panel->GetCustomShaderPath();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetCustomShaderPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_custom_shader_path(LCComponentHandle handle, const char* path) {
    if (!path) { SetPanelError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetCustomShaderPath(path); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetCustomShaderPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ============================================================================
 * Panel3D Component
 * ============================================================================ */

LC_API LCResult lc_panel3d_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetPanelError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto component = std::make_shared<Panel3D>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetPanelError(LC_ERROR_INTERNAL_ERROR, "Failed to create Panel3D component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Size Properties ----- */

LC_API LCResult lc_panel3d_get_width(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetPanelError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = panel->GetWidth(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_width(LCComponentHandle handle, float width) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetWidth(width); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_height(LCComponentHandle handle, float* outHeight) {
    if (!outHeight) { SetPanelError(LC_ERROR_NULL_POINTER, "outHeight is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outHeight = panel->GetHeight(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_height(LCComponentHandle handle, float height) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetHeight(height); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- 3D Rendering Properties ----- */

LC_API LCResult lc_panel3d_get_billboard_mode(LCComponentHandle handle, LCPanel3DBillboardMode* outMode) {
    if (!outMode) { SetPanelError(LC_ERROR_NULL_POINTER, "outMode is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        auto mode = panel->GetBillboardMode();
        *outMode = static_cast<LCPanel3DBillboardMode>(static_cast<int>(mode));
        return LC_SUCCESS;
    } catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetBillboardMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_billboard_mode(LCComponentHandle handle, LCPanel3DBillboardMode mode) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        panel->SetBillboardMode(static_cast<Panel3DBillboardMode>(static_cast<int>(mode)));
        return LC_SUCCESS;
    } catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetBillboardMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_double_sided(LCComponentHandle handle, bool* outDoubleSided) {
    if (!outDoubleSided) { SetPanelError(LC_ERROR_NULL_POINTER, "outDoubleSided is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outDoubleSided = panel->GetDoubleSided(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetDoubleSided failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_double_sided(LCComponentHandle handle, bool doubleSided) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetDoubleSided(doubleSided); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetDoubleSided failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_cast_shadow(LCComponentHandle handle, bool* outCastShadow) {
    if (!outCastShadow) { SetPanelError(LC_ERROR_NULL_POINTER, "outCastShadow is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outCastShadow = panel->GetCastShadow(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetCastShadow failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_cast_shadow(LCComponentHandle handle, bool castShadow) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetCastShadow(castShadow); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetCastShadow failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_receive_shadow(LCComponentHandle handle, bool* outReceiveShadow) {
    if (!outReceiveShadow) { SetPanelError(LC_ERROR_NULL_POINTER, "outReceiveShadow is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outReceiveShadow = panel->GetReceiveShadow(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetReceiveShadow failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_receive_shadow(LCComponentHandle handle, bool receiveShadow) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetReceiveShadow(receiveShadow); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetReceiveShadow failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Layer Properties ----- */

LC_API LCResult lc_panel3d_get_layer(LCComponentHandle handle, int* outLayer) {
    if (!outLayer) { SetPanelError(LC_ERROR_NULL_POINTER, "outLayer is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLayer = panel->GetLayer(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetLayer failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_layer(LCComponentHandle handle, int layer) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetLayer(layer); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetLayer failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_sorting_order(LCComponentHandle handle, int* outOrder) {
    if (!outOrder) { SetPanelError(LC_ERROR_NULL_POINTER, "outOrder is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outOrder = panel->GetSortingOrder(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetSortingOrder failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_sorting_order(LCComponentHandle handle, int order) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetSortingOrder(order); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetSortingOrder failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- StyleBox File ----- */

LC_API LCResult lc_panel3d_load_style(LCComponentHandle handle, const char* filepath) {
    if (!filepath) { SetPanelError(LC_ERROR_NULL_POINTER, "filepath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        if (panel->LoadStyleFromFile(filepath)) return LC_SUCCESS;
        SetPanelError(LC_ERROR_FILE_NOT_FOUND, "Failed to load style file");
        return LC_ERROR_FILE_NOT_FOUND;
    } catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "LoadStyleFromFile failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_save_style(LCComponentHandle handle, const char* filepath) {
    if (!filepath) { SetPanelError(LC_ERROR_NULL_POINTER, "filepath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        if (panel->SaveStyleToFile(filepath)) return LC_SUCCESS;
        SetPanelError(LC_ERROR_INTERNAL_ERROR, "Failed to save style file");
        return LC_ERROR_INTERNAL_ERROR;
    } catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SaveStyleToFile failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_style_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetPanelError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = panel->GetStylePath();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetStylePath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_style_path(LCComponentHandle handle, const char* path) {
    if (!path) { SetPanelError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetStylePath(path); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetStylePath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Background ----- */

LC_API LCResult lc_panel3d_get_background_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetPanelError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(panel->GetBackgroundColor()); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_background_color(LCComponentHandle handle, LCColor color) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetBackgroundColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_opacity(LCComponentHandle handle, float* outOpacity) {
    if (!outOpacity) { SetPanelError(LC_ERROR_NULL_POINTER, "outOpacity is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outOpacity = panel->GetOpacity(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetOpacity failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_opacity(LCComponentHandle handle, float opacity) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetOpacity(opacity); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetOpacity failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Border Width ----- */

LC_API LCResult lc_panel3d_get_border_width_linked(LCComponentHandle handle, bool* outLinked) {
    if (!outLinked) { SetPanelError(LC_ERROR_NULL_POINTER, "outLinked is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLinked = panel->GetBorderWidthLinked(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidthLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_border_width_linked(LCComponentHandle handle, bool linked) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetBorderWidthLinked(linked); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_border_width_left(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetPanelError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = panel->GetBorderWidthLeft(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidthLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_border_width_left(LCComponentHandle handle, float width) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetBorderWidthLeft(width); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_border_width_right(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetPanelError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = panel->GetBorderWidthRight(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidthRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_border_width_right(LCComponentHandle handle, float width) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetBorderWidthRight(width); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_border_width_top(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetPanelError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = panel->GetBorderWidthTop(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidthTop failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_border_width_top(LCComponentHandle handle, float width) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetBorderWidthTop(width); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthTop failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_border_width_bottom(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetPanelError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = panel->GetBorderWidthBottom(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidthBottom failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_border_width_bottom(LCComponentHandle handle, float width) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetBorderWidthBottom(width); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthBottom failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Border Enabled/Color ----- */

LC_API LCResult lc_panel3d_get_border_enabled(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) { SetPanelError(LC_ERROR_NULL_POINTER, "outEnabled is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outEnabled = panel->GetBorderEnabled(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetBorderEnabled failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_border_enabled(LCComponentHandle handle, bool enabled) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetBorderEnabled(enabled); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetBorderEnabled failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_border_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetPanelError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(panel->GetBorderColor()); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetBorderColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_border_color(LCComponentHandle handle, LCColor color) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetBorderColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetBorderColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Corner Radius ----- */

LC_API LCResult lc_panel3d_get_corner_radius_linked(LCComponentHandle handle, bool* outLinked) {
    if (!outLinked) { SetPanelError(LC_ERROR_NULL_POINTER, "outLinked is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLinked = panel->GetCornerRadiusLinked(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadiusLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_corner_radius_linked(LCComponentHandle handle, bool linked) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetCornerRadiusLinked(linked); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_corner_radius_top_left(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) { SetPanelError(LC_ERROR_NULL_POINTER, "outRadius is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outRadius = panel->GetCornerRadiusTopLeft(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadiusTopLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_corner_radius_top_left(LCComponentHandle handle, float radius) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetCornerRadiusTopLeft(radius); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusTopLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_corner_radius_top_right(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) { SetPanelError(LC_ERROR_NULL_POINTER, "outRadius is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outRadius = panel->GetCornerRadiusTopRight(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadiusTopRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_corner_radius_top_right(LCComponentHandle handle, float radius) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetCornerRadiusTopRight(radius); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusTopRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_corner_radius_bottom_left(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) { SetPanelError(LC_ERROR_NULL_POINTER, "outRadius is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outRadius = panel->GetCornerRadiusBottomLeft(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadiusBottomLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_corner_radius_bottom_left(LCComponentHandle handle, float radius) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetCornerRadiusBottomLeft(radius); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusBottomLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_corner_radius_bottom_right(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) { SetPanelError(LC_ERROR_NULL_POINTER, "outRadius is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outRadius = panel->GetCornerRadiusBottomRight(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadiusBottomRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_corner_radius_bottom_right(LCComponentHandle handle, float radius) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetCornerRadiusBottomRight(radius); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusBottomRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Corner Detail & Anti-Aliasing ----- */

LC_API LCResult lc_panel3d_get_corner_detail(LCComponentHandle handle, int* outDetail) {
    if (!outDetail) { SetPanelError(LC_ERROR_NULL_POINTER, "outDetail is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outDetail = panel->GetCornerDetail(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetCornerDetail failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_corner_detail(LCComponentHandle handle, int detail) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetCornerDetail(detail); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetCornerDetail failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_anti_aliasing(LCComponentHandle handle, bool* outAA) {
    if (!outAA) { SetPanelError(LC_ERROR_NULL_POINTER, "outAA is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outAA = panel->GetAntiAliasing(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetAntiAliasing failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_anti_aliasing(LCComponentHandle handle, bool aa) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetAntiAliasing(aa); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetAntiAliasing failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_anti_aliasing_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) { SetPanelError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = panel->GetAntiAliasingSize(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetAntiAliasingSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_anti_aliasing_size(LCComponentHandle handle, float size) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetAntiAliasingSize(size); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetAntiAliasingSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Shadow (StyleBox visual shadow) ----- */

LC_API LCResult lc_panel3d_get_shadow_enabled(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) { SetPanelError(LC_ERROR_NULL_POINTER, "outEnabled is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outEnabled = panel->GetShadowEnabled(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetShadowEnabled failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_shadow_enabled(LCComponentHandle handle, bool enabled) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetShadowEnabled(enabled); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetShadowEnabled failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_shadow_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetPanelError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(panel->GetShadowColor()); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetShadowColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_shadow_color(LCComponentHandle handle, LCColor color) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetShadowColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetShadowColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_shadow_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) { SetPanelError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = panel->GetShadowSize(); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetShadowSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_shadow_size(LCComponentHandle handle, float size) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetShadowSize(size); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetShadowSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_get_shadow_offset(LCComponentHandle handle, LCVec2* outOffset) {
    if (!outOffset) { SetPanelError(LC_ERROR_NULL_POINTER, "outOffset is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outOffset = FromEngineVec2(panel->GetShadowOffset()); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetShadowOffset failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_shadow_offset(LCComponentHandle handle, LCVec2 offset) {
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetShadowOffset(ToEngineVec2(offset)); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetShadowOffset failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Custom Shader ----- */

LC_API LCResult lc_panel3d_get_custom_shader_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetPanelError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = panel->GetCustomShaderPath();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetCustomShaderPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel3d_set_custom_shader_path(LCComponentHandle handle, const char* path) {
    if (!path) { SetPanelError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel3D(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetCustomShaderPath(path); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetCustomShaderPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Panel Custom Shader (.lsh) ----- */

LC_API LCResult lc_panel_get_shader(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetPanelError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    if (maxLength == 0) { SetPanelError(LC_ERROR_INVALID_PARAMETER, "maxLength is 0"); return LC_ERROR_INVALID_PARAMETER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = panel->GetShader();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetShader failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_shader(LCComponentHandle handle, const char* path) {
    if (!path) { SetPanelError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetShader(path); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetShader failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_get_shader_parameters(LCComponentHandle handle, char* outJson, uint32_t maxLength) {
    if (!outJson) { SetPanelError(LC_ERROR_NULL_POINTER, "outJson is NULL"); return LC_ERROR_NULL_POINTER; }
    if (maxLength == 0) { SetPanelError(LC_ERROR_INVALID_PARAMETER, "maxLength is 0"); return LC_ERROR_INVALID_PARAMETER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& params = panel->GetShaderParameters();
        CopyStringToBuffer(outJson, maxLength, params.c_str());
        return LC_SUCCESS;
    } catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "GetShaderParameters failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_panel_set_shader_parameters(LCComponentHandle handle, const char* json) {
    if (!json) { SetPanelError(LC_ERROR_NULL_POINTER, "json is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* panel = GetPanel(handle);
    if (!panel) { SetPanelError(LC_ERROR_INVALID_HANDLE, "Invalid Panel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { panel->SetShaderParameters(json); return LC_SUCCESS; }
    catch (...) { SetPanelError(LC_ERROR_INTERNAL_ERROR, "SetShaderParameters failed"); return LC_ERROR_INTERNAL_ERROR; }
}
