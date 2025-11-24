#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/asset/FontAsset.hpp"

namespace lupine {
namespace components {

// Forward declaration - BillboardMode is defined in Sprite3D.hpp
enum class BillboardMode;

/**
 * Label3D Component
 *
 * Renders text in 3D space with font support, text properties,
 * and optional billboarding to face the camera.
 *
 * Features:
 * - Font loading and GPU upload
 * - Text mesh generation in 3D space
 * - Billboard modes (face camera, Y-axis only, disabled)
 * - Text color and styling
 * - Centered text alignment
 * - Position offset
 *
 * Usage:
 *   auto label = node->AddComponent<Label3D>();
 *   label->LoadFont("assets/fonts/default.ttf");
 *   label->SetText("Hello 3D World");
 *   label->SetFontSize(24.0f);
 *   label->SetBillboardMode(BillboardMode::Enabled);
 */
class Label3D : public core::Component, public IRenderableComponent {
public:
    Label3D();
    explicit Label3D(const std::string& name);
    virtual ~Label3D();

    // ISerializable interface
    std::string GetTypeName() const override { return "Label3D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;

    // Gizmo interaction
    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // Font management
    bool LoadFont(const std::string& filepath);
    void SetFont(const asset::AssetRef<asset::FontAsset>& font);
    void UploadFontToGPU();

    // Text measurement
    math::Vec2 CalculateTextSize() const;

    // Property accessors
    const std::string& GetText() const;
    void SetText(const std::string& text);

    const std::string& GetFontPath() const;
    void SetFontPath(const std::string& path);

    float GetFontSize() const;
    void SetFontSize(float size);

    const math::Color& GetColor() const;
    void SetColor(const math::Color& color);

    bool GetCentered() const;
    void SetCentered(bool centered);

    const math::Vec2& GetOffset() const;
    void SetOffset(const math::Vec2& offset);

    BillboardMode GetBillboardMode() const;
    void SetBillboardMode(BillboardMode mode);

    float GetPixelSize() const;
    void SetPixelSize(float size);

    // IRenderableComponent interface
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return SpatialType::World3D; }

    // Ray intersection for editor picking
    bool IntersectRay(const math::Ray& ray, float& outDistance) const override;
    math::OBB getOrientedBounds() const;

private:
    // Font asset and GPU handle
    asset::AssetRef<asset::FontAsset> m_FontAsset;
    FontHandle m_FontHandle;
    bool m_FontNeedsUpload;
    float m_CurrentFontSize;
    std::string m_CurrentFontPath;

    // Text mesh and cached data
    MeshHandle m_TextMesh;
    float m_CachedFontSize;
    std::string m_CachedText;
    AABB m_CachedMeshBounds;
    bool m_MeshNeedsRegeneration;

    // Helper methods
    void RegenerateTextMesh(RenderContext& ctx);
    math::Mat4 CalculateBillboardTransform(const math::Mat4& viewMatrix) const;
    math::Vec2 CalculateRenderSize() const;
};

} // namespace components
} // namespace lupine
