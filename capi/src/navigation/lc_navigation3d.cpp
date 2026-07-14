/**
 * @file lc_navigation3d.cpp
 * @brief Implementation of the 3D navigation C API.
 */

#include "navigation/lc_navigation3d.h"
#include "../core/lc_internal.h"

#include <lupine/components/NavigationRegion3D.hpp>
#include <lupine/components/NavigationAgent3D.hpp>
#include <lupine/components/NavigationObstacle3D.hpp>
#include <lupine/navigation/NavigationServer3D.hpp>

#include <vector>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

NavigationRegion3D* GetRegion(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<NavigationRegion3D*>(comp.get());
}

NavigationAgent3D* GetAgent(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<NavigationAgent3D*>(comp.get());
}

NavigationObstacle3D* GetObstacle(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<NavigationObstacle3D*>(comp.get());
}

} // namespace

/* ===================== NavigationRegion3D ===================== */

LC_API LCResult lc_navigation_region3d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto comp = name ? std::make_shared<NavigationRegion3D>(name)
                         : std::make_shared<NavigationRegion3D>();
        comp->RegisterProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_navigation_region3d_bake(LCComponentHandle component) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    region->Bake();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region3d_set_enabled(LCComponentHandle component, bool enabled) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    region->SetEnabledRegion(enabled);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region3d_is_enabled(LCComponentHandle component, bool* out_enabled) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    if (!out_enabled) return LC_ERROR_NULL_POINTER;
    *out_enabled = region->GetEnabledRegion();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region3d_get_polygon_count(LCComponentHandle component, int* out_count) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    if (!out_count) return LC_ERROR_NULL_POINTER;
    *out_count = static_cast<int>(region->GetPolygonCount());
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region3d_get_triangle_count(LCComponentHandle component, int* out_count) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    if (!out_count) return LC_ERROR_NULL_POINTER;
    *out_count = static_cast<int>(region->GetBakedTriangles().size());
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region3d_set_cell_size(LCComponentHandle component, float value) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    region->SetCellSize(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region3d_set_cell_height(LCComponentHandle component, float value) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    region->SetCellHeight(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region3d_set_agent_height(LCComponentHandle component, float value) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    region->SetAgentHeight(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region3d_set_agent_radius(LCComponentHandle component, float value) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    region->SetAgentRadius(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region3d_set_agent_max_climb(LCComponentHandle component, float value) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    region->SetAgentMaxClimb(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region3d_set_max_slope_degrees(LCComponentHandle component, float value) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    region->SetMaxSlopeDegrees(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region3d_set_min_region_area(LCComponentHandle component, int value) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    region->SetMinRegionArea(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region3d_set_geometry_source(LCComponentHandle component, int source) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    region->SetGeometrySource(source);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region3d_set_include_collision(LCComponentHandle component, bool include) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    region->SetIncludeCollision(include);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region3d_set_include_meshes(LCComponentHandle component, bool include) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    region->SetIncludeMeshes(include);
    return LC_SUCCESS;
}

/* ===================== NavigationAgent3D ===================== */

LC_API LCResult lc_navigation_agent3d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto comp = name ? std::make_shared<NavigationAgent3D>(name)
                         : std::make_shared<NavigationAgent3D>();
        comp->RegisterProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_navigation_agent3d_set_target_position(LCComponentHandle component, LCVec3 target) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    agent->SetTargetPosition(math::Vec3(target.x, target.y, target.z));
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent3d_get_target_position(LCComponentHandle component, LCVec3* out_target) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_target) return LC_ERROR_NULL_POINTER;
    math::Vec3 t = agent->GetTargetPosition();
    *out_target = LCVec3{t.x, t.y, t.z};
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent3d_get_next_path_position(LCComponentHandle component, LCVec3* out_position) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_position) return LC_ERROR_NULL_POINTER;
    math::Vec3 p = agent->GetNextPathPosition();
    *out_position = LCVec3{p.x, p.y, p.z};
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent3d_is_navigation_finished(LCComponentHandle component, bool* out_finished) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_finished) return LC_ERROR_NULL_POINTER;
    *out_finished = agent->IsNavigationFinished();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent3d_is_target_reachable(LCComponentHandle component, bool* out_reachable) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_reachable) return LC_ERROR_NULL_POINTER;
    *out_reachable = agent->IsTargetReachable();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent3d_distance_to_target(LCComponentHandle component, float* out_distance) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_distance) return LC_ERROR_NULL_POINTER;
    *out_distance = agent->DistanceToTarget();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent3d_get_path_count(LCComponentHandle component, int* out_count) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_count) return LC_ERROR_NULL_POINTER;
    *out_count = static_cast<int>(agent->GetCurrentPath().size());
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent3d_set_max_speed(LCComponentHandle component, float speed) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    agent->SetMaxSpeed(speed);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent3d_get_max_speed(LCComponentHandle component, float* out_speed) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_speed) return LC_ERROR_NULL_POINTER;
    *out_speed = agent->GetMaxSpeed();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent3d_set_auto_move(LCComponentHandle component, bool auto_move) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    agent->SetAutoMove(auto_move);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent3d_reset(LCComponentHandle component) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    agent->ResetPath();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent3d_set_radius(LCComponentHandle component, float radius) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    agent->SetRadius(radius);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent3d_get_radius(LCComponentHandle component, float* out_radius) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_radius) return LC_ERROR_NULL_POINTER;
    *out_radius = agent->GetRadius();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent3d_set_avoidance_enabled(LCComponentHandle component, bool enabled) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    agent->SetAvoidanceEnabled(enabled);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent3d_is_avoidance_enabled(LCComponentHandle component, bool* out_enabled) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_enabled) return LC_ERROR_NULL_POINTER;
    *out_enabled = agent->GetAvoidanceEnabled();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent3d_get_velocity(LCComponentHandle component, LCVec3* out_velocity) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_velocity) return LC_ERROR_NULL_POINTER;
    math::Vec3 v = agent->GetVelocity();
    *out_velocity = LCVec3{v.x, v.y, v.z};
    return LC_SUCCESS;
}

/* ===================== NavigationObstacle3D ===================== */

LC_API LCResult lc_navigation_obstacle3d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto comp = name ? std::make_shared<NavigationObstacle3D>(name)
                         : std::make_shared<NavigationObstacle3D>();
        comp->RegisterProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_navigation_obstacle3d_set_radius(LCComponentHandle component, float radius) {
    auto obstacle = GetObstacle(component);
    if (!obstacle) return LC_ERROR_INVALID_HANDLE;
    obstacle->SetRadius(radius);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_obstacle3d_get_radius(LCComponentHandle component, float* out_radius) {
    auto obstacle = GetObstacle(component);
    if (!obstacle) return LC_ERROR_INVALID_HANDLE;
    if (!out_radius) return LC_ERROR_NULL_POINTER;
    *out_radius = obstacle->GetRadius();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_obstacle3d_set_height(LCComponentHandle component, float height) {
    auto obstacle = GetObstacle(component);
    if (!obstacle) return LC_ERROR_INVALID_HANDLE;
    obstacle->SetHeight(height);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_obstacle3d_set_avoidance_enabled(LCComponentHandle component, bool enabled) {
    auto obstacle = GetObstacle(component);
    if (!obstacle) return LC_ERROR_INVALID_HANDLE;
    obstacle->SetAvoidanceEnabled(enabled);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_obstacle3d_set_carve(LCComponentHandle component, bool carve) {
    auto obstacle = GetObstacle(component);
    if (!obstacle) return LC_ERROR_INVALID_HANDLE;
    obstacle->SetCarveNavMesh(carve);
    obstacle->Bake();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_obstacle3d_bake(LCComponentHandle component) {
    auto obstacle = GetObstacle(component);
    if (!obstacle) return LC_ERROR_INVALID_HANDLE;
    obstacle->Bake();
    return LC_SUCCESS;
}

/* ===================== NavigationServer3D ===================== */

LC_API LCResult lc_navigation_server3d_query_path(LCVec3 from, LCVec3 to,
                                                  LCVec3* out_points, int max_points,
                                                  int* out_count) {
    if (!out_count) return LC_ERROR_NULL_POINTER;
    *out_count = 0;
    std::vector<math::Vec3> path;
    bool found = navigation::NavigationServer3D::GetInstance().QueryPath(
        math::Vec3(from.x, from.y, from.z), math::Vec3(to.x, to.y, to.z), path);
    if (!found) {
        return LC_ERROR_NOT_FOUND;
    }
    int written = 0;
    if (out_points && max_points > 0) {
        int limit = static_cast<int>(path.size());
        if (limit > max_points) {
            limit = max_points;
        }
        for (; written < limit; ++written) {
            out_points[written] = LCVec3{path[written].x, path[written].y, path[written].z};
        }
    }
    *out_count = static_cast<int>(path.size());
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_server3d_get_closest_point(LCVec3 p, LCVec3* out_point) {
    if (!out_point) return LC_ERROR_NULL_POINTER;
    math::Vec3 result = navigation::NavigationServer3D::GetInstance().GetClosestPoint(
        math::Vec3(p.x, p.y, p.z));
    *out_point = LCVec3{result.x, result.y, result.z};
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_server3d_is_point_navigable(LCVec3 p, float vertical_tolerance,
                                                          bool* out_navigable) {
    if (!out_navigable) return LC_ERROR_NULL_POINTER;
    *out_navigable = navigation::NavigationServer3D::GetInstance().IsPointNavigable(
        math::Vec3(p.x, p.y, p.z), vertical_tolerance);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_server3d_sample_height(float x, float z, float* out_y, bool* out_found) {
    float y = 0.0f;
    bool found = navigation::NavigationServer3D::GetInstance().SampleHeight(x, z, y);
    if (out_y) *out_y = y;
    if (out_found) *out_found = found;
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_server3d_get_region_count(int* out_count) {
    if (!out_count) return LC_ERROR_NULL_POINTER;
    *out_count = static_cast<int>(navigation::NavigationServer3D::GetInstance().GetRegionCount());
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_server3d_get_carve_volume_count(int* out_count) {
    if (!out_count) return LC_ERROR_NULL_POINTER;
    *out_count = static_cast<int>(navigation::NavigationServer3D::GetInstance().GetCarveVolumeCount());
    return LC_SUCCESS;
}
