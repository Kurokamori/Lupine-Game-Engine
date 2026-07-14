/**
 * @file lc_parallax_background.cpp
 * @brief Implementation of ParallaxBackground C API
 */

#include "components/lc_parallax_background.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/ParallaxBackground.hpp>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

lupine::math::Vec2 ToEngineVec2(LCVec2 v) {
    return lupine::math::Vec2(v.x, v.y);
}

LCVec2 FromEngineVec2(const lupine::math::Vec2& v) {
    return LCVec2{v.x, v.y};
}

ParallaxBackground* GetParallaxBackground(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<ParallaxBackground*>(comp.get());
}

} // anonymous namespace

LC_API LCResult lc_parallax_background_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto comp = std::make_shared<ParallaxBackground>();
        comp->RegisterProperties();
        nodePtr->AddComponent(comp);
        *outHandle = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create ParallaxBackground component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_parallax_background_set_scroll_scale(LCComponentHandle handle, LCVec2 scale) {
    auto pb = GetParallaxBackground(handle);
    if (!pb) return LC_ERROR_INVALID_HANDLE;
    pb->SetScrollScale(ToEngineVec2(scale));
    return LC_SUCCESS;
}

LC_API LCResult lc_parallax_background_get_scroll_scale(LCComponentHandle handle, LCVec2* outScale) {
    if (!outScale) return LC_ERROR_NULL_POINTER;
    auto pb = GetParallaxBackground(handle);
    if (!pb) return LC_ERROR_INVALID_HANDLE;
    *outScale = FromEngineVec2(pb->GetScrollScale());
    return LC_SUCCESS;
}

LC_API LCResult lc_parallax_background_set_scroll_base_offset(LCComponentHandle handle, LCVec2 offset) {
    auto pb = GetParallaxBackground(handle);
    if (!pb) return LC_ERROR_INVALID_HANDLE;
    pb->SetScrollBaseOffset(ToEngineVec2(offset));
    return LC_SUCCESS;
}

LC_API LCResult lc_parallax_background_get_scroll_base_offset(LCComponentHandle handle, LCVec2* outOffset) {
    if (!outOffset) return LC_ERROR_NULL_POINTER;
    auto pb = GetParallaxBackground(handle);
    if (!pb) return LC_ERROR_INVALID_HANDLE;
    *outOffset = FromEngineVec2(pb->GetScrollBaseOffset());
    return LC_SUCCESS;
}

LC_API LCResult lc_parallax_background_set_ignore_camera_scroll(LCComponentHandle handle, bool ignore) {
    auto pb = GetParallaxBackground(handle);
    if (!pb) return LC_ERROR_INVALID_HANDLE;
    pb->SetIgnoreCameraScroll(ignore);
    return LC_SUCCESS;
}

LC_API LCResult lc_parallax_background_get_ignore_camera_scroll(LCComponentHandle handle, bool* outIgnore) {
    if (!outIgnore) return LC_ERROR_NULL_POINTER;
    auto pb = GetParallaxBackground(handle);
    if (!pb) return LC_ERROR_INVALID_HANDLE;
    *outIgnore = pb->GetIgnoreCameraScroll();
    return LC_SUCCESS;
}

LC_API LCResult lc_parallax_background_set_scroll_offset(LCComponentHandle handle, LCVec2 scroll) {
    auto pb = GetParallaxBackground(handle);
    if (!pb) return LC_ERROR_INVALID_HANDLE;
    pb->SetScrollOffset(ToEngineVec2(scroll));
    return LC_SUCCESS;
}

LC_API LCResult lc_parallax_background_get_scroll_offset(LCComponentHandle handle, LCVec2* outScroll) {
    if (!outScroll) return LC_ERROR_NULL_POINTER;
    auto pb = GetParallaxBackground(handle);
    if (!pb) return LC_ERROR_INVALID_HANDLE;
    *outScroll = FromEngineVec2(pb->GetScrollOffset());
    return LC_SUCCESS;
}

LC_API LCResult lc_parallax_background_get_applied_scroll(LCComponentHandle handle, LCVec2* outScroll) {
    if (!outScroll) return LC_ERROR_NULL_POINTER;
    auto pb = GetParallaxBackground(handle);
    if (!pb) return LC_ERROR_INVALID_HANDLE;
    *outScroll = FromEngineVec2(pb->GetAppliedScroll());
    return LC_SUCCESS;
}

LC_API LCResult lc_parallax_background_update_layers(LCComponentHandle handle) {
    auto pb = GetParallaxBackground(handle);
    if (!pb) return LC_ERROR_INVALID_HANDLE;
    pb->UpdateLayers();
    return LC_SUCCESS;
}
