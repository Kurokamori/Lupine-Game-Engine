#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/components/UIControl.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/TextLayout.hpp"
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
 * - Horizontal/vertical alignment (Left/Center/Right/Fill, Top/Center/Bottom)
 * - Multi-line support with optional word wrap
 * - Outline (width + color)
 * - Adjustable line spacing
 */
class Label : public UIControl, public IRenderableComponent {
public:
    Label();
    explicit Label(const std::string& name);
    virtual ~Label();

    // ISerializable interface
    std::string GetTypeName() const override { return "Label"; }
    void DefineProperties() override;

    // Theme: text colour, outline colour + font size.
    const std::vector<ThemeBinding>& GetThemeBindings() const override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;

    // Mark the text mesh dirty when a text-affecting property changes in the editor.
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // Editor gizmo hooks
    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // Asset hot-reload support
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

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
    math::Color GetColor() const;
    void SetColor(const math::Color& color);
    
    // Centered flag
    bool GetCentered() const;
    void SetCentered(bool centered);

    // Offset (when not centered)
    const math::Vec2& GetOffset() const;
    void SetOffset(const math::Vec2& offset);

    // Horizontal alignment (Left/Center/Right/Fill)
    TextHAlign GetHorizontalAlign() const;
    void SetHorizontalAlign(TextHAlign align);

    // Vertical alignment (Top/Center/Bottom)
    TextVAlign GetVerticalAlign() const;
    void SetVerticalAlign(TextVAlign align);

    // Word wrap (requires a non-zero control width)
    bool GetWordWrap() const;
    void SetWordWrap(bool wrap);

    // Multiline: when false, newlines are collapsed to spaces (single line)
    bool GetMultiline() const;
    void SetMultiline(bool multiline);

    // Line spacing multiplier
    float GetLineSpacing() const;
    void SetLineSpacing(float spacing);

    // Outline width (render pixels; 0 disables the outline)
    float GetOutlineWidth() const;
    void SetOutlineWidth(float width);

    // Outline color
    math::Color GetOutlineColor() const;
    void SetOutlineColor(const math::Color& color);

    // ===== IRenderableComponent Interface =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return GetUISpatialType(); }

    // Content minimum size = measured text size (used by containers/anchors).
    math::Vec2 GetContentMinSize() const override;

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

    // Most recent laid-out text size (width, height), used for bounds/min-size.
    mutable math::Vec2 m_CachedTextSize;

    // Text size last propagated to the parent container, so a size change (e.g. the
    // font finishing loading, which makes GetContentMinSize jump from zero to the real
    // text size) re-triggers the container layout exactly once.
    math::Vec2 m_LastPropagatedTextSize{-1.0f, -1.0f};

    // Text box size at last layout, so alignment/word-wrap re-lay out on resize.
    math::Vec2 m_CachedBoxSize;

    // Helper to calculate render size
    math::Vec2 CalculateTextSize() const;

    /**
     * The world-space box the text is laid out (and aligned) inside.
     *
     * For a Label with a resolved size — an authored width/height, stretched
     * anchors, or a size assigned by a parent container — this is the control's own
     * rect, so the text, the reported bounds and the on-screen rectangle are all the
     * same box. A Label with no resolved size falls back to its natural text extent
     * anchored at the node position (honoring the legacy `centered`/`offset`
     * properties), which is what horizontal/vertical alignment has nothing to align
     * against.
     *
     * As with every math::Rect on the Y-up canvas, `position` is the MIN corner; the
     * layout origin TextLayout expects is `position + Vec2(0, size.y)`.
     */
    math::Rect GetTextBoxRect() const;

    // Build the layout parameters for a given layout box. The box is passed in
    // rather than queried so that the measurement path (which feeds GetBoundsSize
    // through GetContentMinSize) cannot recurse back into the sizing it feeds.
    TextLayoutParams BuildLayoutParams(const math::Vec2& boxSize) const;

    /**
     * Layout params at an EXPLICIT font size, bypassing auto-shrink. The auto-shrink search
     * needs to measure candidate sizes, and it cannot do that through BuildLayoutParams
     * without re-entering itself.
     */
    TextLayoutParams BuildLayoutParamsAt(const math::Vec2& boxSize, float fontSize) const;

    /**
     * The font size actually used: the authored fontSize, stepped down toward minFontSize
     * until the text fits `boxSize` when autoShrink is on.
     */
    float GetEffectiveFontSize(const math::Vec2& boxSize) const;

    // Helper to upload font to GPU
    void UploadFontToGPU();

    // Helper to regenerate text mesh
    void RegenerateTextMesh(RenderContext& ctx);
};

} // namespace components
} // namespace lupine

