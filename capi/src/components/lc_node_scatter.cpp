/**
 * @file lc_node_scatter.cpp
 * @brief Implementation of NodeScatter C API
 */

#include "components/lc_node_scatter.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/NodeScatter.hpp>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

lupine::math::Color ToEngineColor(LCColor color) {
    return lupine::math::Color(color.r, color.g, color.b, color.a);
}

LCColor FromEngineColor(const lupine::math::Color& color) {
    return LCColor{color.r, color.g, color.b, color.a};
}

lupine::math::Vec2 ToEngineVec2(LCVec2 v) {
    return lupine::math::Vec2(v.x, v.y);
}

LCVec2 FromEngineVec2(const lupine::math::Vec2& v) {
    return LCVec2{v.x, v.y};
}

lupine::math::Vec3 ToEngineVec3(LCVec3 v) {
    return lupine::math::Vec3(v.x, v.y, v.z);
}

LCVec3 FromEngineVec3(const lupine::math::Vec3& v) {
    return LCVec3{v.x, v.y, v.z};
}

NodeScatterMode ToEngineScatterMode(LCNodeScatterMode mode) {
    return static_cast<NodeScatterMode>(mode);
}

LCNodeScatterMode FromEngineScatterMode(NodeScatterMode mode) {
    return static_cast<LCNodeScatterMode>(mode);
}

NodeScatterDistribution ToEngineDistribution(LCNodeScatterDistribution mode) {
    return static_cast<NodeScatterDistribution>(mode);
}

LCNodeScatterDistribution FromEngineDistribution(NodeScatterDistribution mode) {
    return static_cast<LCNodeScatterDistribution>(mode);
}

ScatterCollisionMode ToEngineCollisionMode(LCScatterCollisionMode mode) {
    return static_cast<ScatterCollisionMode>(mode);
}

LCScatterCollisionMode FromEngineCollisionMode(ScatterCollisionMode mode) {
    return static_cast<LCScatterCollisionMode>(mode);
}

NodeScatter* GetNodeScatter(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<NodeScatter*>(comp.get());
}

} // anonymous namespace

/* ============================================================================
 * NodeScatter Creation
 * ============================================================================ */

LC_API LCResult lc_node_scatter_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = name ? std::make_shared<NodeScatter>(name) : std::make_shared<NodeScatter>();
        comp->RegisterProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create NodeScatter component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Scatter Mode
 * ============================================================================ */

LC_API LCResult lc_node_scatter_get_mode(LCComponentHandle component, LCNodeScatterMode* out_mode) {
    if (!out_mode) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_mode = FromEngineScatterMode(scatter->GetScatterMode());
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_mode(LCComponentHandle component, LCNodeScatterMode mode) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetScatterMode(ToEngineScatterMode(mode));
    return LC_SUCCESS;
}

/* ============================================================================
 * Collision Layer Settings
 * ============================================================================ */

LC_API LCResult lc_node_scatter_get_scatter_layers(LCComponentHandle component, uint32_t* out_layers) {
    if (!out_layers) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_layers = scatter->GetScatterLayers();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_scatter_layers(LCComponentHandle component, uint32_t layers) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetScatterLayers(layers);
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_get_cutout_layers(LCComponentHandle component, uint32_t* out_layers) {
    if (!out_layers) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_layers = scatter->GetCutoutLayers();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_cutout_layers(LCComponentHandle component, uint32_t layers) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetCutoutLayers(layers);
    return LC_SUCCESS;
}

/* ============================================================================
 * Distribution Settings
 * ============================================================================ */

LC_API LCResult lc_node_scatter_get_distribution_mode(LCComponentHandle component, LCNodeScatterDistribution* out_mode) {
    if (!out_mode) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_mode = FromEngineDistribution(scatter->GetDistributionMode());
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_distribution_mode(LCComponentHandle component, LCNodeScatterDistribution mode) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetDistributionMode(ToEngineDistribution(mode));
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_get_instance_count(LCComponentHandle component, int* out_count) {
    if (!out_count) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_count = scatter->GetInstanceCount();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_instance_count(LCComponentHandle component, int count) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetInstanceCount(count);
    return LC_SUCCESS;
}

/* ============================================================================
 * Grid Settings
 * ============================================================================ */

LC_API LCResult lc_node_scatter_get_grid_spacing_x(LCComponentHandle component, float* out_spacing) {
    if (!out_spacing) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_spacing = scatter->GetGridSpacingX();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_grid_spacing_x(LCComponentHandle component, float spacing) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetGridSpacingX(spacing);
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_get_grid_spacing_z(LCComponentHandle component, float* out_spacing) {
    if (!out_spacing) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_spacing = scatter->GetGridSpacingZ();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_grid_spacing_z(LCComponentHandle component, float spacing) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetGridSpacingZ(spacing);
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_get_grid_jitter(LCComponentHandle component, float* out_jitter) {
    if (!out_jitter) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_jitter = scatter->GetGridJitter();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_grid_jitter(LCComponentHandle component, float jitter) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetGridJitter(jitter);
    return LC_SUCCESS;
}

/* ============================================================================
 * Area Settings
 * ============================================================================ */

LC_API LCResult lc_node_scatter_get_area_size(LCComponentHandle component, LCVec3* out_size) {
    if (!out_size) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_size = FromEngineVec3(scatter->GetScatterAreaSize());
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_area_size(LCComponentHandle component, LCVec3 size) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetScatterAreaSize(ToEngineVec3(size));
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_get_area_offset(LCComponentHandle component, LCVec3* out_offset) {
    if (!out_offset) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_offset = FromEngineVec3(scatter->GetScatterAreaOffset());
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_area_offset(LCComponentHandle component, LCVec3 offset) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetScatterAreaOffset(ToEngineVec3(offset));
    return LC_SUCCESS;
}

/* ============================================================================
 * Surface Settings
 * ============================================================================ */

LC_API LCResult lc_node_scatter_get_align_to_normal(LCComponentHandle component, bool* out_align) {
    if (!out_align) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_align = scatter->GetAlignToNormal();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_align_to_normal(LCComponentHandle component, bool align) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetAlignToNormal(align);
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_get_normal_alignment_strength(LCComponentHandle component, float* out_strength) {
    if (!out_strength) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_strength = scatter->GetNormalAlignmentStrength();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_normal_alignment_strength(LCComponentHandle component, float strength) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetNormalAlignmentStrength(strength);
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_get_surface_offset(LCComponentHandle component, float* out_offset) {
    if (!out_offset) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_offset = scatter->GetSurfaceOffset();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_surface_offset(LCComponentHandle component, float offset) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetSurfaceOffset(offset);
    return LC_SUCCESS;
}

/* ============================================================================
 * Slope Filtering
 * ============================================================================ */

LC_API LCResult lc_node_scatter_get_min_slope(LCComponentHandle component, float* out_slope) {
    if (!out_slope) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_slope = scatter->GetMinSlope();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_min_slope(LCComponentHandle component, float slope) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetMinSlope(slope);
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_get_max_slope(LCComponentHandle component, float* out_slope) {
    if (!out_slope) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_slope = scatter->GetMaxSlope();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_max_slope(LCComponentHandle component, float slope) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetMaxSlope(slope);
    return LC_SUCCESS;
}

/* ============================================================================
 * Scale Variation
 * ============================================================================ */

LC_API LCResult lc_node_scatter_get_scale_range(LCComponentHandle component, LCVec2* out_range) {
    if (!out_range) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_range = FromEngineVec2(scatter->GetScaleRange());
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_scale_range(LCComponentHandle component, LCVec2 range) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetScaleRange(ToEngineVec2(range));
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_get_uniform_scale(LCComponentHandle component, bool* out_uniform) {
    if (!out_uniform) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_uniform = scatter->GetUniformScale();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_uniform_scale(LCComponentHandle component, bool uniform) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetUniformScale(uniform);
    return LC_SUCCESS;
}

/* ============================================================================
 * Rotation Variation
 * ============================================================================ */

LC_API LCResult lc_node_scatter_get_rotation_variation(LCComponentHandle component, LCVec3* out_variation) {
    if (!out_variation) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_variation = FromEngineVec3(scatter->GetRotationVariation());
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_rotation_variation(LCComponentHandle component, LCVec3 variation) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetRotationVariation(ToEngineVec3(variation));
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_get_random_y_rotation(LCComponentHandle component, bool* out_random) {
    if (!out_random) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_random = scatter->GetRandomYRotation();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_random_y_rotation(LCComponentHandle component, bool random) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetRandomYRotation(random);
    return LC_SUCCESS;
}

/* ============================================================================
 * Randomization
 * ============================================================================ */

LC_API LCResult lc_node_scatter_get_random_seed(LCComponentHandle component, int* out_seed) {
    if (!out_seed) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_seed = scatter->GetRandomSeed();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_random_seed(LCComponentHandle component, int seed) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetRandomSeed(seed);
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_get_use_random_seed(LCComponentHandle component, bool* out_use) {
    if (!out_use) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_use = scatter->GetUseRandomSeed();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_use_random_seed(LCComponentHandle component, bool use) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetUseRandomSeed(use);
    return LC_SUCCESS;
}

/* ============================================================================
 * Debug Visualization
 * ============================================================================ */

LC_API LCResult lc_node_scatter_get_show_debug_shape(LCComponentHandle component, bool* out_show) {
    if (!out_show) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_show = scatter->GetShowDebugShape();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_show_debug_shape(LCComponentHandle component, bool show) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetShowDebugShape(show);
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_get_debug_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_color = FromEngineColor(scatter->GetDebugColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_debug_color(LCComponentHandle component, LCColor color) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetDebugColor(ToEngineColor(color));
    return LC_SUCCESS;
}

/* ============================================================================
 * Performance / Optimization
 * ============================================================================ */

LC_API LCResult lc_node_scatter_get_instances_per_frame(LCComponentHandle component, int* out_count) {
    if (!out_count) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_count = scatter->GetInstancesPerFrame();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_instances_per_frame(LCComponentHandle component, int count) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetInstancesPerFrame(count);
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_get_collision_optimization(LCComponentHandle component, LCScatterCollisionMode* out_mode) {
    if (!out_mode) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_mode = FromEngineCollisionMode(scatter->GetCollisionOptimization());
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_collision_optimization(LCComponentHandle component, LCScatterCollisionMode mode) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetCollisionOptimization(ToEngineCollisionMode(mode));
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_get_collision_distance(LCComponentHandle component, float* out_distance) {
    if (!out_distance) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_distance = scatter->GetCollisionDistance();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_collision_distance(LCComponentHandle component, float distance) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetCollisionDistance(distance);
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_get_progressive_loading(LCComponentHandle component, bool* out_enabled) {
    if (!out_enabled) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_enabled = scatter->GetProgressiveLoading();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_set_progressive_loading(LCComponentHandle component, bool enabled) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->SetProgressiveLoading(enabled);
    return LC_SUCCESS;
}

/* ============================================================================
 * Regeneration
 * ============================================================================ */

LC_API LCResult lc_node_scatter_regenerate(LCComponentHandle component) {
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    scatter->RegenerateScatter();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_needs_regeneration(LCComponentHandle component, bool* out_needs) {
    if (!out_needs) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_needs = scatter->NeedsRegeneration();
    return LC_SUCCESS;
}

LC_API LCResult lc_node_scatter_get_scattered_instance_count(LCComponentHandle component, int* out_count) {
    if (!out_count) return LC_ERROR_NULL_POINTER;
    auto scatter = GetNodeScatter(component);
    if (!scatter) return LC_ERROR_INVALID_HANDLE;
    *out_count = static_cast<int>(scatter->GetScatteredInstances().size());
    return LC_SUCCESS;
}
