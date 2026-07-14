/**
 * @file lc_shapecast3d.cpp
 * @brief Implementation of ShapeCast3D C API
 */

#include "physics/lc_shapecast3d.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/ShapeCast3D.hpp>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

lupine::math::Vec3 ToEngineVec3(LCVec3 v) {
    return lupine::math::Vec3(v.x, v.y, v.z);
}

LCVec3 FromEngineVec3(const lupine::math::Vec3& v) {
    return LCVec3{v.x, v.y, v.z};
}

ShapeCast3D* GetShapeCast3D(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<ShapeCast3D*>(comp.get());
}

} // anonymous namespace

LC_API LCResult lc_shapecast3d_create(LCNodeHandle node, LCComponentHandle* outHandle) {
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
        auto comp = std::make_shared<ShapeCast3D>();
        comp->RegisterProperties();
        nodePtr->AddComponent(comp);
        *outHandle = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create ShapeCast3D component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_shapecast3d_set_target_position(LCComponentHandle handle, LCVec3 target) {
    auto sc = GetShapeCast3D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    sc->SetTargetPosition(ToEngineVec3(target));
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast3d_get_target_position(LCComponentHandle handle, LCVec3* outTarget) {
    if (!outTarget) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast3D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    *outTarget = FromEngineVec3(sc->GetTargetPosition());
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast3d_set_shape_radius(LCComponentHandle handle, float radius) {
    auto sc = GetShapeCast3D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    sc->SetShapeRadius(radius);
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast3d_get_shape_radius(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast3D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    *outRadius = sc->GetShapeRadius();
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast3d_set_exclude_parent(LCComponentHandle handle, bool exclude) {
    auto sc = GetShapeCast3D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    sc->SetExcludeParent(exclude);
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast3d_get_exclude_parent(LCComponentHandle handle, bool* outExclude) {
    if (!outExclude) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast3D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    *outExclude = sc->GetExcludeParent();
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast3d_set_visible_in_game(LCComponentHandle handle, bool visible) {
    auto sc = GetShapeCast3D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    sc->SetVisibleInGame(visible);
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast3d_get_visible_in_game(LCComponentHandle handle, bool* outVisible) {
    if (!outVisible) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast3D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    *outVisible = sc->GetVisibleInGame();
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast3d_force_update(LCComponentHandle handle) {
    auto sc = GetShapeCast3D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    sc->ForceShapecastUpdate();
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast3d_is_colliding(LCComponentHandle handle, bool* outColliding) {
    if (!outColliding) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast3D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    *outColliding = sc->IsColliding();
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast3d_get_collider(LCComponentHandle handle, LCNodeHandle* outNode) {
    if (!outNode) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast3D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;

    Node* collider = sc->GetCollider();
    if (!collider) {
        *outNode = nullptr;
        return LC_ERROR_NOT_FOUND;
    }

    *outNode = CreateHandle(collider->shared_from_this());
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast3d_get_collision_point(LCComponentHandle handle, LCVec3* outPoint) {
    if (!outPoint) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast3D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    *outPoint = FromEngineVec3(sc->GetCollisionPoint());
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast3d_get_collision_normal(LCComponentHandle handle, LCVec3* outNormal) {
    if (!outNormal) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast3D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    *outNormal = FromEngineVec3(sc->GetCollisionNormal());
    return LC_SUCCESS;
}

LC_API LCResult lc_shapecast3d_get_collision_fraction(LCComponentHandle handle, float* outFraction) {
    if (!outFraction) return LC_ERROR_NULL_POINTER;
    auto sc = GetShapeCast3D(handle);
    if (!sc) return LC_ERROR_INVALID_HANDLE;
    *outFraction = sc->GetCollisionFraction();
    return LC_SUCCESS;
}
