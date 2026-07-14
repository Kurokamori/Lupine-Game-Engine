/**
 * @file lc_navigation.cpp
 * @brief Implementation of the 2D navigation C API.
 */

#include "navigation/lc_navigation.h"
#include "../core/lc_internal.h"

#include <lupine/components/NavigationRegion2D.hpp>
#include <lupine/components/NavigationAgent2D.hpp>
#include <lupine/components/NavigationObstacle2D.hpp>
#include <lupine/navigation/NavigationServer2D.hpp>

#include <vector>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

NavigationRegion2D* GetRegion(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<NavigationRegion2D*>(comp.get());
}

NavigationAgent2D* GetAgent(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<NavigationAgent2D*>(comp.get());
}

NavigationObstacle2D* GetObstacle(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<NavigationObstacle2D*>(comp.get());
}

std::vector<math::Vec2> ToVecList(const LCVec2* points, int count) {
    std::vector<math::Vec2> result;
    if (!points || count <= 0) {
        return result;
    }
    result.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        result.push_back(math::Vec2(points[i].x, points[i].y));
    }
    return result;
}

} // namespace

/* ===================== NavigationRegion2D ===================== */

LC_API LCResult lc_navigation_region2d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto comp = name ? std::make_shared<NavigationRegion2D>(name)
                         : std::make_shared<NavigationRegion2D>();
        comp->RegisterProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_navigation_region2d_set_outline(LCComponentHandle component,
                                                   const LCVec2* points, int count) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    region->SetOutline(ToVecList(points, count));
    region->Bake();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region2d_add_hole(LCComponentHandle component,
                                                const LCVec2* points, int count) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    region->AddHole(ToVecList(points, count));
    region->Bake();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region2d_clear_holes(LCComponentHandle component) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    region->ClearHoles();
    region->Bake();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region2d_set_enabled(LCComponentHandle component, bool enabled) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    region->SetEnabledRegion(enabled);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region2d_is_enabled(LCComponentHandle component, bool* out_enabled) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    if (!out_enabled) return LC_ERROR_NULL_POINTER;
    *out_enabled = region->GetEnabledRegion();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region2d_bake(LCComponentHandle component) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    region->Bake();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_region2d_get_polygon_count(LCComponentHandle component, int* out_count) {
    auto region = GetRegion(component);
    if (!region) return LC_ERROR_INVALID_HANDLE;
    if (!out_count) return LC_ERROR_NULL_POINTER;
    *out_count = static_cast<int>(region->GetPolygonCount());
    return LC_SUCCESS;
}

/* ===================== NavigationAgent2D ===================== */

LC_API LCResult lc_navigation_agent2d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto comp = name ? std::make_shared<NavigationAgent2D>(name)
                         : std::make_shared<NavigationAgent2D>();
        comp->RegisterProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_navigation_agent2d_set_target_position(LCComponentHandle component, LCVec2 target) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    agent->SetTargetPosition(math::Vec2(target.x, target.y));
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent2d_get_target_position(LCComponentHandle component, LCVec2* out_target) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_target) return LC_ERROR_NULL_POINTER;
    math::Vec2 t = agent->GetTargetPosition();
    *out_target = LCVec2{t.x, t.y};
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent2d_get_next_path_position(LCComponentHandle component, LCVec2* out_position) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_position) return LC_ERROR_NULL_POINTER;
    math::Vec2 p = agent->GetNextPathPosition();
    *out_position = LCVec2{p.x, p.y};
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent2d_is_navigation_finished(LCComponentHandle component, bool* out_finished) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_finished) return LC_ERROR_NULL_POINTER;
    *out_finished = agent->IsNavigationFinished();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent2d_is_target_reachable(LCComponentHandle component, bool* out_reachable) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_reachable) return LC_ERROR_NULL_POINTER;
    *out_reachable = agent->IsTargetReachable();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent2d_distance_to_target(LCComponentHandle component, float* out_distance) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_distance) return LC_ERROR_NULL_POINTER;
    *out_distance = agent->DistanceToTarget();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent2d_get_path_count(LCComponentHandle component, int* out_count) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_count) return LC_ERROR_NULL_POINTER;
    *out_count = static_cast<int>(agent->GetCurrentPath().size());
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent2d_set_max_speed(LCComponentHandle component, float speed) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    agent->SetMaxSpeed(speed);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent2d_get_max_speed(LCComponentHandle component, float* out_speed) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_speed) return LC_ERROR_NULL_POINTER;
    *out_speed = agent->GetMaxSpeed();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent2d_set_auto_move(LCComponentHandle component, bool auto_move) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    agent->SetAutoMove(auto_move);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent2d_reset(LCComponentHandle component) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    agent->ResetPath();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent2d_set_radius(LCComponentHandle component, float radius) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    agent->SetRadius(radius);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent2d_get_radius(LCComponentHandle component, float* out_radius) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_radius) return LC_ERROR_NULL_POINTER;
    *out_radius = agent->GetRadius();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent2d_set_avoidance_enabled(LCComponentHandle component, bool enabled) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    agent->SetAvoidanceEnabled(enabled);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent2d_is_avoidance_enabled(LCComponentHandle component, bool* out_enabled) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_enabled) return LC_ERROR_NULL_POINTER;
    *out_enabled = agent->GetAvoidanceEnabled();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_agent2d_get_velocity(LCComponentHandle component, LCVec2* out_velocity) {
    auto agent = GetAgent(component);
    if (!agent) return LC_ERROR_INVALID_HANDLE;
    if (!out_velocity) return LC_ERROR_NULL_POINTER;
    math::Vec2 v = agent->GetVelocity();
    *out_velocity = LCVec2{v.x, v.y};
    return LC_SUCCESS;
}

/* ===================== NavigationObstacle2D ===================== */

LC_API LCResult lc_navigation_obstacle2d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto comp = name ? std::make_shared<NavigationObstacle2D>(name)
                         : std::make_shared<NavigationObstacle2D>();
        comp->RegisterProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_navigation_obstacle2d_set_radius(LCComponentHandle component, float radius) {
    auto obstacle = GetObstacle(component);
    if (!obstacle) return LC_ERROR_INVALID_HANDLE;
    obstacle->SetRadius(radius);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_obstacle2d_get_radius(LCComponentHandle component, float* out_radius) {
    auto obstacle = GetObstacle(component);
    if (!obstacle) return LC_ERROR_INVALID_HANDLE;
    if (!out_radius) return LC_ERROR_NULL_POINTER;
    *out_radius = obstacle->GetRadius();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_obstacle2d_set_avoidance_enabled(LCComponentHandle component, bool enabled) {
    auto obstacle = GetObstacle(component);
    if (!obstacle) return LC_ERROR_INVALID_HANDLE;
    obstacle->SetAvoidanceEnabled(enabled);
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_obstacle2d_set_carve(LCComponentHandle component, bool carve) {
    auto obstacle = GetObstacle(component);
    if (!obstacle) return LC_ERROR_INVALID_HANDLE;
    obstacle->SetCarveNavMesh(carve);
    obstacle->Bake();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_obstacle2d_set_vertices(LCComponentHandle component,
                                                      const LCVec2* points, int count) {
    auto obstacle = GetObstacle(component);
    if (!obstacle) return LC_ERROR_INVALID_HANDLE;
    obstacle->SetVertices(ToVecList(points, count));
    obstacle->Bake();
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_obstacle2d_bake(LCComponentHandle component) {
    auto obstacle = GetObstacle(component);
    if (!obstacle) return LC_ERROR_INVALID_HANDLE;
    obstacle->Bake();
    return LC_SUCCESS;
}

/* ===================== NavigationServer2D ===================== */

LC_API LCResult lc_navigation_server2d_query_path(LCVec2 from, LCVec2 to,
                                                  LCVec2* out_points, int max_points,
                                                  int* out_count) {
    if (!out_count) return LC_ERROR_NULL_POINTER;
    *out_count = 0;
    std::vector<math::Vec2> path;
    bool found = navigation::NavigationServer2D::GetInstance().QueryPath(
        math::Vec2(from.x, from.y), math::Vec2(to.x, to.y), path);
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
            out_points[written] = LCVec2{path[written].x, path[written].y};
        }
    }
    *out_count = static_cast<int>(path.size());
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_server2d_get_closest_point(LCVec2 p, LCVec2* out_point) {
    if (!out_point) return LC_ERROR_NULL_POINTER;
    math::Vec2 result = navigation::NavigationServer2D::GetInstance().GetClosestPoint(
        math::Vec2(p.x, p.y));
    *out_point = LCVec2{result.x, result.y};
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_server2d_is_point_navigable(LCVec2 p, bool* out_navigable) {
    if (!out_navigable) return LC_ERROR_NULL_POINTER;
    *out_navigable = navigation::NavigationServer2D::GetInstance().IsPointNavigable(
        math::Vec2(p.x, p.y));
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_server2d_get_region_count(int* out_count) {
    if (!out_count) return LC_ERROR_NULL_POINTER;
    *out_count = static_cast<int>(navigation::NavigationServer2D::GetInstance().GetRegionCount());
    return LC_SUCCESS;
}

LC_API LCResult lc_navigation_server2d_get_obstacle_count(int* out_count) {
    if (!out_count) return LC_ERROR_NULL_POINTER;
    *out_count = static_cast<int>(navigation::NavigationServer2D::GetInstance().GetObstacleCount());
    return LC_SUCCESS;
}
