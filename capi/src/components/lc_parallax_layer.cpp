/**
 * @file lc_parallax_layer.cpp
 * @brief Implementation of ParallaxLayer C API
 */

#include "components/lc_parallax_layer.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/ParallaxLayer.hpp>

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

ParallaxLayer* GetParallaxLayer(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<ParallaxLayer*>(comp.get());
}

} // anonymous namespace

LC_API LCResult lc_parallax_layer_create(LCNodeHandle node, LCComponentHandle* outHandle) {
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
        auto comp = std::make_shared<ParallaxLayer>();
        comp->RegisterProperties();
        nodePtr->AddComponent(comp);
        *outHandle = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create ParallaxLayer component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_parallax_layer_set_motion_scale(LCComponentHandle handle, LCVec2 scale) {
    auto pl = GetParallaxLayer(handle);
    if (!pl) return LC_ERROR_INVALID_HANDLE;
    pl->SetMotionScale(ToEngineVec2(scale));
    return LC_SUCCESS;
}

LC_API LCResult lc_parallax_layer_get_motion_scale(LCComponentHandle handle, LCVec2* outScale) {
    if (!outScale) return LC_ERROR_NULL_POINTER;
    auto pl = GetParallaxLayer(handle);
    if (!pl) return LC_ERROR_INVALID_HANDLE;
    *outScale = FromEngineVec2(pl->GetMotionScale());
    return LC_SUCCESS;
}

LC_API LCResult lc_parallax_layer_set_motion_offset(LCComponentHandle handle, LCVec2 offset) {
    auto pl = GetParallaxLayer(handle);
    if (!pl) return LC_ERROR_INVALID_HANDLE;
    pl->SetMotionOffset(ToEngineVec2(offset));
    return LC_SUCCESS;
}

LC_API LCResult lc_parallax_layer_get_motion_offset(LCComponentHandle handle, LCVec2* outOffset) {
    if (!outOffset) return LC_ERROR_NULL_POINTER;
    auto pl = GetParallaxLayer(handle);
    if (!pl) return LC_ERROR_INVALID_HANDLE;
    *outOffset = FromEngineVec2(pl->GetMotionOffset());
    return LC_SUCCESS;
}

LC_API LCResult lc_parallax_layer_set_motion_mirroring(LCComponentHandle handle, LCVec2 mirroring) {
    auto pl = GetParallaxLayer(handle);
    if (!pl) return LC_ERROR_INVALID_HANDLE;
    pl->SetMotionMirroring(ToEngineVec2(mirroring));
    return LC_SUCCESS;
}

LC_API LCResult lc_parallax_layer_get_motion_mirroring(LCComponentHandle handle, LCVec2* outMirroring) {
    if (!outMirroring) return LC_ERROR_NULL_POINTER;
    auto pl = GetParallaxLayer(handle);
    if (!pl) return LC_ERROR_INVALID_HANDLE;
    *outMirroring = FromEngineVec2(pl->GetMotionMirroring());
    return LC_SUCCESS;
}

LC_API LCResult lc_parallax_layer_get_home_position(LCComponentHandle handle, LCVec2* outHome) {
    if (!outHome) return LC_ERROR_NULL_POINTER;
    auto pl = GetParallaxLayer(handle);
    if (!pl) return LC_ERROR_INVALID_HANDLE;
    *outHome = FromEngineVec2(pl->GetHomePosition());
    return LC_SUCCESS;
}

LC_API LCResult lc_parallax_layer_capture_home_position(LCComponentHandle handle) {
    auto pl = GetParallaxLayer(handle);
    if (!pl) return LC_ERROR_INVALID_HANDLE;
    pl->CaptureHomePosition();
    return LC_SUCCESS;
}

LC_API LCResult lc_parallax_layer_apply_scroll(LCComponentHandle handle, LCVec2 scroll) {
    auto pl = GetParallaxLayer(handle);
    if (!pl) return LC_ERROR_INVALID_HANDLE;
    pl->ApplyScroll(ToEngineVec2(scroll));
    return LC_SUCCESS;
}
