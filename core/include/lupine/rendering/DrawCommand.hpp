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
 * Per-instance vertex data for GPU-instanced rendering.
 *
 * One of these is uploaded per instance into a per-instance vertex buffer that
 * is bound at binding 1 of an instanced pipeline. The memory layout here is the
 * authoritative contract shared by the instance-buffer producer
 * (MultiMeshGeneric) and the instanced vertex layout created in RenderWorld:
 *
 *   location 4..7 : model matrix (4 x vec4 columns)
 *   location 8    : instance color (rgba)
 *   location 9    : instance custom data (xyzw)
 *
 * The model matrix is stored column-major as four Vec4 columns so it maps to
 * four consecutive vec4 vertex attributes without transposition on any backend.
 */
struct InstanceVertexData {
    Vec4 modelCol0;
    Vec4 modelCol1;
    Vec4 modelCol2;
    Vec4 modelCol3;
    Color color = Color::White();
    Vec4 customData = Vec4(0.0f, 0.0f, 0.0f, 0.0f);

    void setModel(const Mat4& m) {
        // Mat4 exposes column access via GetColumn(); store columns directly.
        modelCol0 = m.GetColumn(0);
        modelCol1 = m.GetColumn(1);
        modelCol2 = m.GetColumn(2);
        modelCol3 = m.GetColumn(3);
    }
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
    // Canvas/2D layer index (from an enclosing UILayer node). For World2D and
    // Canvas passes this is the outermost sort key: an entire higher layer draws
    // over an entire lower one, before per-item renderLayer / Z-index / order.
    // Content outside any UILayer is layer 0.
    int canvasLayer = 0;
    float sortKey = 0.0f; // For manual sorting
    uint32_t drawOrder = 0; // Original gather order, used as a stable tie-breaker
    float distanceToCamera = 0.0f; // For depth sorting

    // Culling
    AABB worldBounds; // For frustum culling
    bool isVisible = true;

    // Spatial type (determines which render pass this item goes to)
    SpatialType spatialType = SpatialType::World3D;

    // Shadow flags
    bool castShadow = true;
    bool receiveShadow = true;

    // UI clipping (scissor). When clipEnabled, rasterization for this item is
    // restricted to the clip rectangle, expressed in framebuffer pixels with a
    // top-left origin (the IGfxCommandList::setScissor contract). The rect is
    // produced by RenderContext's clip stack and applied by RenderWorld.
    // clipEnabled=false means "no clipping" (full render target).
    bool clipEnabled = false;
    int32_t clipX = 0;
    int32_t clipY = 0;
    uint32_t clipWidth = 0;
    uint32_t clipHeight = 0;

    // Instancing hint
    uint32_t instanceID = 0; // Items with same material/mesh can be instanced

    // ===== GPU instancing =====
    // When isInstanced is true, this draw item represents a single instanced
    // draw of `instanceCount` copies of (mesh, submeshIndex) using the
    // per-instance vertex buffer `instanceBuffer` (bound at binding 1) which
    // holds `instanceCount` InstanceVertexData records. `material` must be an
    // instanced-pipeline material. `worldTransform` is not used to position the
    // geometry in this mode (each instance carries its own transform); it is
    // still set to the owning node transform for sorting/bounds purposes.
    bool isInstanced = false;
    BufferHandle instanceBuffer;
    uint32_t instanceCount = 0;
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

    // ===== 2D geometry batching =====
    // When true, a run of bakeable 2D quad items (sprites / colored quads sharing
    // pipeline + material + texture + clip) was pre-combined on the CPU into the
    // dynamic buffers below: every source quad's four vertices already hold
    // world-space positions, baked vertex color (= per-item tint) and uv-rect-mapped
    // texcoords. The whole batch renders with ONE drawIndexed over geomVertexBuffer /
    // geomIndexBuffer (NOT `mesh`), binding `batchedTexture` once. The executor takes
    // the single-draw geometry path and must NOT iterate `items` as separate draws;
    // `items` is retained only for per-batch scissor (items[0]'s clip), draw-list-cache
    // pointer rebasing, and stats. The buffers are owned by a per-view pool in
    // RenderWorld and stay valid for as long as the view's cached draw list references
    // this batch.
    bool isGeometryBatched = false;
    BufferHandle geomVertexBuffer;
    BufferHandle geomIndexBuffer;
    uint32_t geomIndexCount = 0;
    TextureHandle batchedTexture;       // bound once for the combined draw (may be invalid)
    bool batchedUseTexture = false;     // whether the combined draw samples batchedTexture
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
    // Compositing mode: 0 = Alpha (default), 1 = Additive.
    int blendMode = 0;
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
