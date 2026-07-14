#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/components/UIControl.hpp"
#include "lupine/components/UIImageDraw.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include <string>

namespace lupine {
namespace components {

/**
 * NineSlicePanel Component
 *
 * A UI panel that uses nine-slice (9-patch) scaling for textures.
 * This allows textures to scale without distorting corners and edges.
 *
 * Features:
 * - Nine-slice texture rendering
 * - Configurable border margins (left, right, top, bottom)
 * - Color modulation
 * - Layer ordering
 * - UI space selection
 */
class NineSlicePanel : public UIControl, public IRenderableComponent {
public:
    NineSlicePanel();
    explicit NineSlicePanel(const std::string& name);
    virtual ~NineSlicePanel();

    // ISerializable interface
    std::string GetTypeName() const override { return "NineSlicePanel"; }
    void DefineProperties() override;

    // Theme: texture tint/modulate colour.
    const std::vector<ThemeBinding>& GetThemeBindings() const override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;

    // Editor gizmo hooks
    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // Asset hot-reload support
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    // ===== Size Properties =====
    // Width/height/size are provided by the UIControl base class.

    // The nine-slice border margins form the minimum size (corners must not squish).
    math::Vec2 GetContentMinSize() const override;

    // ===== Layer Properties =====
    
    int GetLayer() const;
    void SetLayer(int layer);

    int GetSortingOrder() const;
    void SetSortingOrder(int order);

    // ===== UI Space =====
    // GetUseUISpace/SetUseUISpace are provided by the UIControl base class.

    // ===== Texture Properties =====
    
    std::string GetTexturePath() const;
    void SetTexturePath(const std::string& path);

    math::Color GetModulate() const;
    void SetModulate(const math::Color& color);

    // ===== Nine-Slice Margins =====
    
    float GetMarginLeft() const;
    void SetMarginLeft(float margin);

    float GetMarginRight() const;
    void SetMarginRight(float margin);

    float GetMarginTop() const;
    void SetMarginTop(float margin);

    float GetMarginBottom() const;
    void SetMarginBottom(float margin);

    // ===== Nine-Slice Options =====
    // How edge/center bands are filled (stretch vs tile) and whether the center patch
    // is drawn (off = frame-only border).

    UINineSliceAxisMode GetNineSliceAxisHorizontal() const;
    void SetNineSliceAxisHorizontal(UINineSliceAxisMode mode);

    UINineSliceAxisMode GetNineSliceAxisVertical() const;
    void SetNineSliceAxisVertical(UINineSliceAxisMode mode);

    bool GetNineSliceDrawCenter() const;
    void SetNineSliceDrawCenter(bool drawCenter);

    // ===== IRenderableComponent Interface =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

    // Override to provide oriented bounding box that rotates with panel
    math::OBB getOrientedBounds() const override;

    // Override for precise ray intersection on rotated panels
    bool IntersectRay(const math::Ray& ray, float& outDistance) const override;

private:
    // Load texture from file
    void LoadTexture(const std::string& path);

    // Upload texture to GPU
    void UploadTextureToGPU(RenderContext& ctx);

    // Render the nine-slice panel
    void RenderNineSlice(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size, float rotation);

    // Texture asset and handle
    asset::AssetRef<asset::ImageAsset> m_TextureAsset;
    TextureHandle m_TextureHandle;
    std::string m_CurrentTexturePath;
};

} // namespace components
} // namespace lupine

