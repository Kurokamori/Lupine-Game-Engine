/**
 * @file lc_label3d.cpp
 * @brief Implementation of Lupine Engine C API - Label3D Component
 */

#include "ui/lc_label3d.h"
#include "../core/lc_internal.h"

#include <lupine/components/Label3D.hpp>
#include <lupine/components/Sprite3D.hpp>  // For BillboardMode enum
#include <lupine/core/Node.hpp>

#include <cstring>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetLabel3DError(LCResult code, const char* message) {
    ::SetError(code, message);
}

Label3D* GetLabel3D(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<Label3D*>(component.get());
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
 * Label3D Component
 * ============================================================================ */

LC_API LCResult lc_label3d_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetLabel3DError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto component = std::make_shared<Label3D>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "Failed to create Label3D component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Font Management ----- */

LC_API LCResult lc_label3d_load_font(LCComponentHandle handle, const char* filepath) {
    if (!filepath) { SetLabel3DError(LC_ERROR_NULL_POINTER, "filepath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* label = GetLabel3D(handle);
    if (!label) { SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid Label3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        if (label->LoadFont(filepath)) return LC_SUCCESS;
        SetLabel3DError(LC_ERROR_FILE_NOT_FOUND, "Failed to load font file");
        return LC_ERROR_FILE_NOT_FOUND;
    } catch (...) { SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "LoadFont failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_label3d_get_font_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetLabel3DError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* label = GetLabel3D(handle);
    if (!label) { SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid Label3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = label->GetFontPath();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "GetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_label3d_set_font_path(LCComponentHandle handle, const char* path) {
    if (!path) { SetLabel3DError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* label = GetLabel3D(handle);
    if (!label) { SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid Label3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { label->SetFontPath(path); return LC_SUCCESS; }
    catch (...) { SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "SetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Text Properties ----- */

LC_API LCResult lc_label3d_get_text(LCComponentHandle handle, char* outText, uint32_t maxLength) {
    if (!outText) { SetLabel3DError(LC_ERROR_NULL_POINTER, "outText is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* label = GetLabel3D(handle);
    if (!label) { SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid Label3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& text = label->GetText();
        CopyStringToBuffer(outText, maxLength, text.c_str());
        return LC_SUCCESS;
    } catch (...) { SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "GetText failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_label3d_set_text(LCComponentHandle handle, const char* text) {
    if (!text) { SetLabel3DError(LC_ERROR_NULL_POINTER, "text is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* label = GetLabel3D(handle);
    if (!label) { SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid Label3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { label->SetText(text); return LC_SUCCESS; }
    catch (...) { SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "SetText failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Font Size ----- */

LC_API LCResult lc_label3d_get_font_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) { SetLabel3DError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* label = GetLabel3D(handle);
    if (!label) { SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid Label3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = label->GetFontSize(); return LC_SUCCESS; }
    catch (...) { SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "GetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_label3d_set_font_size(LCComponentHandle handle, float size) {
    auto* label = GetLabel3D(handle);
    if (!label) { SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid Label3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { label->SetFontSize(size); return LC_SUCCESS; }
    catch (...) { SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "SetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Color ----- */

LC_API LCResult lc_label3d_get_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetLabel3DError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* label = GetLabel3D(handle);
    if (!label) { SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid Label3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(label->GetColor()); return LC_SUCCESS; }
    catch (...) { SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "GetColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_label3d_set_color(LCComponentHandle handle, LCColor color) {
    auto* label = GetLabel3D(handle);
    if (!label) { SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid Label3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { label->SetColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "SetColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Text Alignment ----- */

LC_API LCResult lc_label3d_get_centered(LCComponentHandle handle, bool* outCentered) {
    if (!outCentered) { SetLabel3DError(LC_ERROR_NULL_POINTER, "outCentered is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* label = GetLabel3D(handle);
    if (!label) { SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid Label3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outCentered = label->GetCentered(); return LC_SUCCESS; }
    catch (...) { SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "GetCentered failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_label3d_set_centered(LCComponentHandle handle, bool centered) {
    auto* label = GetLabel3D(handle);
    if (!label) { SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid Label3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { label->SetCentered(centered); return LC_SUCCESS; }
    catch (...) { SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "SetCentered failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Offset ----- */

LC_API LCResult lc_label3d_get_offset(LCComponentHandle handle, LCVec2* outOffset) {
    if (!outOffset) { SetLabel3DError(LC_ERROR_NULL_POINTER, "outOffset is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* label = GetLabel3D(handle);
    if (!label) { SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid Label3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outOffset = FromEngineVec2(label->GetOffset()); return LC_SUCCESS; }
    catch (...) { SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "GetOffset failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_label3d_set_offset(LCComponentHandle handle, LCVec2 offset) {
    auto* label = GetLabel3D(handle);
    if (!label) { SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid Label3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { label->SetOffset(ToEngineVec2(offset)); return LC_SUCCESS; }
    catch (...) { SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "SetOffset failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Billboard Mode ----- */

LC_API LCResult lc_label3d_get_billboard_mode(LCComponentHandle handle, LCLabel3DBillboardMode* outMode) {
    if (!outMode) { SetLabel3DError(LC_ERROR_NULL_POINTER, "outMode is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* label = GetLabel3D(handle);
    if (!label) { SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid Label3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        auto mode = label->GetBillboardMode();
        *outMode = static_cast<LCLabel3DBillboardMode>(static_cast<int>(mode));
        return LC_SUCCESS;
    } catch (...) { SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "GetBillboardMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_label3d_set_billboard_mode(LCComponentHandle handle, LCLabel3DBillboardMode mode) {
    auto* label = GetLabel3D(handle);
    if (!label) { SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid Label3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        label->SetBillboardMode(static_cast<BillboardMode>(static_cast<int>(mode)));
        return LC_SUCCESS;
    } catch (...) { SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "SetBillboardMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Pixel Size ----- */

LC_API LCResult lc_label3d_get_pixel_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) { SetLabel3DError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* label = GetLabel3D(handle);
    if (!label) { SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid Label3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = label->GetPixelSize(); return LC_SUCCESS; }
    catch (...) { SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "GetPixelSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_label3d_set_pixel_size(LCComponentHandle handle, float size) {
    auto* label = GetLabel3D(handle);
    if (!label) { SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid Label3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { label->SetPixelSize(size); return LC_SUCCESS; }
    catch (...) { SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "SetPixelSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Text Measurement ----- */

LC_API LCResult lc_label3d_calculate_text_size(LCComponentHandle handle, LCVec2* outSize) {
    if (!outSize) { SetLabel3DError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* label = GetLabel3D(handle);
    if (!label) { SetLabel3DError(LC_ERROR_INVALID_HANDLE, "Invalid Label3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = FromEngineVec2(label->CalculateTextSize()); return LC_SUCCESS; }
    catch (...) { SetLabel3DError(LC_ERROR_INTERNAL_ERROR, "CalculateTextSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}
