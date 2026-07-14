/**
 * @file lc_raycast3d.cpp
 * @brief Implementation of RayCast3D C API
 */

#include "physics/lc_raycast3d.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/RayCast3D.hpp>

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

RayCast3D* GetRayCast3D(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<RayCast3D*>(comp.get());
}

} // anonymous namespace

LC_API LCResult lc_raycast3d_create(LCNodeHandle node, LCComponentHandle* outHandle) {
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
        auto comp = std::make_shared<RayCast3D>();
        comp->RegisterProperties();
        nodePtr->AddComponent(comp);
        *outHandle = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create RayCast3D component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_raycast3d_set_target_position(LCComponentHandle handle, LCVec3 target) {
    auto rc = GetRayCast3D(handle);
    if (!rc) return LC_ERROR_INVALID_HANDLE;
    rc->SetTargetPosition(ToEngineVec3(target));
    return LC_SUCCESS;
}

LC_API LCResult lc_raycast3d_get_target_position(LCComponentHandle handle, LCVec3* outTarget) {
    if (!outTarget) return LC_ERROR_NULL_POINTER;
    auto rc = GetRayCast3D(handle);
    if (!rc) return LC_ERROR_INVALID_HANDLE;
    *outTarget = FromEngineVec3(rc->GetTargetPosition());
    return LC_SUCCESS;
}

LC_API LCResult lc_raycast3d_set_exclude_parent(LCComponentHandle handle, bool exclude) {
    auto rc = GetRayCast3D(handle);
    if (!rc) return LC_ERROR_INVALID_HANDLE;
    rc->SetExcludeParent(exclude);
    return LC_SUCCESS;
}

LC_API LCResult lc_raycast3d_get_exclude_parent(LCComponentHandle handle, bool* outExclude) {
    if (!outExclude) return LC_ERROR_NULL_POINTER;
    auto rc = GetRayCast3D(handle);
    if (!rc) return LC_ERROR_INVALID_HANDLE;
    *outExclude = rc->GetExcludeParent();
    return LC_SUCCESS;
}

LC_API LCResult lc_raycast3d_set_visible_in_game(LCComponentHandle handle, bool visible) {
    auto rc = GetRayCast3D(handle);
    if (!rc) return LC_ERROR_INVALID_HANDLE;
    rc->SetVisibleInGame(visible);
    return LC_SUCCESS;
}

LC_API LCResult lc_raycast3d_get_visible_in_game(LCComponentHandle handle, bool* outVisible) {
    if (!outVisible) return LC_ERROR_NULL_POINTER;
    auto rc = GetRayCast3D(handle);
    if (!rc) return LC_ERROR_INVALID_HANDLE;
    *outVisible = rc->GetVisibleInGame();
    return LC_SUCCESS;
}

LC_API LCResult lc_raycast3d_force_update(LCComponentHandle handle) {
    auto rc = GetRayCast3D(handle);
    if (!rc) return LC_ERROR_INVALID_HANDLE;
    rc->ForceRaycastUpdate();
    return LC_SUCCESS;
}

LC_API LCResult lc_raycast3d_is_colliding(LCComponentHandle handle, bool* outColliding) {
    if (!outColliding) return LC_ERROR_NULL_POINTER;
    auto rc = GetRayCast3D(handle);
    if (!rc) return LC_ERROR_INVALID_HANDLE;
    *outColliding = rc->IsColliding();
    return LC_SUCCESS;
}

LC_API LCResult lc_raycast3d_get_collider(LCComponentHandle handle, LCNodeHandle* outNode) {
    if (!outNode) return LC_ERROR_NULL_POINTER;
    auto rc = GetRayCast3D(handle);
    if (!rc) return LC_ERROR_INVALID_HANDLE;

    Node* collider = rc->GetCollider();
    if (!collider) {
        *outNode = nullptr;
        return LC_ERROR_NOT_FOUND;
    }

    *outNode = CreateHandle(collider->shared_from_this());
    return LC_SUCCESS;
}

LC_API LCResult lc_raycast3d_get_collision_point(LCComponentHandle handle, LCVec3* outPoint) {
    if (!outPoint) return LC_ERROR_NULL_POINTER;
    auto rc = GetRayCast3D(handle);
    if (!rc) return LC_ERROR_INVALID_HANDLE;
    *outPoint = FromEngineVec3(rc->GetCollisionPoint());
    return LC_SUCCESS;
}

LC_API LCResult lc_raycast3d_get_collision_normal(LCComponentHandle handle, LCVec3* outNormal) {
    if (!outNormal) return LC_ERROR_NULL_POINTER;
    auto rc = GetRayCast3D(handle);
    if (!rc) return LC_ERROR_INVALID_HANDLE;
    *outNormal = FromEngineVec3(rc->GetCollisionNormal());
    return LC_SUCCESS;
}
