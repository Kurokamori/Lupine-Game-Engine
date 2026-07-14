#pragma once

#include "DrawCommand.hpp"
#include "RenderCamera.hpp"
#include "ResourceHandles.hpp"
#include "PBRMaterial.hpp"
#include "lupine/math/Mat4.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec3.hpp"
#include <vector>
#include <string>
#include <unordered_map>

namespace lupine {

// Import math types into lupine namespace for rendering
using math::Mat4;
using math::Vec2;
using math::Vec3;
using math::Color;
using math::AABB;
using math::Plane;

// Forward declarations
class RenderView;
class IGfxDevice;
class RenderWorld;

/**
 * RenderContext - the API given to nodes/components to issue rendering commands.
 *
 * This is the interface that game code uses to render.
 * During the gather stage, each visible renderable component calls methods on the RenderContext
 * to record what it wants to draw.
 *
 * The RenderContext doesn't execute GPU commands immediately - it just records DrawItems
 * into a list. Later, the RenderWorld batches and sorts these items before actual GPU work.
 */
class RenderContext {
public:
    explicit RenderContext(RenderView* view, IGfxDevice* device);
    ~RenderContext() = default;

    // ===== View Information =====

    RenderView* getView() const { return m_view; }
    RenderCamera* getCamera() const;
    IGfxDevice* getDevice() const { return m_device; }

    const Mat4& getViewMatrix() const { return m_viewMatrix; }
    const Mat4& getProjectionMatrix() const { return m_projectionMatrix; }
    const Mat4& getViewProjectionMatrix() const { return m_viewProjectionMatrix; }

    // ===== Spatial Type Context =====

    /**
     * Set the current spatial type for draw items.
     * Called by RenderWorld before gathering renderables from a component.
     */
    void setSpatialType(SpatialType type) { m_currentSpatialType = type; }

    /**
     * Get the current spatial type.
     */
    SpatialType getSpatialType() const { return m_currentSpatialType; }

    /**
     * Set the current Z-index for 2D/UI draw item sorting.
     * Called by RenderWorld before gathering renderables from a component.
     */
    void setZIndex(int zIndex) { m_currentZIndex = zIndex; }

    /**
     * Get the current Z-index.
     */
    int getZIndex() const { return m_currentZIndex; }

    /**
     * Set the current canvas layer for 2D/UI draw item sorting.
     * Set by RenderWorld while traversing a UILayer node's subtree so that the
     * whole subtree is grouped onto one render layer; restored afterwards.
     */
    void setCanvasLayer(int layer) { m_currentCanvasLayer = layer; }

    /**
     * Get the current canvas layer.
     */
    int getCanvasLayer() const { return m_currentCanvasLayer; }

    // ===== High-Level Draw Commands =====

    /**
     * Draw a mesh with a material.
     * This is the most common rendering method.
     */
    void drawMesh(
        MeshHandle mesh,
        MaterialHandle material,
        const Mat4& transform,
        uint32_t submeshIndex = 0
    );

    /**
     * Draw a mesh with property overrides.
     */
    void drawMesh(
        MeshHandle mesh,
        MaterialHandle material,
        const Mat4& transform,
        const MaterialPropertyBlock& overrides,
        uint32_t submeshIndex = 0,
        bool castShadow = true,
        bool receiveShadow = true
    );

    /**
     * Draw many copies of a mesh in a single GPU-instanced draw call.
     *
     * `instanceBuffer` is a per-instance vertex buffer holding `instanceCount`
     * InstanceVertexData records (model columns + color + custom data). It is
     * bound at binding 1 of the instanced pipeline; each instance supplies its
     * own world transform, so no per-instance push constants are issued.
     *
     * `material` must be an instanced-pipeline material (see
     * getDefaultPBRInstancedMaterial). `worldBounds` is the combined world AABB
     * of all instances, used for frustum culling of the whole batch.
     */
    void drawMeshInstanced(
        MeshHandle mesh,
        MaterialHandle material,
        BufferHandle instanceBuffer,
        uint32_t instanceCount,
        const AABB& worldBounds,
        const MaterialPropertyBlock& overrides,
        uint32_t submeshIndex = 0,
        bool castShadow = true,
        bool receiveShadow = true
    );

    /**
     * Draw a 2D sprite.
     */
    void drawSprite(const SpriteDrawData& sprite);

    /**
     * Draw a primitive shape (for debugging, gizmos, etc.).
     */
    void drawPrimitive(const PrimitiveDrawData& primitive);

    /**
     * Draw text.
     */
    void drawText(const TextDrawData& text);

    /**
     * Draw a quad (common for UI, sprites, etc.).
     * @param blendMode Compositing mode (0=Alpha, 1=Additive).
     */
    void drawQuad(
        const Vec3& position,
        const Vec2& size,
        const Color& color,
        TextureHandle texture = TextureHandle(),
        int blendMode = 0
    );

    /**
     * Draw a rounded rectangle (for UI).
     * @param blendMode Blending mode (0=Alpha, 1=Additive, 2=Multiply, 3=Opaque, 4=Overlay)
     */
    void drawRoundedRect(
        const Vec2& position,
        const Vec2& size,
        float cornerRadius,
        const Color& color,
        int blendMode = 0
    );

    /**
     * Draw a rounded rectangle with per-corner radius (for UI).
     * @param cornerRadius Vec4 containing radius for each corner (x=top-left, y=top-right, z=bottom-right, w=bottom-left)
     * @param blendMode Blending mode (0=Alpha, 1=Additive, 2=Multiply, 3=Opaque, 4=Overlay)
     */
    void drawRoundedRect(
        const Vec2& position,
        const Vec2& size,
        const Vec4& cornerRadius,
        const Color& color,
        int blendMode = 0
    );

    /**
     * Draw a textured rounded rectangle with per-corner radius (for UI).
     * @param cornerRadius Vec4 containing radius for each corner (x=top-left, y=top-right, z=bottom-right, w=bottom-left)
     * @param uvMin Minimum UV coordinates (default 0,0)
     * @param uvMax Maximum UV coordinates (default 1,1)
     * @param blendMode Blending mode (0=Alpha, 1=Additive, 2=Multiply, 3=Opaque, 4=Overlay)
     */
    void drawRoundedRect(
        const Vec2& position,
        const Vec2& size,
        const Vec4& cornerRadius,
        const Color& color,
        TextureHandle texture,
        const Vec2& uvMin = Vec2(0.0f, 0.0f),
        const Vec2& uvMax = Vec2(1.0f, 1.0f),
        int blendMode = 0
    );

    /**
     * Draw a rounded rectangle with rotation (for UI).
     * @param position Position (center when rotated)
     * @param size Size of the rectangle
     * @param cornerRadius Vec4 containing radius for each corner
     * @param color Fill color
     * @param rotation Rotation in radians
     * @param blendMode Blending mode (0=Alpha, 1=Additive, 2=Multiply, 3=Opaque, 4=Overlay)
     */
    void drawRoundedRect(
        const Vec2& position,
        const Vec2& size,
        const Vec4& cornerRadius,
        const Color& color,
        float rotation,
        int blendMode = 0
    );

    /**
     * Draw a textured rounded rectangle with rotation (for UI).
     * @param position Position (center when rotated)
     * @param size Size of the rectangle
     * @param cornerRadius Vec4 containing radius for each corner
     * @param color Tint color
     * @param texture Texture handle
     * @param rotation Rotation in radians
     * @param uvMin Minimum UV coordinates
     * @param uvMax Maximum UV coordinates
     * @param blendMode Blending mode (0=Alpha, 1=Additive, 2=Multiply, 3=Opaque, 4=Overlay)
     */
    void drawRoundedRect(
        const Vec2& position,
        const Vec2& size,
        const Vec4& cornerRadius,
        const Color& color,
        TextureHandle texture,
        float rotation,
        const Vec2& uvMin = Vec2(0.0f, 0.0f),
        const Vec2& uvMax = Vec2(1.0f, 1.0f),
        int blendMode = 0
    );

    /**
     * Draw a rounded rectangle border with inner cutout (single color).
     * @param outerPosition Position of the outer rectangle (top-left corner)
     * @param outerSize Size of the outer rectangle
     * @param outerCornerRadius Outer corner radius (x=top-left, y=top-right, z=bottom-right, w=bottom-left)
     * @param borderWidth Border widths (x=top, y=right, z=bottom, w=left)
     * @param color Border color
     */
    void drawRoundedRectBorder(
        const Vec2& outerPosition,
        const Vec2& outerSize,
        const Vec4& outerCornerRadius,
        const Vec4& borderWidth,
        const Color& color
    );

    /**
     * Draw a rounded rectangle border with rotation (single color).
     * @param outerPosition Position of the outer rectangle (center when rotated)
     * @param outerSize Size of the outer rectangle
     * @param outerCornerRadius Outer corner radius (x=top-left, y=top-right, z=bottom-right, w=bottom-left)
     * @param borderWidth Border widths (x=top, y=right, z=bottom, w=left)
     * @param color Border color
     * @param rotation Rotation in radians
     */
    void drawRoundedRectBorder(
        const Vec2& outerPosition,
        const Vec2& outerSize,
        const Vec4& outerCornerRadius,
        const Vec4& borderWidth,
        const Color& color,
        float rotation
    );

    /**
     * Draw a rounded rectangle border with inner cutout (per-side colors).
     * @param outerPosition Position of the outer rectangle (top-left corner)
     * @param outerSize Size of the outer rectangle
     * @param outerCornerRadius Outer corner radius (x=top-left, y=top-right, z=bottom-right, w=bottom-left)
     * @param borderWidth Border widths (x=top, y=right, z=bottom, w=left)
     * @param colorTop Top border color
     * @param colorRight Right border color
     * @param colorBottom Bottom border color
     * @param colorLeft Left border color
     */
    void drawRoundedRectBorder(
        const Vec2& outerPosition,
        const Vec2& outerSize,
        const Vec4& outerCornerRadius,
        const Vec4& borderWidth,
        const Color& colorTop,
        const Color& colorRight,
        const Color& colorBottom,
        const Color& colorLeft
    );

    /**
     * Draw a line.
     */
    void drawLine(
        const Vec3& start,
        const Vec3& end,
        const Color& color,
        float thickness = 1.0f
    );

    /**
     * Draw a circle.
     */
    void drawCircle(
        const Vec3& center,
        float radius,
        const Color& color,
        bool filled = true
    );

    /**
     * Draw a radial gradient (for 2D lights and effects).
     * @param center Center position in world space
     * @param radius Radius of the gradient
     * @param color Color of the gradient (alpha will be modulated by gradient)
     * @param falloff Falloff exponent (1.0 = linear, 2.0 = quadratic, higher = sharper)
     * @param innerRadius Inner radius as fraction (0.0-1.0), full intensity inside this
     * @param blendMode Blending mode (0=Alpha, 1=Additive, 2=Multiply, 3=Opaque)
     */
    void drawRadialGradient(
        const Vec2& center,
        float radius,
        const Color& color,
        float falloff = 2.0f,
        float innerRadius = 0.0f,
        int blendMode = 0
    );

    /**
     * Draw a radial gradient with rotation (for 2D lights and effects).
     * @param center Center position in world space
     * @param radius Radius of the gradient
     * @param color Color of the gradient
     * @param rotation Rotation in radians
     * @param falloff Falloff exponent
     * @param innerRadius Inner radius as fraction (0.0-1.0)
     * @param blendMode Blending mode (0=Alpha, 1=Additive, 2=Multiply, 3=Opaque)
     */
    void drawRadialGradient(
        const Vec2& center,
        float radius,
        const Color& color,
        float rotation,
        float falloff = 2.0f,
        float innerRadius = 0.0f,
        int blendMode = 0
    );

    /**
     * Draw a regular polygon (triangle, pentagon, hexagon, etc.) with SDF-based antialiased edges.
     * @param center Center position in world space
     * @param radius Radius of the polygon (distance from center to vertex)
     * @param sides Number of sides (3=triangle, 5=pentagon, 6=hexagon, etc.)
     * @param color Fill color
     * @param rotation Rotation in radians
     * @param blendMode Blending mode (0=Alpha, 1=Additive, 2=Multiply, 3=Opaque)
     */
    void drawPolygon(
        const Vec2& center,
        float radius,
        int sides,
        const Color& color,
        float rotation = 0.0f,
        int blendMode = 0
    );

    /**
     * Draw a box (wireframe or filled).
     */
    void drawBox(
        const Vec3& center,
        const Vec3& size,
        const Color& color,
        bool wireframe = false
    );

    // ===== UI Clipping (scissor) =====

    /**
     * Push a clip rectangle, expressed in logical-canvas coordinates (top-left
     * origin, Y-down — the same space UI components use for drawRoundedRect).
     * While the stack is non-empty, every draw item recorded is restricted to the
     * intersection of all pushed rectangles. The rect is converted to framebuffer
     * pixels here (using the view's viewport and canvas size) and intersected with
     * the current top of the stack, so nested containers clip correctly.
     */
    void pushClipRect(const Vec2& topLeft, const Vec2& size);

    /**
     * Pop the most recently pushed clip rectangle.
     */
    void popClipRect();

    /**
     * Remove all clip rectangles (no clipping). Called on clear().
     */
    void clearClipStack();

    /**
     * True if a clip rectangle is currently active.
     */
    bool hasActiveClip() const { return !m_clipStack.empty(); }

    // ===== Utilities =====

    /**
     * Check if a bounding box is visible in the camera frustum.
     */
    bool isVisible(const AABB& bounds) const;

    /**
     * Get the number of draw items recorded so far.
     */
    size_t getDrawItemCount() const { return m_drawItems.size(); }

    /**
     * Clear all recorded draw items.
     */
    void clear();

    // ===== Internal - used by RenderWorld =====

    /**
     * Get all recorded draw items.
     * Called by RenderWorld during batching.
     */
    const std::vector<DrawItem>& getDrawItems() const {
        return m_drawItems;
    }

    /**
     * Update camera matrices.
     * Called before gathering renderables.
     */
    void updateCameraMatrices();

    /**
     * Set default materials for primitive rendering.
     * Called by RenderWorld during initialization.
     */
    void setDefaultMaterials(
        MaterialHandle coloredMaterial,
        MaterialHandle texturedMaterial,
        MaterialHandle wireframeMaterial,
        MaterialHandle lineMaterial,
        MaterialHandle textMaterial
    );

    /**
     * Get default colored material.
     */
    MaterialHandle getDefaultColoredMaterial() const { return m_coloredMaterial; }

    /**
     * Get appropriate colored material based on current spatial type (2D or 3D).
     */
    MaterialHandle getColoredMaterialForSpatialType() const {
        return (m_currentSpatialType == SpatialType::World2D || m_currentSpatialType == SpatialType::Canvas)
               ? m_colored2DMaterial : m_coloredMaterial;
    }

    /**
     * Get default colored double-sided material.
     */
    MaterialHandle getDefaultColoredDoubleSidedMaterial() const { return m_coloredDoubleSidedMaterial; }

    /**
     * Set default colored double-sided material.
     */
    void setDefaultColoredDoubleSidedMaterial(MaterialHandle material) { m_coloredDoubleSidedMaterial = material; }

    /**
     * Set default 2D colored material.
     */
    void setDefaultColored2DMaterial(MaterialHandle material) { m_colored2DMaterial = material; }

    /**
     * Set default 2D line material.
     */
    void setDefaultLine2DMaterial(MaterialHandle material) { m_line2DMaterial = material; }

    /**
     * Get default textured material.
     */
    MaterialHandle getDefaultTexturedMaterial() const { return m_texturedMaterial; }

    /**
     * Get/set the 2D textured material with additive blending (used by
     * drawSprite when SpriteDrawData::blendMode == 1).
     */
    MaterialHandle getTexturedAdditiveMaterial() const { return m_texturedAdditiveMaterial; }
    void setTexturedAdditiveMaterial(MaterialHandle material) { m_texturedAdditiveMaterial = material; }

    /**
     * Get/set the 2D colored material with additive blending (used by drawQuad
     * for textureless additive quads).
     */
    MaterialHandle getColored2DAdditiveMaterial() const { return m_colored2DAdditiveMaterial; }
    void setColored2DAdditiveMaterial(MaterialHandle material) { m_colored2DAdditiveMaterial = material; }

    /**
     * Get the 3D billboard particle materials (depth read-only). Components
     * select alpha or additive per their blend mode.
     */
    MaterialHandle getParticle3DAlphaMaterial() const { return m_particle3DAlphaMaterial; }
    MaterialHandle getParticle3DAdditiveMaterial() const { return m_particle3DAdditiveMaterial; }
    void setParticle3DMaterials(MaterialHandle alpha, MaterialHandle additive) {
        m_particle3DAlphaMaterial = alpha;
        m_particle3DAdditiveMaterial = additive;
    }

    /**
     * Get default textured double-sided material.
     */
    MaterialHandle getDefaultTexturedDoubleSidedMaterial() const { return m_texturedDoubleSidedMaterial; }

    /**
     * Set default textured double-sided material.
     */
    void setDefaultTexturedDoubleSidedMaterial(MaterialHandle material) { m_texturedDoubleSidedMaterial = material; }

    /**
     * Get default text material.
     */
    MaterialHandle getDefaultTextMaterial() const { return m_textMaterial; }

    /**
     * Get default 3D text material.
     */
    MaterialHandle getDefaultText3DMaterial() const { return m_text3DMaterial; }

    /**
     * Set default 3D text material.
     */
    void setDefaultText3DMaterial(MaterialHandle material) { m_text3DMaterial = material; }

    /**
     * Get default rounded rect material (alpha blend).
     */
    MaterialHandle getDefaultRoundedRectMaterial() const { return m_roundedRectMaterial; }

    /**
     * Set default rounded rect material.
     */
    void setDefaultRoundedRectMaterial(MaterialHandle material) { m_roundedRectMaterial = material; }

    /**
     * Get rounded rect material with additive blend.
     */
    MaterialHandle getRoundedRectAdditiveMaterial() const { return m_roundedRectAdditiveMaterial; }

    /**
     * Set rounded rect additive material.
     */
    void setRoundedRectAdditiveMaterial(MaterialHandle material) { m_roundedRectAdditiveMaterial = material; }

    /**
     * Get rounded rect material with multiply blend.
     */
    MaterialHandle getRoundedRectMultiplyMaterial() const { return m_roundedRectMultiplyMaterial; }

    /**
     * Set rounded rect multiply material.
     */
    void setRoundedRectMultiplyMaterial(MaterialHandle material) { m_roundedRectMultiplyMaterial = material; }

    /**
     * Get rounded rect material with opaque (no blend).
     */
    MaterialHandle getRoundedRectOpaqueMaterial() const { return m_roundedRectOpaqueMaterial; }

    /**
     * Set rounded rect opaque material.
     */
    void setRoundedRectOpaqueMaterial(MaterialHandle material) { m_roundedRectOpaqueMaterial = material; }

    /**
     * Get rounded rect material with overlay blend.
     */
    MaterialHandle getRoundedRectOverlayMaterial() const { return m_roundedRectOverlayMaterial; }

    /**
     * Set rounded rect overlay material.
     */
    void setRoundedRectOverlayMaterial(MaterialHandle material) { m_roundedRectOverlayMaterial = material; }

    /**
     * Get rounded rect border material (with inner cutout).
     */
    MaterialHandle getRoundedRectBorderMaterial() const { return m_roundedRectBorderMaterial; }

    /**
     * Set rounded rect border material.
     */
    void setRoundedRectBorderMaterial(MaterialHandle material) { m_roundedRectBorderMaterial = material; }

    /**
     * Get 3D rounded rect material (with depth testing).
     */
    MaterialHandle getRoundedRect3DMaterial() const { return m_roundedRect3DMaterial; }

    /**
     * Set 3D rounded rect material.
     */
    void setRoundedRect3DMaterial(MaterialHandle material) { m_roundedRect3DMaterial = material; }

    /**
     * Get 3D rounded rect border material (with depth testing).
     */
    MaterialHandle getRoundedRect3DBorderMaterial() const { return m_roundedRect3DBorderMaterial; }

    /**
     * Set 3D rounded rect border material.
     */
    void setRoundedRect3DBorderMaterial(MaterialHandle material) { m_roundedRect3DBorderMaterial = material; }

    /**
     * Get radial gradient material (alpha blend).
     */
    MaterialHandle getRadialGradientMaterial() const { return m_radialGradientMaterial; }

    /**
     * Set radial gradient material.
     */
    void setRadialGradientMaterial(MaterialHandle material) { m_radialGradientMaterial = material; }

    /**
     * Get radial gradient additive material.
     */
    MaterialHandle getRadialGradientAdditiveMaterial() const { return m_radialGradientAdditiveMaterial; }

    /**
     * Set radial gradient additive material.
     */
    void setRadialGradientAdditiveMaterial(MaterialHandle material) { m_radialGradientAdditiveMaterial = material; }

    /**
     * Get radial gradient multiply material.
     */
    MaterialHandle getRadialGradientMultiplyMaterial() const { return m_radialGradientMultiplyMaterial; }

    /**
     * Set radial gradient multiply material.
     */
    void setRadialGradientMultiplyMaterial(MaterialHandle material) { m_radialGradientMultiplyMaterial = material; }

    /**
     * Get radial gradient opaque material.
     */
    MaterialHandle getRadialGradientOpaqueMaterial() const { return m_radialGradientOpaqueMaterial; }

    /**
     * Set radial gradient opaque material.
     */
    void setRadialGradientOpaqueMaterial(MaterialHandle material) { m_radialGradientOpaqueMaterial = material; }

    /**
     * Get polygon material (alpha blend).
     */
    MaterialHandle getPolygonMaterial() const { return m_polygonMaterial; }

    /**
     * Set polygon material.
     */
    void setPolygonMaterial(MaterialHandle material) { m_polygonMaterial = material; }

    /**
     * Get polygon additive material.
     */
    MaterialHandle getPolygonAdditiveMaterial() const { return m_polygonAdditiveMaterial; }

    /**
     * Set polygon additive material.
     */
    void setPolygonAdditiveMaterial(MaterialHandle material) { m_polygonAdditiveMaterial = material; }

    /**
     * Get polygon multiply material.
     */
    MaterialHandle getPolygonMultiplyMaterial() const { return m_polygonMultiplyMaterial; }

    /**
     * Set polygon multiply material.
     */
    void setPolygonMultiplyMaterial(MaterialHandle material) { m_polygonMultiplyMaterial = material; }

    /**
     * Get polygon opaque material.
     */
    MaterialHandle getPolygonOpaqueMaterial() const { return m_polygonOpaqueMaterial; }

    /**
     * Set polygon opaque material.
     */
    void setPolygonOpaqueMaterial(MaterialHandle material) { m_polygonOpaqueMaterial = material; }

    /**
     * Get default PBR material.
     */
    MaterialHandle getDefaultPBRMaterial() const { return m_pbrMaterial; }

    /**
     * Set default PBR material.
     */
    void setDefaultPBRMaterial(MaterialHandle material) { m_pbrMaterial = material; }

    /**
     * Get default instanced PBR material (GPU-instanced variant of DefaultPBR,
     * reads per-instance transform/color from binding 1). Used by MultiMesh.
     */
    MaterialHandle getDefaultPBRInstancedMaterial() const { return m_pbrInstancedMaterial; }

    /**
     * Set default instanced PBR material.
     */
    void setDefaultPBRInstancedMaterial(MaterialHandle material) { m_pbrInstancedMaterial = material; }

    /**
     * Get default skeletal material (PBR with skeletal animation support).
     */
    MaterialHandle getDefaultSkeletalMaterial() const { return m_skeletalMaterial; }

    /**
     * Set default skeletal material.
     */
    void setDefaultSkeletalMaterial(MaterialHandle material) { m_skeletalMaterial = material; }

    /**
     * Get default toon material (cel shading).
     */
    MaterialHandle getDefaultToonMaterial() const { return m_toonMaterial; }

    /**
     * Set default toon material.
     */
    void setDefaultToonMaterial(MaterialHandle material) { m_toonMaterial = material; }

    /**
     * Get default skeletal toon material (cel shading with skeletal animation).
     */
    MaterialHandle getDefaultSkeletalToonMaterial() const { return m_skeletalToonMaterial; }

    /**
     * Set default skeletal toon material.
     */
    void setDefaultSkeletalToonMaterial(MaterialHandle material) { m_skeletalToonMaterial = material; }

    /**
     * Get default stylized material (fantasy-style soft shading).
     */
    MaterialHandle getDefaultStylizedMaterial() const { return m_stylizedMaterial; }

    /**
     * Set default stylized material.
     */
    void setDefaultStylizedMaterial(MaterialHandle material) { m_stylizedMaterial = material; }

    /**
     * Get default skeletal stylized material (stylized rendering with skeletal animation).
     */
    MaterialHandle getDefaultSkeletalStylizedMaterial() const { return m_skeletalStylizedMaterial; }

    /**
     * Set default skeletal stylized material.
     */
    void setDefaultSkeletalStylizedMaterial(MaterialHandle material) { m_skeletalStylizedMaterial = material; }

    /**
     * Get default transparent material (glass/refraction effects).
     */
    MaterialHandle getDefaultTransparentMaterial() const { return m_transparentMaterial; }

    /**
     * Set default transparent material.
     */
    void setDefaultTransparentMaterial(MaterialHandle material) { m_transparentMaterial = material; }

    /**
     * Get default glow material (emissive effects for stars, lights).
     */
    MaterialHandle getDefaultGlowMaterial() const { return m_glowMaterial; }

    /**
     * Set default glow material.
     */
    void setDefaultGlowMaterial(MaterialHandle material) { m_glowMaterial = material; }

    // ===== Material Registry (for flexible shader support) =====

    /**
     * Register a material for a shader type.
     * @param type The shader type
     * @param material The material handle
     * @param isSkeletal If true, registers as a skeletal variant
     */
    void registerMaterial(ShaderType type, MaterialHandle material, bool isSkeletal = false) {
        if (isSkeletal) {
            m_skeletalMaterials[type] = material;
        } else {
            m_staticMaterials[type] = material;
        }
    }

    /**
     * Get a material for a shader type.
     * Falls back to PBR if the requested type is not registered.
     * @param type The shader type
     * @param isSkeletal If true, looks up skeletal variant
     * @return The material handle, or PBR fallback if not found
     */
    MaterialHandle getMaterial(ShaderType type, bool isSkeletal = false) const {
        const auto& registry = isSkeletal ? m_skeletalMaterials : m_staticMaterials;
        auto it = registry.find(type);
        if (it != registry.end() && it->second.isValid()) {
            return it->second;
        }
        // Fallback to PBR/Skeletal
        return isSkeletal ? m_skeletalMaterial : m_pbrMaterial;
    }

    /**
     * Check if a material is registered for a shader type.
     */
    bool hasMaterial(ShaderType type, bool isSkeletal = false) const {
        const auto& registry = isSkeletal ? m_skeletalMaterials : m_staticMaterials;
        auto it = registry.find(type);
        return it != registry.end() && it->second.isValid();
    }

    /**
     * Set the RenderWorld reference (called by RenderWorld).
     */
    void setRenderWorld(RenderWorld* world) { m_renderWorld = world; }

    /**
     * Get the RenderWorld.
     */
    RenderWorld* getRenderWorld() const { return m_renderWorld; }

    /**
     * Get or create a custom material from shader file paths.
     * @param vertPath Path to the vertex shader file
     * @param fragPath Path to the fragment shader file
     * @param isSkeletal If true, uses skeletal vertex layout
     * @return Material handle, or invalid if shader compilation fails
     */
    MaterialHandle getOrCreateCustomMaterial(const std::string& vertPath, const std::string& fragPath, bool isSkeletal = false);

    /**
     * Get or create a material compiled from a Lupine Shader (.lsh) file. Used by 2D UI
     * components (e.g. ColorRect) and 3D mesh components to attach a custom shader. The
     * `layout` selects the vertex input layout + default render state for the host; the
     * shader's optional `#render_mode` overrides blend/cull/depth.
     * @param lshPath res:// or physical path to the .lsh file
     * @param blendMode Blending mode (0=Alpha, 1=Additive, 2=Multiply, 3=Opaque, 4=Overlay)
     * @param layout Vertex-layout / host kind (defaults to 2D UI)
     * @return Material handle, or invalid handle if loading/compilation fails
     */
    MaterialHandle getOrCreateLshMaterial(const std::string& lshPath, int blendMode,
                                          LshMaterialLayout layout = LshMaterialLayout::UI2D);

    /**
     * Draw a rounded rectangle quad using a custom (.lsh-derived) material.
     *
     * Builds the standard ColorRect uniform set (u_TintColor, u_CornerRadius, u_Size,
     * u_UseTexture, u_UVRect) and then merges in caller-supplied parameters, so a custom
     * shader receives both the engine-provided values and any exported parameters it declares.
     *
     * @param position Top-left corner when rotation is ~0, otherwise the rotation center
     * @param size Rectangle size
     * @param cornerRadius Per-corner radius (x=top-left, y=top-right, z=bottom-right, w=bottom-left)
     * @param color Fill/tint color (mapped to u_TintColor)
     * @param rotation Rotation in radians (0 = axis-aligned)
     * @param material The custom material handle (from getOrCreateLshMaterial)
     * @param customParams Exported shader parameter values (override/extend the standard set)
     */
    void drawRoundedRectShader(
        const Vec2& position,
        const Vec2& size,
        const Vec4& cornerRadius,
        const Color& color,
        float rotation,
        MaterialHandle material,
        const MaterialPropertyBlock& customParams);

    /**
     * Get or create cached quad mesh.
     */
    MeshHandle getOrCreateQuadMesh();

private:
    RenderView* m_view;
    IGfxDevice* m_device;
    RenderWorld* m_renderWorld = nullptr;

    // Camera matrices (cached)
    Mat4 m_viewMatrix;
    Mat4 m_projectionMatrix;
    Mat4 m_viewProjectionMatrix;

    // Current spatial type context (set by RenderWorld before gathering)
    SpatialType m_currentSpatialType = SpatialType::World3D;

    // Current Z-index for 2D/UI sorting (set by RenderWorld before gathering)
    int m_currentZIndex = 0;

    // Current canvas layer for 2D/UI sorting (set by RenderWorld while inside a
    // UILayer subtree). Stamped onto every recorded draw item.
    int m_currentCanvasLayer = 0;

    // UI clip stack. Each entry is the resolved (already intersected with the
    // entry below it) clip rectangle in framebuffer pixels, top-left origin.
    // The back() entry is the active clip stamped onto recorded draw items.
    struct ClipRectPx {
        int32_t x = 0;
        int32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
    };
    std::vector<ClipRectPx> m_clipStack;

    // Recorded draw items
    std::vector<DrawItem> m_drawItems;

    // Default materials for primitive rendering
    MaterialHandle m_coloredMaterial;
    MaterialHandle m_colored2DMaterial;
    MaterialHandle m_coloredDoubleSidedMaterial;
    MaterialHandle m_texturedMaterial;
    MaterialHandle m_texturedAdditiveMaterial;
    MaterialHandle m_colored2DAdditiveMaterial;
    MaterialHandle m_particle3DAlphaMaterial;
    MaterialHandle m_particle3DAdditiveMaterial;
    MaterialHandle m_texturedDoubleSidedMaterial;
    MaterialHandle m_wireframeMaterial;
    MaterialHandle m_lineMaterial;
    MaterialHandle m_line2DMaterial;
    MaterialHandle m_textMaterial;
    MaterialHandle m_text3DMaterial;               // 3D text (depth-tested)
    MaterialHandle m_roundedRectMaterial;          // Alpha blend (default) - 2D UI
    MaterialHandle m_roundedRectAdditiveMaterial;  // Additive blend - 2D UI
    MaterialHandle m_roundedRectMultiplyMaterial;  // Multiply blend - 2D UI
    MaterialHandle m_roundedRectOpaqueMaterial;    // Opaque (no blend) - 2D UI
    MaterialHandle m_roundedRectOverlayMaterial;   // Overlay blend - 2D UI
    MaterialHandle m_roundedRectBorderMaterial;    // Border with inner cutout - 2D UI
    MaterialHandle m_roundedRect3DMaterial;        // 3D variant with depth testing
    MaterialHandle m_roundedRect3DBorderMaterial;  // 3D border variant with depth testing
    MaterialHandle m_radialGradientMaterial;       // Radial gradient - alpha blend
    MaterialHandle m_radialGradientAdditiveMaterial; // Radial gradient - additive blend
    MaterialHandle m_radialGradientMultiplyMaterial; // Radial gradient - multiply blend
    MaterialHandle m_radialGradientOpaqueMaterial;   // Radial gradient - opaque
    MaterialHandle m_polygonMaterial;                // Polygon - alpha blend
    MaterialHandle m_polygonAdditiveMaterial;        // Polygon - additive blend
    MaterialHandle m_polygonMultiplyMaterial;        // Polygon - multiply blend
    MaterialHandle m_polygonOpaqueMaterial;          // Polygon - opaque
    MaterialHandle m_pbrMaterial;
    MaterialHandle m_pbrInstancedMaterial;         // GPU-instanced PBR (per-instance attributes)
    MaterialHandle m_skeletalMaterial;             // Skeletal animation with PBR lighting
    MaterialHandle m_toonMaterial;                 // Toon/cel shading
    MaterialHandle m_skeletalToonMaterial;         // Skeletal animation with toon shading
    MaterialHandle m_stylizedMaterial;             // Stylized fantasy-style shading
    MaterialHandle m_skeletalStylizedMaterial;     // Skeletal animation with stylized shading
    MaterialHandle m_transparentMaterial;          // Transparent/glass with refraction
    MaterialHandle m_glowMaterial;                 // Glow/emissive effects

    // Material registries for flexible shader support
    std::unordered_map<ShaderType, MaterialHandle> m_staticMaterials;    // Static mesh materials by ShaderType
    std::unordered_map<ShaderType, MaterialHandle> m_skeletalMaterials;  // Skeletal mesh materials by ShaderType

    // Cached primitive meshes (created on first use)
    MeshHandle m_cachedQuadMesh;
    MeshHandle m_cachedCubeMesh;
    MeshHandle m_cachedSphereMesh;
    MeshHandle m_cachedCircleMesh;

    // Helper to add a draw item
    void addDrawItem(const DrawItem& item);

    // Helper to get or create cached meshes
    MeshHandle getOrCreateCubeMesh();
    MeshHandle getOrCreateSphereMesh();
    MeshHandle getOrCreateCircleMesh();
};

} // namespace lupine
