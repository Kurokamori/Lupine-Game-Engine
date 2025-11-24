#pragma once

#include "ResourceHandles.hpp"
#include "Material.hpp"
#include "lupine/math/Mat4.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/AABB.hpp"
#include <vector>

namespace lupine {

// Import math types into lupine namespace for rendering
using math::Mat4;
using math::Vec2;
using math::Vec3;
using math::Color;
using math::AABB;

/**
 * Spatial type for renderable components.
 * Determines which view modes the component should be rendered in.
 */
enum class SpatialType {
    World2D,    // 2D world space (sprites, 2D meshes) - rendered in 2D view
    World3D,    // 3D world space (3D meshes, models) - rendered in 3D view
    Canvas      // UI/Canvas space (UI elements) - rendered in both views
};

/**
 * Per-object rendering parameters.
 * Uploaded as uniform data for each draw call.
 */
struct PerObjectParams {
    Mat4 worldTransform;
    Mat4 normalMatrix; // For lighting calculations
    Color tintColor = Color::White();
    float customFloat0 = 0.0f;
    float customFloat1 = 0.0f;
    float customFloat2 = 0.0f;
    float customFloat3 = 0.0f;
};

/**
 * Draw item - a single draw request from a renderable component.
 * These are collected during the gather stage and then sorted/batched.
 */
struct DrawItem {
    // Geometry
    MeshHandle mesh;
    uint32_t submeshIndex = 0;

    // Material
    MaterialHandle material;
    MaterialPropertyBlock propertyOverrides;

    // Transform
    Mat4 worldTransform;
    PerObjectParams perObject;

    // Sorting
    RenderLayer renderLayer = RenderLayer::Opaque;
    float sortKey = 0.0f; // For manual sorting
    float distanceToCamera = 0.0f; // For depth sorting

    // Culling
    AABB worldBounds; // For frustum culling
    bool isVisible = true;

    // Spatial type (determines which render pass this item goes to)
    SpatialType spatialType = SpatialType::World3D;

    // Shadow flags
    bool castShadow = true;
    bool receiveShadow = true;

    // Instancing hint
    uint32_t instanceID = 0; // Items with same material/mesh can be instanced
};

/**
 * Render batch - a group of DrawItems that can be rendered together.
 * The renderer sorts DrawItems and creates batches to minimize state changes.
 */
struct RenderBatch {
    PipelineHandle pipeline;
    MaterialHandle material;
    MeshHandle mesh;
    uint32_t submeshIndex = 0;

    // Draw items in this batch
    std::vector<const DrawItem*> items;

    // Instancing
    bool isInstanced = false;
    uint32_t instanceCount = 0;
};

/**
 * Render pass type
 */
enum class RenderPassType {
    Opaque3D,      // Opaque 3D geometry
    Transparent3D, // Transparent 3D geometry
    World2D,       // 2D sprites in world space
    Canvas,        // UI/Canvas elements
    Shadow,        // Shadow map generation
    PostProcess    // Post-processing effects
};

/**
 * Render pass - a collection of batches for a specific rendering stage
 */
struct RenderPass {
    RenderPassType type;
    std::vector<RenderBatch> batches;

    void clear() {
        batches.clear();
    }
};

/**
 * Draw command types for high-level API
 */
enum class DrawCommandType {
    Mesh,
    Sprite,
    Primitive,
    Text,
    Custom
};

/**
 * Sprite draw data
 */
struct SpriteDrawData {
    TextureHandle texture;
    Vec2 position;
    Vec2 size;
    Vec2 pivot = Vec2(0.5f, 0.5f);
    float rotation = 0.0f;
    Color tint = Color::White();
    Vec2 uvMin = Vec2(0.0f, 0.0f);
    Vec2 uvMax = Vec2(1.0f, 1.0f);
    int layer = 0;
};

/**
 * Primitive draw data (for debug rendering, etc.)
 */
enum class PrimitiveType {
    Line,
    Triangle,
    Quad,
    Circle,
    Box,
    Sphere
};

struct PrimitiveDrawData {
    PrimitiveType type;
    Vec3 position;
    Vec3 size = Vec3(1.0f, 1.0f, 1.0f);
    Color color = Color::White();
    float thickness = 1.0f;
    bool wireframe = false;
};

/**
 * Text draw data
 */
struct TextDrawData {
    std::string text;
    Vec2 position;
    float fontSize = 16.0f;
    Color color = Color::White();
    FontHandle fontAtlas; // Font atlas handle
    int layer = 0;
};

} // namespace lupine
