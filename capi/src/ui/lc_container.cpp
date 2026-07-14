/**
 * @file lc_container.cpp
 * @brief Implementation of Lupine Engine C API - Container Components
 */

#include "ui/lc_container.h"
#include "../core/lc_internal.h"

#include <lupine/components/Container.hpp>
#include <lupine/components/VerticalContainer.hpp>
#include <lupine/components/HorizontalContainer.hpp>
#include <lupine/components/GridContainer.hpp>
#include <lupine/components/CenterContainer.hpp>
#include <lupine/core/Node.hpp>

#include <cstring>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetContainerError(LCResult code, const char* message) {
    ::SetError(code, message);
}

Container* GetContainer(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<Container*>(component.get());
}

VerticalContainer* GetVerticalContainer(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<VerticalContainer*>(component.get());
}

HorizontalContainer* GetHorizontalContainer(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<HorizontalContainer*>(component.get());
}

GridContainer* GetGridContainer(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<GridContainer*>(component.get());
}

CenterContainer* GetCenterContainer(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<CenterContainer*>(component.get());
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

LCVec4 FromEngineVec4(const Vec4& vec) {
    return LCVec4{vec.x, vec.y, vec.z, vec.w};
}

Vec4 ToEngineVec4(const LCVec4& vec) {
    return Vec4(vec.x, vec.y, vec.z, vec.w);
}

} // anonymous namespace

/* ============================================================================
 * Container Base Component
 * ============================================================================ */

LC_API LCResult lc_container_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetContainerError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto component = std::make_shared<Container>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetContainerError(LC_ERROR_INTERNAL_ERROR, "Failed to create Container component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Size Properties ----- */

LC_API LCResult lc_container_get_width(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetContainerError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = container->GetWidth(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_width(LCComponentHandle handle, float width) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetWidth(width); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_height(LCComponentHandle handle, float* outHeight) {
    if (!outHeight) { SetContainerError(LC_ERROR_NULL_POINTER, "outHeight is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outHeight = container->GetHeight(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_height(LCComponentHandle handle, float height) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetHeight(height); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_size(LCComponentHandle handle, LCVec2* outSize) {
    if (!outSize) { SetContainerError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = FromEngineVec2(container->GetSize()); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_size(LCComponentHandle handle, LCVec2 size) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetSize(ToEngineVec2(size)); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_min_size(LCComponentHandle handle, LCVec2* outSize) {
    if (!outSize) { SetContainerError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = FromEngineVec2(container->GetMinSize()); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetMinSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_min_size(LCComponentHandle handle, LCVec2 size) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetMinSize(ToEngineVec2(size)); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetMinSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_max_size(LCComponentHandle handle, LCVec2* outSize) {
    if (!outSize) { SetContainerError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = FromEngineVec2(container->GetMaxSize()); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetMaxSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_max_size(LCComponentHandle handle, LCVec2 size) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetMaxSize(ToEngineVec2(size)); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetMaxSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_horizontal_size_mode(LCComponentHandle handle, LCContainerSizeMode* outMode) {
    if (!outMode) { SetContainerError(LC_ERROR_NULL_POINTER, "outMode is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outMode = static_cast<LCContainerSizeMode>(container->GetHorizontalSizeMode()); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetHorizontalSizeMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_horizontal_size_mode(LCComponentHandle handle, LCContainerSizeMode mode) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetHorizontalSizeMode(static_cast<Container::SizeMode>(mode)); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetHorizontalSizeMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_vertical_size_mode(LCComponentHandle handle, LCContainerSizeMode* outMode) {
    if (!outMode) { SetContainerError(LC_ERROR_NULL_POINTER, "outMode is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outMode = static_cast<LCContainerSizeMode>(container->GetVerticalSizeMode()); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetVerticalSizeMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_vertical_size_mode(LCComponentHandle handle, LCContainerSizeMode mode) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetVerticalSizeMode(static_cast<Container::SizeMode>(mode)); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetVerticalSizeMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Padding ----- */

LC_API LCResult lc_container_get_padding_linked(LCComponentHandle handle, bool* outLinked) {
    if (!outLinked) { SetContainerError(LC_ERROR_NULL_POINTER, "outLinked is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLinked = container->GetPaddingLinked(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetPaddingLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_padding_linked(LCComponentHandle handle, bool linked) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetPaddingLinked(linked); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetPaddingLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_padding_left(LCComponentHandle handle, float* outPadding) {
    if (!outPadding) { SetContainerError(LC_ERROR_NULL_POINTER, "outPadding is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outPadding = container->GetPaddingLeft(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetPaddingLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_padding_left(LCComponentHandle handle, float padding) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetPaddingLeft(padding); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetPaddingLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_padding_right(LCComponentHandle handle, float* outPadding) {
    if (!outPadding) { SetContainerError(LC_ERROR_NULL_POINTER, "outPadding is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outPadding = container->GetPaddingRight(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetPaddingRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_padding_right(LCComponentHandle handle, float padding) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetPaddingRight(padding); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetPaddingRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_padding_top(LCComponentHandle handle, float* outPadding) {
    if (!outPadding) { SetContainerError(LC_ERROR_NULL_POINTER, "outPadding is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outPadding = container->GetPaddingTop(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetPaddingTop failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_padding_top(LCComponentHandle handle, float padding) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetPaddingTop(padding); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetPaddingTop failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_padding_bottom(LCComponentHandle handle, float* outPadding) {
    if (!outPadding) { SetContainerError(LC_ERROR_NULL_POINTER, "outPadding is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outPadding = container->GetPaddingBottom(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetPaddingBottom failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_padding_bottom(LCComponentHandle handle, float padding) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetPaddingBottom(padding); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetPaddingBottom failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_padding(LCComponentHandle handle, LCVec4* outPadding) {
    if (!outPadding) { SetContainerError(LC_ERROR_NULL_POINTER, "outPadding is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outPadding = FromEngineVec4(container->GetPadding()); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetPadding failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_padding(LCComponentHandle handle, LCVec4 padding) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetPadding(ToEngineVec4(padding)); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetPadding failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Margin ----- */

LC_API LCResult lc_container_get_margin_linked(LCComponentHandle handle, bool* outLinked) {
    if (!outLinked) { SetContainerError(LC_ERROR_NULL_POINTER, "outLinked is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLinked = container->GetMarginLinked(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetMarginLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_margin_linked(LCComponentHandle handle, bool linked) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetMarginLinked(linked); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetMarginLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_margin_left(LCComponentHandle handle, float* outMargin) {
    if (!outMargin) { SetContainerError(LC_ERROR_NULL_POINTER, "outMargin is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outMargin = container->GetMarginLeft(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetMarginLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_margin_left(LCComponentHandle handle, float margin) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetMarginLeft(margin); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetMarginLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_margin_right(LCComponentHandle handle, float* outMargin) {
    if (!outMargin) { SetContainerError(LC_ERROR_NULL_POINTER, "outMargin is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outMargin = container->GetMarginRight(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetMarginRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_margin_right(LCComponentHandle handle, float margin) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetMarginRight(margin); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetMarginRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_margin_top(LCComponentHandle handle, float* outMargin) {
    if (!outMargin) { SetContainerError(LC_ERROR_NULL_POINTER, "outMargin is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outMargin = container->GetMarginTop(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetMarginTop failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_margin_top(LCComponentHandle handle, float margin) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetMarginTop(margin); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetMarginTop failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_margin_bottom(LCComponentHandle handle, float* outMargin) {
    if (!outMargin) { SetContainerError(LC_ERROR_NULL_POINTER, "outMargin is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outMargin = container->GetMarginBottom(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetMarginBottom failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_margin_bottom(LCComponentHandle handle, float margin) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetMarginBottom(margin); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetMarginBottom failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_margin(LCComponentHandle handle, LCVec4* outMargin) {
    if (!outMargin) { SetContainerError(LC_ERROR_NULL_POINTER, "outMargin is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outMargin = FromEngineVec4(container->GetMargin()); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetMargin failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_margin(LCComponentHandle handle, LCVec4 margin) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetMargin(ToEngineVec4(margin)); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetMargin failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Background ----- */

LC_API LCResult lc_container_get_background_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetContainerError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(container->GetBackgroundColor()); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_background_color(LCComponentHandle handle, LCColor color) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetBackgroundColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_opacity(LCComponentHandle handle, float* outOpacity) {
    if (!outOpacity) { SetContainerError(LC_ERROR_NULL_POINTER, "outOpacity is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outOpacity = container->GetOpacity(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetOpacity failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_opacity(LCComponentHandle handle, float opacity) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetOpacity(opacity); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetOpacity failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_draw_background(LCComponentHandle handle, bool* outDraw) {
    if (!outDraw) { SetContainerError(LC_ERROR_NULL_POINTER, "outDraw is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outDraw = container->GetDrawBackground(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetDrawBackground failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_draw_background(LCComponentHandle handle, bool draw) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetDrawBackground(draw); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetDrawBackground failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Border ----- */

LC_API LCResult lc_container_get_border_enabled(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) { SetContainerError(LC_ERROR_NULL_POINTER, "outEnabled is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outEnabled = container->GetBorderEnabled(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetBorderEnabled failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_border_enabled(LCComponentHandle handle, bool enabled) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetBorderEnabled(enabled); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetBorderEnabled failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_border_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetContainerError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(container->GetBorderColor()); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetBorderColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_border_color(LCComponentHandle handle, LCColor color) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetBorderColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetBorderColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_border_width_linked(LCComponentHandle handle, bool* outLinked) {
    if (!outLinked) { SetContainerError(LC_ERROR_NULL_POINTER, "outLinked is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLinked = container->GetBorderWidthLinked(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidthLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_border_width_linked(LCComponentHandle handle, bool linked) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetBorderWidthLinked(linked); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_border_width_left(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetContainerError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = container->GetBorderWidthLeft(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidthLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_border_width_left(LCComponentHandle handle, float width) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetBorderWidthLeft(width); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_border_width_right(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetContainerError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = container->GetBorderWidthRight(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidthRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_border_width_right(LCComponentHandle handle, float width) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetBorderWidthRight(width); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_border_width_top(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetContainerError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = container->GetBorderWidthTop(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidthTop failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_border_width_top(LCComponentHandle handle, float width) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetBorderWidthTop(width); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthTop failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_border_width_bottom(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetContainerError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = container->GetBorderWidthBottom(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidthBottom failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_border_width_bottom(LCComponentHandle handle, float width) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetBorderWidthBottom(width); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthBottom failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Corner Radius ----- */

LC_API LCResult lc_container_get_corner_radius_linked(LCComponentHandle handle, bool* outLinked) {
    if (!outLinked) { SetContainerError(LC_ERROR_NULL_POINTER, "outLinked is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLinked = container->GetCornerRadiusLinked(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadiusLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_corner_radius_linked(LCComponentHandle handle, bool linked) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetCornerRadiusLinked(linked); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_corner_radius_top_left(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) { SetContainerError(LC_ERROR_NULL_POINTER, "outRadius is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outRadius = container->GetCornerRadiusTopLeft(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadiusTopLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_corner_radius_top_left(LCComponentHandle handle, float radius) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetCornerRadiusTopLeft(radius); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusTopLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_corner_radius_top_right(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) { SetContainerError(LC_ERROR_NULL_POINTER, "outRadius is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outRadius = container->GetCornerRadiusTopRight(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadiusTopRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_corner_radius_top_right(LCComponentHandle handle, float radius) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetCornerRadiusTopRight(radius); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusTopRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_corner_radius_bottom_left(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) { SetContainerError(LC_ERROR_NULL_POINTER, "outRadius is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outRadius = container->GetCornerRadiusBottomLeft(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadiusBottomLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_corner_radius_bottom_left(LCComponentHandle handle, float radius) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetCornerRadiusBottomLeft(radius); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusBottomLeft failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_corner_radius_bottom_right(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) { SetContainerError(LC_ERROR_NULL_POINTER, "outRadius is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outRadius = container->GetCornerRadiusBottomRight(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadiusBottomRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_corner_radius_bottom_right(LCComponentHandle handle, float radius) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetCornerRadiusBottomRight(radius); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusBottomRight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Layout ----- */

LC_API LCResult lc_container_get_clip_children(LCComponentHandle handle, bool* outClip) {
    if (!outClip) { SetContainerError(LC_ERROR_NULL_POINTER, "outClip is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outClip = container->GetClipChildren(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetClipChildren failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_clip_children(LCComponentHandle handle, bool clip) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetClipChildren(clip); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetClipChildren failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_separation(LCComponentHandle handle, float* outSeparation) {
    if (!outSeparation) { SetContainerError(LC_ERROR_NULL_POINTER, "outSeparation is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSeparation = container->GetSeparation(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetSeparation failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_separation(LCComponentHandle handle, float separation) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetSeparation(separation); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetSeparation failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_invalidate_layout(LCComponentHandle handle) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->InvalidateLayout(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "InvalidateLayout failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_force_layout_update(LCComponentHandle handle) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->ForceLayoutUpdate(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "ForceLayoutUpdate failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Rendering ----- */

LC_API LCResult lc_container_get_layer(LCComponentHandle handle, int* outLayer) {
    if (!outLayer) { SetContainerError(LC_ERROR_NULL_POINTER, "outLayer is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLayer = container->GetLayer(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetLayer failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_layer(LCComponentHandle handle, int layer) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetLayer(layer); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetLayer failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_sorting_order(LCComponentHandle handle, int* outOrder) {
    if (!outOrder) { SetContainerError(LC_ERROR_NULL_POINTER, "outOrder is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outOrder = container->GetSortingOrder(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetSortingOrder failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_sorting_order(LCComponentHandle handle, int order) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetSortingOrder(order); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetSortingOrder failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_get_use_ui_space(LCComponentHandle handle, bool* outUseUISpace) {
    if (!outUseUISpace) { SetContainerError(LC_ERROR_NULL_POINTER, "outUseUISpace is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outUseUISpace = container->GetUseUISpace(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetUseUISpace failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_container_set_use_ui_space(LCComponentHandle handle, bool useUISpace) {
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetUseUISpace(useUISpace); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetUseUISpace failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Child Info ----- */

LC_API LCResult lc_container_get_child_count(LCComponentHandle handle, int* outCount) {
    if (!outCount) { SetContainerError(LC_ERROR_NULL_POINTER, "outCount is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid Container handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outCount = container->GetChildCount(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetChildCount failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ============================================================================
 * VerticalContainer Component
 * ============================================================================ */

LC_API LCResult lc_vertical_container_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetContainerError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto component = std::make_shared<VerticalContainer>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetContainerError(LC_ERROR_INTERNAL_ERROR, "Failed to create VerticalContainer component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vertical_container_get_horizontal_alignment(LCComponentHandle handle, LCVerticalContainerHAlign* outAlign) {
    if (!outAlign) { SetContainerError(LC_ERROR_NULL_POINTER, "outAlign is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetVerticalContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid VerticalContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outAlign = static_cast<LCVerticalContainerHAlign>(container->GetHorizontalAlignment()); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetHorizontalAlignment failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_vertical_container_set_horizontal_alignment(LCComponentHandle handle, LCVerticalContainerHAlign align) {
    auto* container = GetVerticalContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid VerticalContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetHorizontalAlignment(static_cast<VerticalContainer::HorizontalAlignment>(align)); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetHorizontalAlignment failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_vertical_container_get_vertical_alignment(LCComponentHandle handle, LCVerticalContainerVAlign* outAlign) {
    if (!outAlign) { SetContainerError(LC_ERROR_NULL_POINTER, "outAlign is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetVerticalContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid VerticalContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outAlign = static_cast<LCVerticalContainerVAlign>(container->GetVerticalAlignment()); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetVerticalAlignment failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_vertical_container_set_vertical_alignment(LCComponentHandle handle, LCVerticalContainerVAlign align) {
    auto* container = GetVerticalContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid VerticalContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetVerticalAlignment(static_cast<VerticalContainer::VerticalAlignment>(align)); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetVerticalAlignment failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ============================================================================
 * HorizontalContainer Component
 * ============================================================================ */

LC_API LCResult lc_horizontal_container_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetContainerError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto component = std::make_shared<HorizontalContainer>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetContainerError(LC_ERROR_INTERNAL_ERROR, "Failed to create HorizontalContainer component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_horizontal_container_get_vertical_alignment(LCComponentHandle handle, LCHorizontalContainerVAlign* outAlign) {
    if (!outAlign) { SetContainerError(LC_ERROR_NULL_POINTER, "outAlign is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetHorizontalContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid HorizontalContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outAlign = static_cast<LCHorizontalContainerVAlign>(container->GetVerticalAlignment()); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetVerticalAlignment failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_horizontal_container_set_vertical_alignment(LCComponentHandle handle, LCHorizontalContainerVAlign align) {
    auto* container = GetHorizontalContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid HorizontalContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetVerticalAlignment(static_cast<HorizontalContainer::VerticalAlignment>(align)); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetVerticalAlignment failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_horizontal_container_get_horizontal_alignment(LCComponentHandle handle, LCHorizontalContainerHAlign* outAlign) {
    if (!outAlign) { SetContainerError(LC_ERROR_NULL_POINTER, "outAlign is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetHorizontalContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid HorizontalContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outAlign = static_cast<LCHorizontalContainerHAlign>(container->GetHorizontalAlignment()); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetHorizontalAlignment failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_horizontal_container_set_horizontal_alignment(LCComponentHandle handle, LCHorizontalContainerHAlign align) {
    auto* container = GetHorizontalContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid HorizontalContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetHorizontalAlignment(static_cast<HorizontalContainer::HorizontalAlignment>(align)); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetHorizontalAlignment failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ============================================================================
 * GridContainer Component
 * ============================================================================ */

LC_API LCResult lc_grid_container_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetContainerError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto component = std::make_shared<GridContainer>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetContainerError(LC_ERROR_INTERNAL_ERROR, "Failed to create GridContainer component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_grid_container_get_use_rows(LCComponentHandle handle, bool* outUseRows) {
    if (!outUseRows) { SetContainerError(LC_ERROR_NULL_POINTER, "outUseRows is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outUseRows = container->GetUseRows(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetUseRows failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_set_use_rows(LCComponentHandle handle, bool useRows) {
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetUseRows(useRows); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetUseRows failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_get_row_count(LCComponentHandle handle, int* outCount) {
    if (!outCount) { SetContainerError(LC_ERROR_NULL_POINTER, "outCount is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outCount = container->GetRowCount(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetRowCount failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_set_row_count(LCComponentHandle handle, int count) {
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetRowCount(count); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetRowCount failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_get_column_count(LCComponentHandle handle, int* outCount) {
    if (!outCount) { SetContainerError(LC_ERROR_NULL_POINTER, "outCount is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outCount = container->GetColumnCount(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetColumnCount failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_set_column_count(LCComponentHandle handle, int count) {
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetColumnCount(count); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetColumnCount failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_get_cell_sizing_mode(LCComponentHandle handle, LCGridCellSizingMode* outMode) {
    if (!outMode) { SetContainerError(LC_ERROR_NULL_POINTER, "outMode is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outMode = static_cast<LCGridCellSizingMode>(container->GetCellSizingMode()); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetCellSizingMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_set_cell_sizing_mode(LCComponentHandle handle, LCGridCellSizingMode mode) {
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetCellSizingMode(static_cast<GridContainer::CellSizingMode>(mode)); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetCellSizingMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_get_fixed_cell_width(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetContainerError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = container->GetFixedCellWidth(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetFixedCellWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_set_fixed_cell_width(LCComponentHandle handle, float width) {
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetFixedCellWidth(width); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetFixedCellWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_get_fixed_cell_height(LCComponentHandle handle, float* outHeight) {
    if (!outHeight) { SetContainerError(LC_ERROR_NULL_POINTER, "outHeight is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outHeight = container->GetFixedCellHeight(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetFixedCellHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_set_fixed_cell_height(LCComponentHandle handle, float height) {
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetFixedCellHeight(height); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetFixedCellHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_get_horizontal_spacing(LCComponentHandle handle, float* outSpacing) {
    if (!outSpacing) { SetContainerError(LC_ERROR_NULL_POINTER, "outSpacing is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSpacing = container->GetHorizontalSpacing(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetHorizontalSpacing failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_set_horizontal_spacing(LCComponentHandle handle, float spacing) {
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetHorizontalSpacing(spacing); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetHorizontalSpacing failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_get_vertical_spacing(LCComponentHandle handle, float* outSpacing) {
    if (!outSpacing) { SetContainerError(LC_ERROR_NULL_POINTER, "outSpacing is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSpacing = container->GetVerticalSpacing(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetVerticalSpacing failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_set_vertical_spacing(LCComponentHandle handle, float spacing) {
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetVerticalSpacing(spacing); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetVerticalSpacing failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_get_flow_direction(LCComponentHandle handle, LCGridFlowDirection* outDirection) {
    if (!outDirection) { SetContainerError(LC_ERROR_NULL_POINTER, "outDirection is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outDirection = static_cast<LCGridFlowDirection>(container->GetFlowDirection()); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetFlowDirection failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_set_flow_direction(LCComponentHandle handle, LCGridFlowDirection direction) {
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetFlowDirection(static_cast<GridContainer::FlowDirection>(direction)); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetFlowDirection failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_get_homogeneous_cells(LCComponentHandle handle, bool* outHomogeneous) {
    if (!outHomogeneous) { SetContainerError(LC_ERROR_NULL_POINTER, "outHomogeneous is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outHomogeneous = container->GetHomogeneousCells(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetHomogeneousCells failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_set_homogeneous_cells(LCComponentHandle handle, bool homogeneous) {
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetHomogeneousCells(homogeneous); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetHomogeneousCells failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_get_size_children(LCComponentHandle handle, bool* outSizeChildren) {
    if (!outSizeChildren) { SetContainerError(LC_ERROR_NULL_POINTER, "outSizeChildren is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSizeChildren = container->GetSizeChildren(); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "GetSizeChildren failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_grid_container_set_size_children(LCComponentHandle handle, bool sizeChildren) {
    auto* container = GetGridContainer(handle);
    if (!container) { SetContainerError(LC_ERROR_INVALID_HANDLE, "Invalid GridContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { container->SetSizeChildren(sizeChildren); return LC_SUCCESS; }
    catch (...) { SetContainerError(LC_ERROR_INTERNAL_ERROR, "SetSizeChildren failed"); return LC_ERROR_INTERNAL_ERROR; }
}

// Note: CenterContainer functions are in lc_center_container.cpp
