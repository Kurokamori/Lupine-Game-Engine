/**
 * @file lc_image2d.cpp
 * @brief Implementation of Lupine Engine C API - Image2D Component
 */

#include "ui/lc_image2d.h"
#include "../core/lc_internal.h"

#include <lupine/components/Image2D.hpp>
#include <lupine/core/Node.hpp>

#include <cstring>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetImage2DError(LCResult code, const char* message) {
    ::SetError(code, message);
}

Image2D* GetImage2D(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<Image2D*>(component.get());
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

LC_API LCResult lc_image2d_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetImage2DError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto component = std::make_shared<Image2D>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetImage2DError(LC_ERROR_INTERNAL_ERROR, "Failed to create Image2D component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Size Properties ----- */

LC_API LCResult lc_image2d_get_width(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetImage2DError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = img->GetWidth(); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_width(LCComponentHandle handle, float width) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetWidth(width); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_get_height(LCComponentHandle handle, float* outHeight) {
    if (!outHeight) { SetImage2DError(LC_ERROR_NULL_POINTER, "outHeight is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outHeight = img->GetHeight(); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_height(LCComponentHandle handle, float height) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetHeight(height); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Texture Management ----- */

LC_API LCResult lc_image2d_load_texture(LCComponentHandle handle, const char* filepath) {
    if (!filepath) { SetImage2DError(LC_ERROR_NULL_POINTER, "filepath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        if (img->LoadTexture(filepath)) return LC_SUCCESS;
        SetImage2DError(LC_ERROR_FILE_NOT_FOUND, "Failed to load texture");
        return LC_ERROR_FILE_NOT_FOUND;
    } catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "LoadTexture failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_get_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetImage2DError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = img->GetTexturePath();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetTexturePath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_texture_path(LCComponentHandle handle, const char* path) {
    if (!path) { SetImage2DError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetTexturePath(path); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetTexturePath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Material Override ----- */

LC_API LCResult lc_image2d_get_material_override(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetImage2DError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = img->GetMaterialOverride();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetMaterialOverride failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_material_override(LCComponentHandle handle, const char* path) {
    if (!path) { SetImage2DError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetMaterialOverride(path); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetMaterialOverride failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Color/Tint ----- */

LC_API LCResult lc_image2d_get_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetImage2DError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(img->GetColor()); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_color(LCComponentHandle handle, LCColor color) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Anchor Properties ----- */

LC_API LCResult lc_image2d_get_anchor_min(LCComponentHandle handle, LCVec2* outAnchor) {
    if (!outAnchor) { SetImage2DError(LC_ERROR_NULL_POINTER, "outAnchor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outAnchor = FromEngineVec2(img->GetAnchorMin()); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetAnchorMin failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_anchor_min(LCComponentHandle handle, LCVec2 anchor) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetAnchorMin(ToEngineVec2(anchor)); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetAnchorMin failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_get_anchor_max(LCComponentHandle handle, LCVec2* outAnchor) {
    if (!outAnchor) { SetImage2DError(LC_ERROR_NULL_POINTER, "outAnchor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outAnchor = FromEngineVec2(img->GetAnchorMax()); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetAnchorMax failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_anchor_max(LCComponentHandle handle, LCVec2 anchor) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetAnchorMax(ToEngineVec2(anchor)); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetAnchorMax failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_get_offset_min(LCComponentHandle handle, LCVec2* outOffset) {
    if (!outOffset) { SetImage2DError(LC_ERROR_NULL_POINTER, "outOffset is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outOffset = FromEngineVec2(img->GetOffsetMin()); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetOffsetMin failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_offset_min(LCComponentHandle handle, LCVec2 offset) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetOffsetMin(ToEngineVec2(offset)); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetOffsetMin failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_get_offset_max(LCComponentHandle handle, LCVec2* outOffset) {
    if (!outOffset) { SetImage2DError(LC_ERROR_NULL_POINTER, "outOffset is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outOffset = FromEngineVec2(img->GetOffsetMax()); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetOffsetMax failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_offset_max(LCComponentHandle handle, LCVec2 offset) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetOffsetMax(ToEngineVec2(offset)); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetOffsetMax failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Aspect Ratio ----- */

LC_API LCResult lc_image2d_get_preserve_aspect(LCComponentHandle handle, bool* outPreserve) {
    if (!outPreserve) { SetImage2DError(LC_ERROR_NULL_POINTER, "outPreserve is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outPreserve = img->GetPreserveAspect(); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetPreserveAspect failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_preserve_aspect(LCComponentHandle handle, bool preserve) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetPreserveAspect(preserve); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetPreserveAspect failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_get_aspect_mode(LCComponentHandle handle, LCAspectMode* outMode) {
    if (!outMode) { SetImage2DError(LC_ERROR_NULL_POINTER, "outMode is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        *outMode = static_cast<LCAspectMode>(static_cast<int>(img->GetAspectMode()));
        return LC_SUCCESS;
    } catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetAspectMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_aspect_mode(LCComponentHandle handle, LCAspectMode mode) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        img->SetAspectMode(static_cast<AspectMode>(static_cast<int>(mode)));
        return LC_SUCCESS;
    } catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetAspectMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Flip Properties ----- */

LC_API LCResult lc_image2d_get_flip_h(LCComponentHandle handle, bool* outFlip) {
    if (!outFlip) { SetImage2DError(LC_ERROR_NULL_POINTER, "outFlip is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outFlip = img->GetFlipH(); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetFlipH failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_flip_h(LCComponentHandle handle, bool flip) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetFlipH(flip); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetFlipH failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_get_flip_v(LCComponentHandle handle, bool* outFlip) {
    if (!outFlip) { SetImage2DError(LC_ERROR_NULL_POINTER, "outFlip is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outFlip = img->GetFlipV(); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetFlipV failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_flip_v(LCComponentHandle handle, bool flip) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetFlipV(flip); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetFlipV failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Blend Mode ----- */

LC_API LCResult lc_image2d_get_blend_mode(LCComponentHandle handle, LCBlendMode* outMode) {
    if (!outMode) { SetImage2DError(LC_ERROR_NULL_POINTER, "outMode is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        *outMode = static_cast<LCBlendMode>(static_cast<int>(img->GetBlendMode()));
        return LC_SUCCESS;
    } catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetBlendMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_blend_mode(LCComponentHandle handle, LCBlendMode mode) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        img->SetBlendMode(static_cast<BlendMode>(static_cast<int>(mode)));
        return LC_SUCCESS;
    } catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetBlendMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Mouse Behaviour ----- */

LC_API LCResult lc_image2d_get_mouse_behaviour(LCComponentHandle handle, LCMouseBehaviour* outBehaviour) {
    if (!outBehaviour) { SetImage2DError(LC_ERROR_NULL_POINTER, "outBehaviour is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        *outBehaviour = static_cast<LCMouseBehaviour>(static_cast<int>(img->GetMouseBehaviour()));
        return LC_SUCCESS;
    } catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetMouseBehaviour failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_mouse_behaviour(LCComponentHandle handle, LCMouseBehaviour behaviour) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        img->SetMouseBehaviour(static_cast<MouseBehaviour>(static_cast<int>(behaviour)));
        return LC_SUCCESS;
    } catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetMouseBehaviour failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Corner Radius ----- */

LC_API LCResult lc_image2d_get_corner_radius_linked(LCComponentHandle handle, bool* outLinked) {
    if (!outLinked) { SetImage2DError(LC_ERROR_NULL_POINTER, "outLinked is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLinked = img->IsCornerRadiusLinked(); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "IsCornerRadiusLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_corner_radius_linked(LCComponentHandle handle, bool linked) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetCornerRadiusLinked(linked); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_corner_radius_all(LCComponentHandle handle, float radius) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetCornerRadiusAll(radius); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusAll failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_corner_radius_individual(LCComponentHandle handle, int index, float radius) {
    if (index < 0 || index > 3) { SetImage2DError(LC_ERROR_INVALID_PARAMETER, "index out of range (0-3)"); return LC_ERROR_INVALID_PARAMETER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetCornerRadiusIndividual(static_cast<size_t>(index), radius); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusIndividual failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_get_corner_radius(LCComponentHandle handle, int index, float* outRadius) {
    if (!outRadius) { SetImage2DError(LC_ERROR_NULL_POINTER, "outRadius is NULL"); return LC_ERROR_NULL_POINTER; }
    if (index < 0 || index > 3) { SetImage2DError(LC_ERROR_INVALID_PARAMETER, "index out of range (0-3)"); return LC_ERROR_INVALID_PARAMETER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const auto& radius = img->GetCornerRadius();
        *outRadius = radius.Get(static_cast<size_t>(index));
        return LC_SUCCESS;
    } catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadius failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Border Properties ----- */

LC_API LCResult lc_image2d_get_border_enabled(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) { SetImage2DError(LC_ERROR_NULL_POINTER, "outEnabled is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outEnabled = img->GetBorderEnabled(); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetBorderEnabled failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_border_enabled(LCComponentHandle handle, bool enabled) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetBorderEnabled(enabled); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetBorderEnabled failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_get_border_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetImage2DError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(img->GetBorderColor()); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetBorderColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_border_color(LCComponentHandle handle, LCColor color) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetBorderColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetBorderColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Border Width ----- */

LC_API LCResult lc_image2d_get_border_width_linked(LCComponentHandle handle, bool* outLinked) {
    if (!outLinked) { SetImage2DError(LC_ERROR_NULL_POINTER, "outLinked is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLinked = img->IsBorderWidthLinked(); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "IsBorderWidthLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_border_width_linked(LCComponentHandle handle, bool linked) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetBorderWidthLinked(linked); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_border_width_all(LCComponentHandle handle, float width) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetBorderWidthAll(width); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthAll failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_border_width_individual(LCComponentHandle handle, int index, float width) {
    if (index < 0 || index > 3) { SetImage2DError(LC_ERROR_INVALID_PARAMETER, "index out of range (0-3)"); return LC_ERROR_INVALID_PARAMETER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetBorderWidthIndividual(static_cast<size_t>(index), width); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthIndividual failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_get_border_width(LCComponentHandle handle, int index, float* outWidth) {
    if (!outWidth) { SetImage2DError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    if (index < 0 || index > 3) { SetImage2DError(LC_ERROR_INVALID_PARAMETER, "index out of range (0-3)"); return LC_ERROR_INVALID_PARAMETER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const auto& width = img->GetBorderWidth();
        *outWidth = width.Get(static_cast<size_t>(index));
        return LC_SUCCESS;
    } catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- UI Space ----- */

LC_API LCResult lc_image2d_get_ui_space(LCComponentHandle handle, bool* outUISpace) {
    if (!outUISpace) { SetImage2DError(LC_ERROR_NULL_POINTER, "outUISpace is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outUISpace = img->GetUISpace(); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetUISpace failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_ui_space(LCComponentHandle handle, bool uiSpace) {
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetUISpace(uiSpace); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetUISpace failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Custom Shader (.lsh) ----- */

LC_API LCResult lc_image2d_get_shader(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetImage2DError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    if (maxLength == 0) { SetImage2DError(LC_ERROR_INVALID_PARAMETER, "maxLength is 0"); return LC_ERROR_INVALID_PARAMETER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = img->GetShader();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetShader failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_shader(LCComponentHandle handle, const char* path) {
    if (!path) { SetImage2DError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetShader(path); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetShader failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_get_shader_parameters(LCComponentHandle handle, char* outJson, uint32_t maxLength) {
    if (!outJson) { SetImage2DError(LC_ERROR_NULL_POINTER, "outJson is NULL"); return LC_ERROR_NULL_POINTER; }
    if (maxLength == 0) { SetImage2DError(LC_ERROR_INVALID_PARAMETER, "maxLength is 0"); return LC_ERROR_INVALID_PARAMETER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& params = img->GetShaderParameters();
        CopyStringToBuffer(outJson, maxLength, params.c_str());
        return LC_SUCCESS;
    } catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "GetShaderParameters failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_image2d_set_shader_parameters(LCComponentHandle handle, const char* json) {
    if (!json) { SetImage2DError(LC_ERROR_NULL_POINTER, "json is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* img = GetImage2D(handle);
    if (!img) { SetImage2DError(LC_ERROR_INVALID_HANDLE, "Invalid Image2D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { img->SetShaderParameters(json); return LC_SUCCESS; }
    catch (...) { SetImage2DError(LC_ERROR_INTERNAL_ERROR, "SetShaderParameters failed"); return LC_ERROR_INTERNAL_ERROR; }
}
