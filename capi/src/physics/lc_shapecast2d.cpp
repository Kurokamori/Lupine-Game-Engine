/**
 * @file lc_shapecast2d.cpp
 * @brief Implementation of ShapeCast2D C API
 */

#include "physics/lc_shapecast2d.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/ShapeCast2D.hpp>

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

ShapeCast2D* GetShapeCast2D(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<ShapeCast2D*>(comp.get());
}

} // anonymous namespace

LC_API LCResult lc_shapecast2d_create(LCNodeHandle node, LCComponentHandle* outHandle) {
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
        auto comp = std::make_shared<ShapeCast2D>();
        comp->RegisterProperties();
        nodePtr->AddComponent(comp);
        *outHandle = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create ShapeCast2D component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_shapecast2d_set_target_position(LCComponentHandle handle, LCVec2 target) {
    auto sc = GetShapeCast2D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    sc->SetTargetPosition(ToEngineVec2(target));
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast2d_get_target_position(LCComponentHandle handle, LCVec2* outTarget) {
    if (!outTarget) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast2D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    *outTarget = FromEngineVec2(sc->GetTargetPosition());
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast2d_set_shape_radius(LCComponentHandle handle, float radius) {
    auto sc = GetShapeCast2D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    sc->SetShapeRadius(radius);
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast2d_get_shape_radius(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast2D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    *outRadius = sc->GetShapeRadius();
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast2d_set_collision_mask(LCComponentHandle handle, uint32_t mask) {
    auto sc = GetShapeCast2D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    sc->SetCollisionMask(mask);
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast2d_get_collision_mask(LCComponentHandle handle, uint32_t* outMask) {
    if (!outMask) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast2D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    *outMask = sc->GetCollisionMask();
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast2d_set_exclude_parent(LCComponentHandle handle, bool exclude) {
    auto sc = GetShapeCast2D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    sc->SetExcludeParent(exclude);
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast2d_get_exclude_parent(LCComponentHandle handle, bool* outExclude) {
    if (!outExclude) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast2D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    *outExclude = sc->GetExcludeParent();
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast2d_set_visible_in_game(LCComponentHandle handle, bool visible) {
    auto sc = GetShapeCast2D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    sc->SetVisibleInGame(visible);
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast2d_get_visible_in_game(LCComponentHandle handle, bool* outVisible) {
    if (!outVisible) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast2D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    *outVisible = sc->GetVisibleInGame();
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast2d_force_update(LCComponentHandle handle) {
    auto sc = GetShapeCast2D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    sc->ForceShapecastUpdate();
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast2d_is_colliding(LCComponentHandle handle, bool* outColliding) {
    if (!outColliding) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast2D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    *outColliding = sc->IsColliding();
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast2d_get_collider(LCComponentHandle handle, LCNodeHandle* outNode) {
    if (!outNode) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast2D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;

    Node* collider = sc->GetCollider();
    if (!collider) {
        *outNode = nullptr;
        return LC_ERROR_NOT_FOUND;
    }

    *outNode = CreateHandle(collider->shared_from_this());
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast2d_get_collision_point(LCComponentHandle handle, LCVec2* outPoint) {
    if (!outPoint) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast2D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    *outPoint = FromEngineVec2(sc->GetCollisionPoint());
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast2d_get_collision_normal(LCComponentHandle handle, LCVec2* outNormal) {
    if (!outNormal) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast2D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    *outNormal = FromEngineVec2(sc->GetCollisionNormal());
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast2d_get_collision_fraction(LCComponentHandle handle, float* outFraction) {
    if (!outFraction) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast2D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    *outFraction = sc->GetCollisionFraction();
    return LC_SUCCESS;
}
