#pragma once

#include "DrawCommand.hpp"
#include "RenderCamera.hpp"
#include "ResourceHandles.hpp"
#include "lupine/math/Mat4.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec3.hpp"
#include <vector>
#include <string>

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
     */
    void drawQuad(
        const Vec3& position,
        const Vec2& size,
        const Color& color,
        TextureHandle texture = TextureHandle()
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
     * Draw a box (wireframe or filled).
     */
    void drawBox(
        const Vec3& center,
        const Vec3& size,
        const Color& color,
        bool wireframe = false
    );

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
     * Get default PBR material.
     */
    MaterialHandle getDefaultPBRMaterial() const { return m_pbrMaterial; }

    /**
     * Set default PBR material.
     */
    void setDefaultPBRMaterial(MaterialHandle material) { m_pbrMaterial = material; }

    /**
     * Get default skeletal material (PBR with skeletal animation support).
     */
    MaterialHandle getDefaultSkeletalMaterial() const { return m_skeletalMaterial; }

    /**
     * Set default skeletal material.
     */
    void setDefaultSkeletalMaterial(MaterialHandle material) { m_skeletalMaterial = material; }

    /**
     * Get or create cached quad mesh.
     */
    MeshHandle getOrCreateQuadMesh();

private:
    RenderView* m_view;
    IGfxDevice* m_device;

    // Camera matrices (cached)
    Mat4 m_viewMatrix;
    Mat4 m_projectionMatrix;
    Mat4 m_viewProjectionMatrix;

    // Current spatial type context (set by RenderWorld before gathering)
    SpatialType m_currentSpatialType = SpatialType::World3D;

    // Current Z-index for 2D/UI sorting (set by RenderWorld before gathering)
    int m_currentZIndex = 0;

    // Recorded draw items
    std::vector<DrawItem> m_drawItems;

    // Default materials for primitive rendering
    MaterialHandle m_coloredMaterial;
    MaterialHandle m_colored2DMaterial;
    MaterialHandle m_coloredDoubleSidedMaterial;
    MaterialHandle m_texturedMaterial;
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
    MaterialHandle m_pbrMaterial;
    MaterialHandle m_skeletalMaterial;             // Skeletal animation with PBR lighting

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
