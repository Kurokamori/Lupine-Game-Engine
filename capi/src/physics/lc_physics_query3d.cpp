/**
 * @file lc_physics_query3d.cpp
 * @brief Implementation of Lupine Engine C API - 3D Physics Query System
 */

#include "physics/lc_physics_query3d.h"
#include "../core/lc_internal.h"

#include <lupine/core/SceneManager.hpp>
#include <lupine/core/Node.hpp>
#include <lupine/physics3d/Physics3DWorld.hpp>

#include <algorithm>

namespace {

void SetQueryError(LCResult code, const char* message) {
    ::SetError(code, message);
}

// Convert C API Vec3 to engine Vec3
lupine::math::Vec3 ToEngineVec3(LCVec3 vec) {
    return lupine::math::Vec3(vec.x, vec.y, vec.z);
}

// Convert engine Vec3 to C API Vec3
LCVec3 FromEngineVec3(const lupine::math::Vec3& vec) {
    return LCVec3{vec.x, vec.y, vec.z};
}

// Convert C API Quat to engine Quat
lupine::math::Quat ToEngineQuat(LCQuat q) {
    return lupine::math::Quat(q.x, q.y, q.z, q.w);
}

// Convert engine UUID to C API bodyId array
void UUIDToBodyId(const lupine::core::UUID& uuid, uint64_t* bodyId) {
    bodyId[0] = static_cast<uint64_t>(uuid);
    bodyId[1] = 0; // Engine uses 64-bit UUIDs
}

// Convert C API bodyId array to engine UUID
lupine::core::UUID BodyIdToUUID(const uint64_t* bodyId) {
    return lupine::core::UUID(bodyId[0]);
}

// Get the Physics3DWorld from SceneManager
lupine::physics3d::Physics3DWorld* GetPhysics3DWorld() {
    auto* sceneManager = lupine::core::SceneManager::GetInstance();
    if (!sceneManager) {
        return nullptr;
    }
    return sceneManager->GetPhysics3DWorld();
}

// Convert engine RaycastHit3D to C API LCRaycastHit3D
void ConvertRaycastHit(const lupine::physics3d::RaycastHit3D& src, LCRaycastHit3D* dst) {
    UUIDToBodyId(src.bodyId, dst->bodyId);
    dst->point = FromEngineVec3(src.point);
    dst->normal = FromEngineVec3(src.normal);
    dst->fraction = src.fraction;
    dst->hit = src.hit;
}

// Convert engine OverlapResult3D to C API LCOverlapResult3D
void ConvertOverlapResult(const lupine::physics3d::OverlapResult3D& src, LCOverlapResult3D* dst) {
    UUIDToBodyId(src.bodyId, dst->bodyId);
}

// Convert engine ShapeCastHit3D to C API LCShapeCastHit3D
void ConvertShapeCastHit(const lupine::physics3d::ShapeCastHit3D& src, LCShapeCastHit3D* dst) {
    UUIDToBodyId(src.bodyId, dst->bodyId);
    dst->point = FromEngineVec3(src.point);
    dst->normal = FromEngineVec3(src.normal);
    dst->fraction = src.fraction;
    dst->hit = src.hit;
}

} // anonymous namespace

/* ============================================================================
 * Raycast Functions
 * ============================================================================ */

LC_API LCResult lc_physics3d_raycast(LCVec3 origin, LCVec3 direction, float maxDistance, LCRaycastHit3D* hit) {
    return lc_physics3d_raycast_ignore(origin, direction, maxDistance, nullptr, hit);
}

LC_API LCResult lc_physics3d_raycast_ignore(LCVec3 origin, LCVec3 direction, float maxDistance,
                                            const uint64_t* ignoreBodyId, LCRaycastHit3D* hit) {
    if (!hit) {
        SetQueryError(LC_ERROR_NULL_POINTER, "hit is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    // Initialize hit result
    hit->hit = false;
    hit->fraction = 1.0f;
    hit->bodyId[0] = 0;
    hit->bodyId[1] = 0;

    auto* physicsWorld = GetPhysics3DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics3D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        lupine::physics3d::RaycastHit3D engineHit;

        if (ignoreBodyId) {
            lupine::core::UUID ignoreUUID = BodyIdToUUID(ignoreBodyId);
            physicsWorld->Raycast(ToEngineVec3(origin), ToEngineVec3(direction), maxDistance, engineHit, &ignoreUUID);
        } else {
            physicsWorld->Raycast(ToEngineVec3(origin), ToEngineVec3(direction), maxDistance, engineHit, nullptr);
        }

        ConvertRaycastHit(engineHit, hit);
        return LC_SUCCESS;

    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "Raycast failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_physics3d_raycast_all(LCVec3 origin, LCVec3 direction, float maxDistance,
                                         LCRaycastHit3D* hits, uint32_t maxHits, uint32_t* hitCount) {
    return lc_physics3d_raycast_all_ignore(origin, direction, maxDistance, nullptr, hits, maxHits, hitCount);
}

LC_API LCResult lc_physics3d_raycast_all_ignore(LCVec3 origin, LCVec3 direction, float maxDistance,
                                                const uint64_t* ignoreBodyId,
                                                LCRaycastHit3D* hits, uint32_t maxHits, uint32_t* hitCount) {
    if (!hits) {
        SetQueryError(LC_ERROR_NULL_POINTER, "hits is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!hitCount) {
        SetQueryError(LC_ERROR_NULL_POINTER, "hitCount is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    *hitCount = 0;

    auto* physicsWorld = GetPhysics3DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics3D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        std::vector<lupine::physics3d::RaycastHit3D> engineHits;

        if (ignoreBodyId) {
            lupine::core::UUID ignoreUUID = BodyIdToUUID(ignoreBodyId);
            engineHits = physicsWorld->RaycastAll(ToEngineVec3(origin), ToEngineVec3(direction), maxDistance, &ignoreUUID);
        } else {
            engineHits = physicsWorld->RaycastAll(ToEngineVec3(origin), ToEngineVec3(direction), maxDistance, nullptr);
        }

        uint32_t count = std::min(static_cast<uint32_t>(engineHits.size()), maxHits);
        for (uint32_t i = 0; i < count; ++i) {
            ConvertRaycastHit(engineHits[i], &hits[i]);
        }

        *hitCount = count;
        return LC_SUCCESS;

    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "RaycastAll failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Overlap Query Functions
 * ============================================================================ */

LC_API LCResult lc_physics3d_overlap_sphere(LCVec3 center, float radius,
                                            LCOverlapResult3D* results, uint32_t maxResults, uint32_t* resultCount) {
    if (!results) {
        SetQueryError(LC_ERROR_NULL_POINTER, "results is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!resultCount) {
        SetQueryError(LC_ERROR_NULL_POINTER, "resultCount is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (radius < 0.0f) {
        SetQueryError(LC_ERROR_INVALID_PARAMETER, "radius cannot be negative");
        return LC_ERROR_INVALID_PARAMETER;
    }

    *resultCount = 0;

    auto* physicsWorld = GetPhysics3DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics3D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        std::vector<lupine::physics3d::OverlapResult3D> engineResults =
            physicsWorld->OverlapSphere(ToEngineVec3(center), radius);

        uint32_t count = std::min(static_cast<uint32_t>(engineResults.size()), maxResults);
        for (uint32_t i = 0; i < count; ++i) {
            ConvertOverlapResult(engineResults[i], &results[i]);
        }

        *resultCount = count;
        return LC_SUCCESS;

    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "OverlapSphere failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_physics3d_overlap_box(LCVec3 center, LCVec3 halfExtents,
                                         LCOverlapResult3D* results, uint32_t maxResults, uint32_t* resultCount) {
    return lc_physics3d_overlap_box_rotated(center, halfExtents, lc_quat_identity(), results, maxResults, resultCount);
}

LC_API LCResult lc_physics3d_overlap_box_rotated(LCVec3 center, LCVec3 halfExtents, LCQuat rotation,
                                                 LCOverlapResult3D* results, uint32_t maxResults, uint32_t* resultCount) {
    if (!results) {
        SetQueryError(LC_ERROR_NULL_POINTER, "results is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!resultCount) {
        SetQueryError(LC_ERROR_NULL_POINTER, "resultCount is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (halfExtents.x < 0.0f || halfExtents.y < 0.0f || halfExtents.z < 0.0f) {
        SetQueryError(LC_ERROR_INVALID_PARAMETER, "halfExtents cannot have negative components");
        return LC_ERROR_INVALID_PARAMETER;
    }

    *resultCount = 0;

    auto* physicsWorld = GetPhysics3DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics3D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        std::vector<lupine::physics3d::OverlapResult3D> engineResults =
            physicsWorld->OverlapBox(ToEngineVec3(center), ToEngineVec3(halfExtents), ToEngineQuat(rotation));

        uint32_t count = std::min(static_cast<uint32_t>(engineResults.size()), maxResults);
        for (uint32_t i = 0; i < count; ++i) {
            ConvertOverlapResult(engineResults[i], &results[i]);
        }

        *resultCount = count;
        return LC_SUCCESS;

    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "OverlapBox failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Shape Cast Functions
 * ============================================================================ */

LC_API LCResult lc_physics3d_sphere_cast(LCVec3 start, LCVec3 end, float radius, LCShapeCastHit3D* hit) {
    return lc_physics3d_sphere_cast_ignore(start, end, radius, nullptr, hit);
}

LC_API LCResult lc_physics3d_sphere_cast_ignore(LCVec3 start, LCVec3 end, float radius,
                                                const uint64_t* ignoreBodyId, LCShapeCastHit3D* hit) {
    if (!hit) {
        SetQueryError(LC_ERROR_NULL_POINTER, "hit is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (radius < 0.0f) {
        SetQueryError(LC_ERROR_INVALID_PARAMETER, "radius cannot be negative");
        return LC_ERROR_INVALID_PARAMETER;
    }

    // Initialize hit result
    hit->hit = false;
    hit->fraction = 1.0f;
    hit->bodyId[0] = 0;
    hit->bodyId[1] = 0;

    auto* physicsWorld = GetPhysics3DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics3D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        lupine::physics3d::ShapeCastHit3D engineHit;

        if (ignoreBodyId) {
            lupine::core::UUID ignoreUUID = BodyIdToUUID(ignoreBodyId);
            physicsWorld->SphereCast(ToEngineVec3(start), ToEngineVec3(end), radius, engineHit, &ignoreUUID);
        } else {
            physicsWorld->SphereCast(ToEngineVec3(start), ToEngineVec3(end), radius, engineHit, nullptr);
        }

        ConvertShapeCastHit(engineHit, hit);
        return LC_SUCCESS;

    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "SphereCast failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

LC_API LCResult lc_physics3d_query_available(bool* available) {
    if (!available) {
        SetQueryError(LC_ERROR_NULL_POINTER, "available is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* physicsWorld = GetPhysics3DWorld();
    *available = (physicsWorld != nullptr);
    return LC_SUCCESS;
}

LC_API LCResult lc_physics3d_get_gravity(LCVec3* gravity) {
    if (!gravity) {
        SetQueryError(LC_ERROR_NULL_POINTER, "gravity is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* physicsWorld = GetPhysics3DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics3D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        *gravity = FromEngineVec3(physicsWorld->GetGravity());
        return LC_SUCCESS;

    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "GetGravity failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_physics3d_set_gravity(LCVec3 gravity) {
    auto* physicsWorld = GetPhysics3DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics3D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        physicsWorld->SetGravity(ToEngineVec3(gravity));
        return LC_SUCCESS;

    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "SetGravity failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_physics3d_get_time_step(float* out_time_step) {
    if (!out_time_step) {
        SetQueryError(LC_ERROR_NULL_POINTER, "out_time_step is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto* physicsWorld = GetPhysics3DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics3D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }
    try {
        *out_time_step = physicsWorld->GetTimeStep();
        return LC_SUCCESS;
    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "GetTimeStep failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_physics3d_set_time_step(float time_step) {
    auto* physicsWorld = GetPhysics3DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics3D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }
    try {
        physicsWorld->SetTimeStep(time_step);
        return LC_SUCCESS;
    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "SetTimeStep failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Body <-> Node Resolution
 * ============================================================================ */

LC_API LCResult lc_physics3d_get_body_node(const uint64_t* bodyId, LCNodeHandle* out_node) {
    if (!bodyId || !out_node) {
        SetQueryError(LC_ERROR_NULL_POINTER, "bodyId or out_node is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    *out_node = nullptr;

    auto* physicsWorld = GetPhysics3DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics3D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }
    try {
        lupine::core::Node* node = physicsWorld->GetBodyNode(BodyIdToUUID(bodyId));
        if (!node) {
            return LC_ERROR_NOT_FOUND;
        }
        *out_node = CreateHandle(node->shared_from_this());
        return LC_SUCCESS;
    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "GetBodyNode failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_physics3d_find_body_for_node(LCNodeHandle node, uint64_t* out_body_id) {
    if (!out_body_id) {
        SetQueryError(LC_ERROR_NULL_POINTER, "out_body_id is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    out_body_id[0] = 0;
    out_body_id[1] = 0;

    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetQueryError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    auto* physicsWorld = GetPhysics3DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics3D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }
    try {
        lupine::core::UUID bodyId = physicsWorld->FindBodyForNode(nodePtr.get());
        if (static_cast<uint64_t>(bodyId) == 0) {
            return LC_ERROR_NOT_FOUND;
        }
        UUIDToBodyId(bodyId, out_body_id);
        return LC_SUCCESS;
    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "FindBodyForNode failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
