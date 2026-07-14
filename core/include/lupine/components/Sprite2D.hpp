#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include <string>
#include <memory>

namespace lupine {
namespace components {

/**
 * Sprite2D Component
 *
 * Renders a 2D sprite with texture support, modulation, sprite sheet slicing,
 * rotation, scale, alpha settings, and alpha cutting.
 *
 * Features:
 * - Texture loading and management
 * - Color modulation/tint
 * - Sprite sheet support with UV rect slicing
 * - Alpha blending and alpha cutoff
 * - Flip horizontal/vertical
 * - Centered or custom pivot
 * - Size control (texture dimensions or custom)
 */
class Sprite2D : public core::Component, public IRenderableComponent {
public:
    Sprite2D();
    explicit Sprite2D(const std::string& name);
    virtual ~Sprite2D();

    // ISerializable interface
    std::string GetTypeName() const override { return "Sprite2D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;

    // Editor gizmo hooks
    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // ===== Texture Management =====
    
    /**
     * Load texture from file path
     */
    bool LoadTexture(const std::string& filepath);
    
    /**
     * Set texture from asset
     */
    void SetTexture(const asset::AssetRef<asset::ImageAsset>& texture);
    
    /**
     * Get current texture asset
     */
    const asset::AssetRef<asset::ImageAsset>& GetTextureAsset() const { return m_TextureAsset; }
    
    /**
     * Get GPU texture handle
     */
    TextureHandle GetTextureHandle() const { return m_TextureHandle; }

    /**
     * Called when an asset file changes on disk.
     * Override to properly invalidate cached texture handles.
     */
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    // ===== Property Accessors =====

    // Texture path
    const std::string& GetTexturePath() const;
    void SetTexturePath(const std::string& path);
    
    // Modulation color (tint)
    math::Color GetModulate() const;
    void SetModulate(const math::Color& color);
    
    // Size (if Vec2(0,0), uses texture dimensions)
    const math::Vec2& GetSize() const;
    void SetSize(const math::Vec2& size);
    
    // UV rectangle for sprite sheet slicing (normalized 0-1)
    const math::Vec4& GetUVRect() const;
    void SetUVRect(const math::Vec4& uvRect);
    
    // Alpha cutoff threshold (0-1)
    float GetAlphaCutoff() const;
    void SetAlphaCutoff(float cutoff);
    
    // Flip flags
    bool GetFlipH() const;
    void SetFlipH(bool flip);
    
    bool GetFlipV() const;
    void SetFlipV(bool flip);
    
    // Centered flag
    bool GetCentered() const;
    void SetCentered(bool centered);
    
    // Offset (when not centered)
    const math::Vec2& GetOffset() const;
    void SetOffset(const math::Vec2& offset);

    // ===== Sprite Sheet Methods =====

    // Sprite sheet enabled flag
    bool GetSpriteSheetEnabled() const;
    void SetSpriteSheetEnabled(bool enabled);

    // Sprite size in pixels (for sprite sheet slicing). Used only when
    // hframes/vframes are 0 (manual slicing); otherwise the cell size is derived
    // from the texture and the frame grid.
    const math::Vec2& GetSpriteSize() const;
    void SetSpriteSize(const math::Vec2& size);

    // Frame-grid slicing. When both are > 0 the
    // cell size is derived automatically from the texture (texture / frames), so
    // no manual spriteSize is needed and each cell renders at its native size.
    int GetHFrames() const;
    void SetHFrames(int columns);
    int GetVFrames() const;
    void SetVFrames(int rows);

    // Current frame index
    int GetCurrentFrame() const;
    void SetCurrentFrame(int frame);

    // Get total number of frames in the sprite sheet
    int GetFrameCount() const;

    // Get frames per row/column
    int GetFramesPerRow() const;
    int GetFramesPerColumn() const;

    // Loaded texture dimensions in pixels (0,0 when no texture is loaded), and
    // the derived per-cell size in sprite-sheet mode (texture when not sheeted).
    math::Vec2 GetTextureSize() const;
    math::Vec2 GetCellSize() const;

    // Script-callable geometry helpers: "get_texture_size", "get_cell_size",
    // "get_frame_count", "get_frames_per_row", "get_frames_per_column".
    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // ===== IRenderableComponent Interface =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return SpatialType::World2D; }

    // ===== Utility Methods =====

    /**
     * Calculate actual render size based on texture and size property
     * Public so editor can use it for hit testing
     */
    math::Vec2 CalculateRenderSize() const;

private:
    // Texture asset and GPU handle
    asset::AssetRef<asset::ImageAsset> m_TextureAsset;
    TextureHandle m_TextureHandle;

    // Flag to track if texture needs to be uploaded to GPU
    bool m_TextureNeedsUpload;

    // Track current texture path to detect changes
    std::string m_CurrentTexturePath;

    /**
     * Upload texture to GPU if needed
     */
    void UploadTextureToGPU();

    /**
     * Calculate UV coordinates based on flip flags and UV rect
     */
    void CalculateUVCoordinates(math::Vec2& uvMin, math::Vec2& uvMax) const;

    /**
     * Calculate UV rect for a specific frame in sprite sheet mode
     */
    math::Vec4 CalculateFrameUVRect(int frameIndex) const;
};

} // namespace components
} // namespace lupine

