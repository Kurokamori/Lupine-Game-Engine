#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/asset/FontAsset.hpp"
#include <string>
#include <memory>

namespace lupine {
namespace components {

/**
 * Label Component
 *
 * Renders text using a font atlas with proper glyph metrics.
 *
 * Features:
 * - Font loading and management
 * - Color tinting
 * - Font size control
 * - Text alignment
 * - Multi-line support
 */
class Label : public core::Component, public IRenderableComponent {
public:
    Label();
    explicit Label(const std::string& name);
    virtual ~Label();

    // ISerializable interface
    std::string GetTypeName() const override { return "Label"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;

    // Editor gizmo hooks
    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // ===== Font Management =====
    
    /**
     * Load font from file path
     */
    bool LoadFont(const std::string& filepath);
    
    /**
     * Set font from asset
     */
    void SetFont(const asset::AssetRef<asset::FontAsset>& font);
    
    /**
     * Get current font asset
     */
    const asset::AssetRef<asset::FontAsset>& GetFontAsset() const { return m_FontAsset; }
    
    /**
     * Get GPU font atlas handle
     */
    FontHandle GetFontHandle() const { return m_FontHandle; }

    // ===== Property Accessors =====
    
    // Text content
    const std::string& GetText() const;
    void SetText(const std::string& text);
    
    // Font path
    const std::string& GetFontPath() const;
    void SetFontPath(const std::string& path);
    
    // Font size
    float GetFontSize() const;
    void SetFontSize(float size);
    
    // Text color
    const math::Color& GetColor() const;
    void SetColor(const math::Color& color);
    
    // Centered flag
    bool GetCentered() const;
    void SetCentered(bool centered);
    
    // Offset (when not centered)
    const math::Vec2& GetOffset() const;
    void SetOffset(const math::Vec2& offset);

    // ===== IRenderableComponent Interface =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return SpatialType::World2D; }

    // Override to provide oriented bounding box that rotates with text
    math::OBB getOrientedBounds() const override;

    // Override for precise ray intersection on rotated text
    bool IntersectRay(const math::Ray& ray, float& outDistance) const override;

private:
    // Font asset and GPU handle
    asset::AssetRef<asset::FontAsset> m_FontAsset;
    FontHandle m_FontHandle;

    // Flag to track if font needs to be uploaded to GPU
    bool m_FontNeedsUpload;

    // Track current font path and size to detect changes
    std::string m_CurrentFontPath;
    float m_CurrentFontSize;

    // Cached text mesh (regenerated when text/font changes)
    MeshHandle m_TextMesh;
    std::string m_CachedText;
    float m_CachedFontSize;
    math::Vec2 m_CachedPosition;  // Track position to regenerate when node moves
    float m_CachedRotation;       // Track rotation to regenerate when node rotates
    bool m_MeshNeedsRegeneration;

    // Cached mesh bounds for accurate getWorldBounds()
    mutable AABB m_CachedMeshBounds;

    // Helper to calculate render size
    math::Vec2 CalculateTextSize() const;

    // Helper to upload font to GPU
    void UploadFontToGPU();

    // Helper to regenerate text mesh
    void RegenerateTextMesh(RenderContext& ctx);
};

} // namespace components
} // namespace lupine

