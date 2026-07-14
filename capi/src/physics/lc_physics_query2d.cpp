/**
 * @file lc_physics_query2d.cpp
 * @brief Implementation of Lupine Engine C API - 2D Physics Query System
 */

#include "physics/lc_physics_query2d.h"
#include "../core/lc_internal.h"

#include <lupine/core/SceneManager.hpp>
#include <lupine/core/Node.hpp>
#include <lupine/physics2d/Physics2DWorld.hpp>

#include <algorithm>

namespace {

void SetQueryError(LCResult code, const char* message) {
    ::SetError(code, message);
}

// Convert C API Vec2 to engine Vec2
lupine::math::Vec2 ToEngineVec2(LCVec2 vec) {
    return lupine::math::Vec2(vec.x, vec.y);
}

// Convert engine Vec2 to C API Vec2
LCVec2 FromEngineVec2(const lupine::math::Vec2& vec) {
    return LCVec2{vec.x, vec.y};
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

// Get the Physics2DWorld from SceneManager
lupine::physics2d::Physics2DWorld* GetPhysics2DWorld() {
    auto* sceneManager = lupine::core::SceneManager::GetInstance();
    if (!sceneManager) {
        return nullptr;
    }
    return sceneManager->GetPhysics2DWorld();
}

// Convert engine RaycastHit2D to C API LCRaycastHit2D
void ConvertRaycastHit(const lupine::physics2d::RaycastHit2D& src, LCRaycastHit2D* dst) {
    UUIDToBodyId(src.bodyId, dst->bodyId);
    dst->point = FromEngineVec2(src.point);
    dst->normal = FromEngineVec2(src.normal);
    dst->fraction = src.fraction;
    dst->hit = src.hit;
}

// Convert engine OverlapResult2D to C API LCOverlapResult2D
void ConvertOverlapResult(const lupine::physics2d::OverlapResult2D& src, LCOverlapResult2D* dst) {
    UUIDToBodyId(src.bodyId, dst->bodyId);
    dst->point = FromEngineVec2(src.point);
}

// Convert engine ShapeCastHit2D to C API LCShapeCastHit2D
void ConvertShapeCastHit(const lupine::physics2d::ShapeCastHit2D& src, LCShapeCastHit2D* dst) {
    UUIDToBodyId(src.bodyId, dst->bodyId);
    dst->point = FromEngineVec2(src.point);
    dst->normal = FromEngineVec2(src.normal);
    dst->fraction = src.fraction;
    dst->hit = src.hit;
}

} // anonymous namespace

/* ============================================================================
 * Raycast Functions
 * ============================================================================ */

LC_API LCResult lc_physics2d_raycast(LCVec2 origin, LCVec2 direction, float maxDistance, LCRaycastHit2D* hit) {
    return lc_physics2d_raycast_ignore(origin, direction, maxDistance, nullptr, hit);
}

LC_API LCResult lc_physics2d_raycast_ignore(LCVec2 origin, LCVec2 direction, float maxDistance,
                                            const uint64_t* ignoreBodyId, LCRaycastHit2D* hit) {
    if (!hit) {
        SetQueryError(LC_ERROR_NULL_POINTER, "hit is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    // Initialize hit result
    hit->hit = false;
    hit->fraction = 1.0f;
    hit->bodyId[0] = 0;
    hit->bodyId[1] = 0;

    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        lupine::physics2d::RaycastHit2D engineHit;

        if (ignoreBodyId) {
            lupine::core::UUID ignoreUUID = BodyIdToUUID(ignoreBodyId);
            physicsWorld->Raycast(ToEngineVec2(origin), ToEngineVec2(direction), maxDistance, engineHit, &ignoreUUID);
        } else {
            physicsWorld->Raycast(ToEngineVec2(origin), ToEngineVec2(direction), maxDistance, engineHit, nullptr);
        }

        ConvertRaycastHit(engineHit, hit);
        return LC_SUCCESS;

    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "Raycast failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_physics2d_raycast_all(LCVec2 origin, LCVec2 direction, float maxDistance,
                                         LCRaycastHit2D* hits, uint32_t maxHits, uint32_t* hitCount) {
    return lc_physics2d_raycast_all_ignore(origin, direction, maxDistance, nullptr, hits, maxHits, hitCount);
}

LC_API LCResult lc_physics2d_raycast_all_ignore(LCVec2 origin, LCVec2 direction, float maxDistance,
                                                const uint64_t* ignoreBodyId,
                                                LCRaycastHit2D* hits, uint32_t maxHits, uint32_t* hitCount) {
    if (!hits) {
        SetQueryError(LC_ERROR_NULL_POINTER, "hits is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!hitCount) {
        SetQueryError(LC_ERROR_NULL_POINTER, "hitCount is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    *hitCount = 0;

    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        std::vector<lupine::physics2d::RaycastHit2D> engineHits;

        if (ignoreBodyId) {
            lupine::core::UUID ignoreUUID = BodyIdToUUID(ignoreBodyId);
            engineHits = physicsWorld->RaycastAll(ToEngineVec2(origin), ToEngineVec2(direction), maxDistance, &ignoreUUID);
        } else {
            engineHits = physicsWorld->RaycastAll(ToEngineVec2(origin), ToEngineVec2(direction), maxDistance, nullptr);
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

LC_API LCResult lc_physics2d_overlap_point(LCVec2 point, LCOverlapResult2D* results,
                                           uint32_t maxResults, uint32_t* resultCount) {
    if (!results) {
        SetQueryError(LC_ERROR_NULL_POINTER, "results is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!resultCount) {
        SetQueryError(LC_ERROR_NULL_POINTER, "resultCount is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    *resultCount = 0;

    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        std::vector<lupine::physics2d::OverlapResult2D> engineResults =
            physicsWorld->OverlapPoint(ToEngineVec2(point));

        uint32_t count = std::min(static_cast<uint32_t>(engineResults.size()), maxResults);
        for (uint32_t i = 0; i < count; ++i) {
            ConvertOverlapResult(engineResults[i], &results[i]);
        }

        *resultCount = count;
        return LC_SUCCESS;

    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "OverlapPoint failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_physics2d_overlap_circle(LCVec2 center, float radius,
                                            LCOverlapResult2D* results, uint32_t maxResults, uint32_t* resultCount) {
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

    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        std::vector<lupine::physics2d::OverlapResult2D> engineResults =
            physicsWorld->OverlapCircle(ToEngineVec2(center), radius);

        uint32_t count = std::min(static_cast<uint32_t>(engineResults.size()), maxResults);
        for (uint32_t i = 0; i < count; ++i) {
            ConvertOverlapResult(engineResults[i], &results[i]);
        }

        *resultCount = count;
        return LC_SUCCESS;

    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "OverlapCircle failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_physics2d_overlap_box(LCVec2 center, LCVec2 size,
                                         LCOverlapResult2D* results, uint32_t maxResults, uint32_t* resultCount) {
    return lc_physics2d_overlap_box_rotated(center, size, 0.0f, results, maxResults, resultCount);
}

LC_API LCResult lc_physics2d_overlap_box_rotated(LCVec2 center, LCVec2 size, float angle,
                                                 LCOverlapResult2D* results, uint32_t maxResults, uint32_t* resultCount) {
    if (!results) {
        SetQueryError(LC_ERROR_NULL_POINTER, "results is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!resultCount) {
        SetQueryError(LC_ERROR_NULL_POINTER, "resultCount is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (size.x < 0.0f || size.y < 0.0f) {
        SetQueryError(LC_ERROR_INVALID_PARAMETER, "size cannot have negative components");
        return LC_ERROR_INVALID_PARAMETER;
    }

    *resultCount = 0;

    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        std::vector<lupine::physics2d::OverlapResult2D> engineResults =
            physicsWorld->OverlapBox(ToEngineVec2(center), ToEngineVec2(size), angle);

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

LC_API LCResult lc_physics2d_circle_cast(LCVec2 start, LCVec2 end, float radius, LCShapeCastHit2D* hit) {
    return lc_physics2d_circle_cast_ignore(start, end, radius, nullptr, hit);
}

LC_API LCResult lc_physics2d_circle_cast_ignore(LCVec2 start, LCVec2 end, float radius,
                                                const uint64_t* ignoreBodyId, LCShapeCastHit2D* hit) {
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

    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        lupine::physics2d::ShapeCastHit2D engineHit;

        if (ignoreBodyId) {
            lupine::core::UUID ignoreUUID = BodyIdToUUID(ignoreBodyId);
            physicsWorld->ShapeCast(ToEngineVec2(start), ToEngineVec2(end), radius, engineHit, &ignoreUUID);
        } else {
            physicsWorld->ShapeCast(ToEngineVec2(start), ToEngineVec2(end), radius, engineHit, nullptr);
        }

        ConvertShapeCastHit(engineHit, hit);
        return LC_SUCCESS;

    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "CircleCast failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

LC_API LCResult lc_physics2d_query_available(bool* available) {
    if (!available) {
        SetQueryError(LC_ERROR_NULL_POINTER, "available is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* physicsWorld = GetPhysics2DWorld();
    *available = (physicsWorld != nullptr);
    return LC_SUCCESS;
}

LC_API LCResult lc_physics2d_get_gravity(LCVec2* gravity) {
    if (!gravity) {
        SetQueryError(LC_ERROR_NULL_POINTER, "gravity is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        *gravity = FromEngineVec2(physicsWorld->GetGravity());
        return LC_SUCCESS;

    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "GetGravity failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_physics2d_set_gravity(LCVec2 gravity) {
    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        physicsWorld->SetGravity(ToEngineVec2(gravity));
        return LC_SUCCESS;

    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "SetGravity failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_physics2d_get_time_step(float* out_time_step) {
    if (!out_time_step) {
        SetQueryError(LC_ERROR_NULL_POINTER, "out_time_step is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
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

LC_API LCResult lc_physics2d_set_time_step(float time_step) {
    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
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

LC_API LCResult lc_physics2d_get_velocity_iterations(int* out_iterations) {
    if (!out_iterations) {
        SetQueryError(LC_ERROR_NULL_POINTER, "out_iterations is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }
    try {
        *out_iterations = physicsWorld->GetVelocityIterations();
        return LC_SUCCESS;
    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "GetVelocityIterations failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_physics2d_set_velocity_iterations(int iterations) {
    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }
    try {
        physicsWorld->SetVelocityIterations(iterations);
        return LC_SUCCESS;
    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "SetVelocityIterations failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_physics2d_get_position_iterations(int* out_iterations) {
    if (!out_iterations) {
        SetQueryError(LC_ERROR_NULL_POINTER, "out_iterations is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }
    try {
        *out_iterations = physicsWorld->GetPositionIterations();
        return LC_SUCCESS;
    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "GetPositionIterations failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_physics2d_set_position_iterations(int iterations) {
    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }
    try {
        physicsWorld->SetPositionIterations(iterations);
        return LC_SUCCESS;
    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "SetPositionIterations failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Body <-> Node Resolution
 * ============================================================================ */

LC_API LCResult lc_physics2d_get_body_node(const uint64_t* bodyId, LCNodeHandle* out_node) {
    if (!bodyId || !out_node) {
        SetQueryError(LC_ERROR_NULL_POINTER, "bodyId or out_node is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    *out_node = nullptr;

    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
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

LC_API LCResult lc_physics2d_find_body_for_node(LCNodeHandle node, uint64_t* out_body_id) {
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
    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
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

/* ============================================================================
 * Layer-Masked Query Variants
 * ============================================================================ */

LC_API LCResult lc_physics2d_raycast_masked(LCVec2 origin, LCVec2 direction, float maxDistance,
                                            const uint64_t* ignoreBodyId, uint64_t collisionMask,
                                            LCRaycastHit2D* hit) {
    if (!hit) {
        SetQueryError(LC_ERROR_NULL_POINTER, "hit is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    hit->hit = false;
    hit->fraction = 1.0f;
    hit->bodyId[0] = 0;
    hit->bodyId[1] = 0;

    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        lupine::physics2d::RaycastHit2D engineHit;
        if (ignoreBodyId) {
            lupine::core::UUID ignoreUUID = BodyIdToUUID(ignoreBodyId);
            physicsWorld->Raycast(ToEngineVec2(origin), ToEngineVec2(direction), maxDistance, engineHit, &ignoreUUID, collisionMask);
        } else {
            physicsWorld->Raycast(ToEngineVec2(origin), ToEngineVec2(direction), maxDistance, engineHit, nullptr, collisionMask);
        }
        ConvertRaycastHit(engineHit, hit);
        return LC_SUCCESS;
    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "Raycast failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_physics2d_raycast_all_masked(LCVec2 origin, LCVec2 direction, float maxDistance,
                                                const uint64_t* ignoreBodyId, uint64_t collisionMask,
                                                LCRaycastHit2D* hits, uint32_t maxHits, uint32_t* hitCount) {
    if (!hits || !hitCount) {
        SetQueryError(LC_ERROR_NULL_POINTER, "hits or hitCount is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    *hitCount = 0;

    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        std::vector<lupine::physics2d::RaycastHit2D> engineHits;
        if (ignoreBodyId) {
            lupine::core::UUID ignoreUUID = BodyIdToUUID(ignoreBodyId);
            engineHits = physicsWorld->RaycastAll(ToEngineVec2(origin), ToEngineVec2(direction), maxDistance, &ignoreUUID, collisionMask);
        } else {
            engineHits = physicsWorld->RaycastAll(ToEngineVec2(origin), ToEngineVec2(direction), maxDistance, nullptr, collisionMask);
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

LC_API LCResult lc_physics2d_overlap_point_masked(LCVec2 point, uint64_t collisionMask,
                                                  LCOverlapResult2D* results, uint32_t maxResults, uint32_t* resultCount) {
    if (!results || !resultCount) {
        SetQueryError(LC_ERROR_NULL_POINTER, "results or resultCount is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    *resultCount = 0;

    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        std::vector<lupine::physics2d::OverlapResult2D> engineResults =
            physicsWorld->OverlapPoint(ToEngineVec2(point), collisionMask);

        uint32_t count = std::min(static_cast<uint32_t>(engineResults.size()), maxResults);
        for (uint32_t i = 0; i < count; ++i) {
            ConvertOverlapResult(engineResults[i], &results[i]);
        }
        *resultCount = count;
        return LC_SUCCESS;
    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "OverlapPoint failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_physics2d_overlap_circle_masked(LCVec2 center, float radius, uint64_t collisionMask,
                                                   LCOverlapResult2D* results, uint32_t maxResults, uint32_t* resultCount) {
    if (!results || !resultCount) {
        SetQueryError(LC_ERROR_NULL_POINTER, "results or resultCount is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (radius < 0.0f) {
        SetQueryError(LC_ERROR_INVALID_PARAMETER, "radius cannot be negative");
        return LC_ERROR_INVALID_PARAMETER;
    }

    *resultCount = 0;

    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        std::vector<lupine::physics2d::OverlapResult2D> engineResults =
            physicsWorld->OverlapCircle(ToEngineVec2(center), radius, collisionMask);

        uint32_t count = std::min(static_cast<uint32_t>(engineResults.size()), maxResults);
        for (uint32_t i = 0; i < count; ++i) {
            ConvertOverlapResult(engineResults[i], &results[i]);
        }
        *resultCount = count;
        return LC_SUCCESS;
    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "OverlapCircle failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_physics2d_overlap_box_masked(LCVec2 center, LCVec2 size, float angle, uint64_t collisionMask,
                                                LCOverlapResult2D* results, uint32_t maxResults, uint32_t* resultCount) {
    if (!results || !resultCount) {
        SetQueryError(LC_ERROR_NULL_POINTER, "results or resultCount is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (size.x < 0.0f || size.y < 0.0f) {
        SetQueryError(LC_ERROR_INVALID_PARAMETER, "size cannot have negative components");
        return LC_ERROR_INVALID_PARAMETER;
    }

    *resultCount = 0;

    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        std::vector<lupine::physics2d::OverlapResult2D> engineResults =
            physicsWorld->OverlapBox(ToEngineVec2(center), ToEngineVec2(size), angle, collisionMask);

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

LC_API LCResult lc_physics2d_circle_cast_masked(LCVec2 start, LCVec2 end, float radius,
                                                const uint64_t* ignoreBodyId, uint64_t collisionMask,
                                                LCShapeCastHit2D* hit) {
    if (!hit) {
        SetQueryError(LC_ERROR_NULL_POINTER, "hit is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (radius < 0.0f) {
        SetQueryError(LC_ERROR_INVALID_PARAMETER, "radius cannot be negative");
        return LC_ERROR_INVALID_PARAMETER;
    }

    hit->hit = false;
    hit->fraction = 1.0f;
    hit->bodyId[0] = 0;
    hit->bodyId[1] = 0;

    auto* physicsWorld = GetPhysics2DWorld();
    if (!physicsWorld) {
        SetQueryError(LC_ERROR_PHYSICS_WORLD_INVALID, "No Physics2D world available");
        return LC_ERROR_PHYSICS_WORLD_INVALID;
    }

    try {
        lupine::physics2d::ShapeCastHit2D engineHit;
        if (ignoreBodyId) {
            lupine::core::UUID ignoreUUID = BodyIdToUUID(ignoreBodyId);
            physicsWorld->ShapeCast(ToEngineVec2(start), ToEngineVec2(end), radius, engineHit, &ignoreUUID, collisionMask);
        } else {
            physicsWorld->ShapeCast(ToEngineVec2(start), ToEngineVec2(end), radius, engineHit, nullptr, collisionMask);
        }
        ConvertShapeCastHit(engineHit, hit);
        return LC_SUCCESS;
    } catch (...) {
        SetQueryError(LC_ERROR_INTERNAL_ERROR, "CircleCast failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
