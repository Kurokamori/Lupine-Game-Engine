/**
 * @file lc_scroll_container.cpp
 * @brief Implementation of Lupine Engine C API - ScrollContainer component
 */

#include "ui/lc_scroll_container.h"
#include "../core/lc_internal.h"

#include <lupine/components/ScrollContainer.hpp>
#include <lupine/core/Node.hpp>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetScrollError(LCResult code, const char* message) {
    ::SetError(code, message);
}

ScrollContainer* GetScrollContainer(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<ScrollContainer*>(component.get());
}

LCVec2 FromEngineVec2(const Vec2& vec) {
    return LCVec2{vec.x, vec.y};
}

} // anonymous namespace

LC_API LCResult lc_scroll_container_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetScrollError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetScrollError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        auto component = std::make_shared<ScrollContainer>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetScrollError(LC_ERROR_INTERNAL_ERROR, "Failed to create ScrollContainer component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Scroll position ----- */

LC_API LCResult lc_scroll_container_get_h_scroll(LCComponentHandle handle, float* outValue) {
    if (!outValue) { SetScrollError(LC_ERROR_NULL_POINTER, "outValue is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sc = GetScrollContainer(handle);
    if (!sc) { SetScrollError(LC_ERROR_INVALID_HANDLE, "Invalid ScrollContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outValue = sc->GetHScroll(); return LC_SUCCESS; }
    catch (...) { SetScrollError(LC_ERROR_INTERNAL_ERROR, "GetHScroll failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_scroll_container_set_h_scroll(LCComponentHandle handle, float value) {
    auto* sc = GetScrollContainer(handle);
    if (!sc) { SetScrollError(LC_ERROR_INVALID_HANDLE, "Invalid ScrollContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sc->SetHScroll(value); return LC_SUCCESS; }
    catch (...) { SetScrollError(LC_ERROR_INTERNAL_ERROR, "SetHScroll failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_scroll_container_get_v_scroll(LCComponentHandle handle, float* outValue) {
    if (!outValue) { SetScrollError(LC_ERROR_NULL_POINTER, "outValue is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sc = GetScrollContainer(handle);
    if (!sc) { SetScrollError(LC_ERROR_INVALID_HANDLE, "Invalid ScrollContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outValue = sc->GetVScroll(); return LC_SUCCESS; }
    catch (...) { SetScrollError(LC_ERROR_INTERNAL_ERROR, "GetVScroll failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_scroll_container_set_v_scroll(LCComponentHandle handle, float value) {
    auto* sc = GetScrollContainer(handle);
    if (!sc) { SetScrollError(LC_ERROR_INVALID_HANDLE, "Invalid ScrollContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sc->SetVScroll(value); return LC_SUCCESS; }
    catch (...) { SetScrollError(LC_ERROR_INTERNAL_ERROR, "SetVScroll failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_scroll_container_get_content_size(LCComponentHandle handle, LCVec2* outSize) {
    if (!outSize) { SetScrollError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sc = GetScrollContainer(handle);
    if (!sc) { SetScrollError(LC_ERROR_INVALID_HANDLE, "Invalid ScrollContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = FromEngineVec2(sc->GetContentSize()); return LC_SUCCESS; }
    catch (...) { SetScrollError(LC_ERROR_INTERNAL_ERROR, "GetContentSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Enable flags ----- */

LC_API LCResult lc_scroll_container_get_h_scroll_enabled(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) { SetScrollError(LC_ERROR_NULL_POINTER, "outEnabled is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sc = GetScrollContainer(handle);
    if (!sc) { SetScrollError(LC_ERROR_INVALID_HANDLE, "Invalid ScrollContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outEnabled = sc->GetHScrollEnabled(); return LC_SUCCESS; }
    catch (...) { SetScrollError(LC_ERROR_INTERNAL_ERROR, "GetHScrollEnabled failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_scroll_container_set_h_scroll_enabled(LCComponentHandle handle, bool enabled) {
    auto* sc = GetScrollContainer(handle);
    if (!sc) { SetScrollError(LC_ERROR_INVALID_HANDLE, "Invalid ScrollContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sc->SetHScrollEnabled(enabled); return LC_SUCCESS; }
    catch (...) { SetScrollError(LC_ERROR_INTERNAL_ERROR, "SetHScrollEnabled failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_scroll_container_get_v_scroll_enabled(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) { SetScrollError(LC_ERROR_NULL_POINTER, "outEnabled is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sc = GetScrollContainer(handle);
    if (!sc) { SetScrollError(LC_ERROR_INVALID_HANDLE, "Invalid ScrollContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outEnabled = sc->GetVScrollEnabled(); return LC_SUCCESS; }
    catch (...) { SetScrollError(LC_ERROR_INTERNAL_ERROR, "GetVScrollEnabled failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_scroll_container_set_v_scroll_enabled(LCComponentHandle handle, bool enabled) {
    auto* sc = GetScrollContainer(handle);
    if (!sc) { SetScrollError(LC_ERROR_INVALID_HANDLE, "Invalid ScrollContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sc->SetVScrollEnabled(enabled); return LC_SUCCESS; }
    catch (...) { SetScrollError(LC_ERROR_INTERNAL_ERROR, "SetVScrollEnabled failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Appearance / behavior ----- */

LC_API LCResult lc_scroll_container_get_scroll_speed(LCComponentHandle handle, float* outSpeed) {
    if (!outSpeed) { SetScrollError(LC_ERROR_NULL_POINTER, "outSpeed is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sc = GetScrollContainer(handle);
    if (!sc) { SetScrollError(LC_ERROR_INVALID_HANDLE, "Invalid ScrollContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSpeed = sc->GetScrollSpeed(); return LC_SUCCESS; }
    catch (...) { SetScrollError(LC_ERROR_INTERNAL_ERROR, "GetScrollSpeed failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_scroll_container_set_scroll_speed(LCComponentHandle handle, float speed) {
    auto* sc = GetScrollContainer(handle);
    if (!sc) { SetScrollError(LC_ERROR_INVALID_HANDLE, "Invalid ScrollContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sc->SetScrollSpeed(speed); return LC_SUCCESS; }
    catch (...) { SetScrollError(LC_ERROR_INTERNAL_ERROR, "SetScrollSpeed failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_scroll_container_get_scroll_bar_width(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetScrollError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sc = GetScrollContainer(handle);
    if (!sc) { SetScrollError(LC_ERROR_INVALID_HANDLE, "Invalid ScrollContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = sc->GetScrollBarWidth(); return LC_SUCCESS; }
    catch (...) { SetScrollError(LC_ERROR_INTERNAL_ERROR, "GetScrollBarWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_scroll_container_set_scroll_bar_width(LCComponentHandle handle, float width) {
    auto* sc = GetScrollContainer(handle);
    if (!sc) { SetScrollError(LC_ERROR_INVALID_HANDLE, "Invalid ScrollContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sc->SetScrollBarWidth(width); return LC_SUCCESS; }
    catch (...) { SetScrollError(LC_ERROR_INTERNAL_ERROR, "SetScrollBarWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_scroll_container_get_h_scroll_bar_visibility(LCComponentHandle handle, LCScrollBarVisibility* outVis) {
    if (!outVis) { SetScrollError(LC_ERROR_NULL_POINTER, "outVis is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sc = GetScrollContainer(handle);
    if (!sc) { SetScrollError(LC_ERROR_INVALID_HANDLE, "Invalid ScrollContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outVis = static_cast<LCScrollBarVisibility>(sc->GetHScrollBarVisibility()); return LC_SUCCESS; }
    catch (...) { SetScrollError(LC_ERROR_INTERNAL_ERROR, "GetHScrollBarVisibility failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_scroll_container_set_h_scroll_bar_visibility(LCComponentHandle handle, LCScrollBarVisibility vis) {
    auto* sc = GetScrollContainer(handle);
    if (!sc) { SetScrollError(LC_ERROR_INVALID_HANDLE, "Invalid ScrollContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sc->SetHScrollBarVisibility(static_cast<ScrollContainer::ScrollBarVisibility>(vis)); return LC_SUCCESS; }
    catch (...) { SetScrollError(LC_ERROR_INTERNAL_ERROR, "SetHScrollBarVisibility failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_scroll_container_get_v_scroll_bar_visibility(LCComponentHandle handle, LCScrollBarVisibility* outVis) {
    if (!outVis) { SetScrollError(LC_ERROR_NULL_POINTER, "outVis is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sc = GetScrollContainer(handle);
    if (!sc) { SetScrollError(LC_ERROR_INVALID_HANDLE, "Invalid ScrollContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outVis = static_cast<LCScrollBarVisibility>(sc->GetVScrollBarVisibility()); return LC_SUCCESS; }
    catch (...) { SetScrollError(LC_ERROR_INTERNAL_ERROR, "GetVScrollBarVisibility failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_scroll_container_set_v_scroll_bar_visibility(LCComponentHandle handle, LCScrollBarVisibility vis) {
    auto* sc = GetScrollContainer(handle);
    if (!sc) { SetScrollError(LC_ERROR_INVALID_HANDLE, "Invalid ScrollContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sc->SetVScrollBarVisibility(static_cast<ScrollContainer::ScrollBarVisibility>(vis)); return LC_SUCCESS; }
    catch (...) { SetScrollError(LC_ERROR_INTERNAL_ERROR, "SetVScrollBarVisibility failed"); return LC_ERROR_INTERNAL_ERROR; }
}
