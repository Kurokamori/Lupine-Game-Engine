/**
 * @file lc_color_rect.cpp
 * @brief Implementation of Lupine Engine C API - ColorRect Component
 */

#include "ui/lc_color_rect.h"
#include "../core/lc_internal.h"

#include <lupine/components/ColorRect.hpp>
#include <lupine/core/Node.hpp>

#include <cstring>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetColorRectError(LCResult code, const char* message) {
    ::SetError(code, message);
}

ColorRect* GetColorRect(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<ColorRect*>(component.get());
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

LC_API LCResult lc_color_rect_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetColorRectError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto component = std::make_shared<ColorRect>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetColorRectError(LC_ERROR_INTERNAL_ERROR, "Failed to create ColorRect component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Core Properties ----- */

LC_API LCResult lc_color_rect_get_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetColorRectError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(rect->GetColor()); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "GetColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_color(LCComponentHandle handle, LCColor color) {
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { rect->SetColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Layer Properties ----- */

LC_API LCResult lc_color_rect_get_layer(LCComponentHandle handle, int* outLayer) {
    if (!outLayer) { SetColorRectError(LC_ERROR_NULL_POINTER, "outLayer is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLayer = rect->GetLayer(); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "GetLayer failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_layer(LCComponentHandle handle, int layer) {
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { rect->SetLayer(layer); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetLayer failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_get_sorting_order(LCComponentHandle handle, int* outOrder) {
    if (!outOrder) { SetColorRectError(LC_ERROR_NULL_POINTER, "outOrder is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outOrder = rect->GetSortingOrder(); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "GetSortingOrder failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_sorting_order(LCComponentHandle handle, int order) {
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { rect->SetSortingOrder(order); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetSortingOrder failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Size Properties ----- */

LC_API LCResult lc_color_rect_get_width(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetColorRectError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = rect->GetWidth(); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "GetWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_width(LCComponentHandle handle, float width) {
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { rect->SetWidth(width); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_get_height(LCComponentHandle handle, float* outHeight) {
    if (!outHeight) { SetColorRectError(LC_ERROR_NULL_POINTER, "outHeight is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outHeight = rect->GetHeight(); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "GetHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_height(LCComponentHandle handle, float height) {
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { rect->SetHeight(height); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_get_size(LCComponentHandle handle, LCVec2* outSize) {
    if (!outSize) { SetColorRectError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = FromEngineVec2(rect->GetSize()); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "GetSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_size(LCComponentHandle handle, LCVec2 size) {
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { rect->SetSize(ToEngineVec2(size)); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Corner Radius ----- */

LC_API LCResult lc_color_rect_get_corner_radius_linked(LCComponentHandle handle, bool* outLinked) {
    if (!outLinked) { SetColorRectError(LC_ERROR_NULL_POINTER, "outLinked is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLinked = rect->IsCornerRadiusLinked(); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "IsCornerRadiusLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_corner_radius_linked(LCComponentHandle handle, bool linked) {
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { rect->SetCornerRadiusLinked(linked); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_corner_radius_all(LCComponentHandle handle, float radius) {
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { rect->SetCornerRadiusAll(radius); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusAll failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_corner_radius_individual(LCComponentHandle handle, int index, float radius) {
    if (index < 0 || index > 3) { SetColorRectError(LC_ERROR_INVALID_PARAMETER, "index out of range (0-3)"); return LC_ERROR_INVALID_PARAMETER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { rect->SetCornerRadiusIndividual(static_cast<size_t>(index), radius); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusIndividual failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_get_corner_radius(LCComponentHandle handle, int index, float* outRadius) {
    if (!outRadius) { SetColorRectError(LC_ERROR_NULL_POINTER, "outRadius is NULL"); return LC_ERROR_NULL_POINTER; }
    if (index < 0 || index > 3) { SetColorRectError(LC_ERROR_INVALID_PARAMETER, "index out of range (0-3)"); return LC_ERROR_INVALID_PARAMETER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const auto& radius = rect->GetCornerRadius();
        *outRadius = radius.Get(static_cast<size_t>(index));
        return LC_SUCCESS;
    } catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadius failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Border Properties ----- */

LC_API LCResult lc_color_rect_get_border_enabled(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) { SetColorRectError(LC_ERROR_NULL_POINTER, "outEnabled is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outEnabled = rect->GetBorderEnabled(); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "GetBorderEnabled failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_border_enabled(LCComponentHandle handle, bool enabled) {
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { rect->SetBorderEnabled(enabled); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetBorderEnabled failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_get_border_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetColorRectError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(rect->GetBorderColor()); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "GetBorderColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_border_color(LCComponentHandle handle, LCColor color) {
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { rect->SetBorderColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetBorderColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Border Width ----- */

LC_API LCResult lc_color_rect_get_border_width_linked(LCComponentHandle handle, bool* outLinked) {
    if (!outLinked) { SetColorRectError(LC_ERROR_NULL_POINTER, "outLinked is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLinked = rect->IsBorderWidthLinked(); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "IsBorderWidthLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_border_width_linked(LCComponentHandle handle, bool linked) {
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { rect->SetBorderWidthLinked(linked); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_border_width_all(LCComponentHandle handle, float width) {
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { rect->SetBorderWidthAll(width); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthAll failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_border_width_individual(LCComponentHandle handle, int index, float width) {
    if (index < 0 || index > 3) { SetColorRectError(LC_ERROR_INVALID_PARAMETER, "index out of range (0-3)"); return LC_ERROR_INVALID_PARAMETER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { rect->SetBorderWidthIndividual(static_cast<size_t>(index), width); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthIndividual failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_get_border_width(LCComponentHandle handle, int index, float* outWidth) {
    if (!outWidth) { SetColorRectError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    if (index < 0 || index > 3) { SetColorRectError(LC_ERROR_INVALID_PARAMETER, "index out of range (0-3)"); return LC_ERROR_INVALID_PARAMETER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const auto& width = rect->GetBorderWidth();
        *outWidth = width.Get(static_cast<size_t>(index));
        return LC_SUCCESS;
    } catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- UI Space ----- */

LC_API LCResult lc_color_rect_get_ui_space(LCComponentHandle handle, bool* outUISpace) {
    if (!outUISpace) { SetColorRectError(LC_ERROR_NULL_POINTER, "outUISpace is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outUISpace = rect->GetUISpace(); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "GetUISpace failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_ui_space(LCComponentHandle handle, bool uiSpace) {
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { rect->SetUISpace(uiSpace); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetUISpace failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Blend Mode ----- */

LC_API LCResult lc_color_rect_get_blend_mode(LCComponentHandle handle, LCBlendMode* outMode) {
    if (!outMode) { SetColorRectError(LC_ERROR_NULL_POINTER, "outMode is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        *outMode = static_cast<LCBlendMode>(static_cast<int>(rect->GetBlendMode()));
        return LC_SUCCESS;
    } catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "GetBlendMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_blend_mode(LCComponentHandle handle, LCBlendMode mode) {
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        rect->SetBlendMode(static_cast<BlendMode>(static_cast<int>(mode)));
        return LC_SUCCESS;
    } catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetBlendMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Material Override ----- */

LC_API LCResult lc_color_rect_get_material_override(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetColorRectError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = rect->GetMaterialOverride();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "GetMaterialOverride failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_material_override(LCComponentHandle handle, const char* path) {
    if (!path) { SetColorRectError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { rect->SetMaterialOverride(path); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetMaterialOverride failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Custom Shader ----- */

LC_API LCResult lc_color_rect_get_shader(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetColorRectError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    if (maxLength == 0) { SetColorRectError(LC_ERROR_INVALID_PARAMETER, "maxLength is 0"); return LC_ERROR_INVALID_PARAMETER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = rect->GetShader();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "GetShader failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_shader(LCComponentHandle handle, const char* path) {
    if (!path) { SetColorRectError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { rect->SetShader(path); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetShader failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_get_shader_parameters(LCComponentHandle handle, char* outJson, uint32_t maxLength) {
    if (!outJson) { SetColorRectError(LC_ERROR_NULL_POINTER, "outJson is NULL"); return LC_ERROR_NULL_POINTER; }
    if (maxLength == 0) { SetColorRectError(LC_ERROR_INVALID_PARAMETER, "maxLength is 0"); return LC_ERROR_INVALID_PARAMETER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& params = rect->GetShaderParameters();
        CopyStringToBuffer(outJson, maxLength, params.c_str());
        return LC_SUCCESS;
    } catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "GetShaderParameters failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_color_rect_set_shader_parameters(LCComponentHandle handle, const char* json) {
    if (!json) { SetColorRectError(LC_ERROR_NULL_POINTER, "json is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* rect = GetColorRect(handle);
    if (!rect) { SetColorRectError(LC_ERROR_INVALID_HANDLE, "Invalid ColorRect handle"); return LC_ERROR_INVALID_HANDLE; }
    try { rect->SetShaderParameters(json); return LC_SUCCESS; }
    catch (...) { SetColorRectError(LC_ERROR_INTERNAL_ERROR, "SetShaderParameters failed"); return LC_ERROR_INTERNAL_ERROR; }
}
