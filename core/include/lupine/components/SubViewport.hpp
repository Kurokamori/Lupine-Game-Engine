#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include <memory>
#include <vector>
#include <string>

namespace lupine {

// Forward declarations (full definitions pulled in by the .cpp)
class RenderWorld;
class RenderCamera;
class IGfxDevice;
enum class CameraType;

namespace core {
    class Node;
    class Scene;
}

namespace components {

/**
 * SubViewport Component
 *
 * Nests an independent view context inside the scene, rendering the node's own
 * subtree into an off-screen render target (render-to-texture), similar to
 * Godot's SubViewport - with its own camera, clear color, and shader/lighting
 * context (each SubViewport gathers its own lights and WorldEnvironment).
 *
 * The subtree below a SubViewport node forms a separate render world: those
 * nodes are rendered only into the SubViewport's target, never directly into
 * the parent view. The resulting color texture is exposed via GetColorTexture()
 * so it can be sampled by materials/sprites, and (optionally) the SubViewport
 * draws that texture itself at the owning node's on-screen rectangle.
 *
 * World type (single coherent pass):
 * - World2D : renders the subtree with a 2D camera (Camera2D node or default).
 * - World3D : renders the subtree with a 3D camera (Camera3D node required for
 *             a meaningful view; a default camera is used otherwise).
 * - ScreenUI: renders the subtree with a screen-space canvas camera.
 * Compose multiple worlds (e.g. a 3D scene with a UI overlay) by nesting
 * SubViewports.
 *
 * Input routing:
 * - None    : the subtree receives no input.
 * - Isolated: the subtree receives input only while the pointer is over the
 *             SubViewport's on-screen rectangle (or it holds click focus), with
 *             pointer coordinates remapped into the SubViewport's own space.
 * - Shared  : the subtree always receives input with remapped coordinates, in
 *             parallel with the rest of the scene.
 */
class SubViewport : public core::Component, public IRenderableComponent {
public:
    enum class WorldType {
        World2D = 0,
        World3D = 1,
        ScreenUI = 2
    };

    enum class DisplayMode {
        Disabled = 0,   // Do not draw the texture; only expose it via GetColorTexture()
        Stretch = 1,    // Draw the texture filling the display rectangle
        KeepAspect = 2  // Draw the texture letterboxed to the render aspect ratio
    };

    enum class InputMode {
        None = 0,
        Isolated = 1,
        Shared = 2
    };

    SubViewport();
    explicit SubViewport(const std::string& name);
    virtual ~SubViewport();

    // ISerializable interface
    std::string GetTypeName() const override { return "SubViewport"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnDestroy() override;

    // ===== IRenderableComponent =====
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;
    void prepareGPUResources(IGfxDevice* device) override;

    // ===== Input routing =====
    bool WantsChildInputControl() const override { return true; }
    void DispatchChildInput(float deltaTime,
                            const std::vector<std::shared_ptr<core::Node>>& children) override;

    // ===== Engine integration =====

    /**
     * Render this SubViewport's subtree into its off-screen target.
     * Called by RenderWorld before a view that contains this SubViewport is
     * gathered. Renders at most once per frame regardless of how many views
     * reference it. Safe to call directly to force a refresh.
     */
    void RenderEmbedded(RenderWorld* world, core::Scene* scene);

    /**
     * The color texture produced by the most recent render.
     * Invalid until the first successful render. Sample this from materials,
     * Sprite components, or scripts to display the SubViewport elsewhere.
     */
    TextureHandle GetColorTexture() const { return m_ColorTexture; }

    /**
     * The depth texture of the off-screen target (if any).
     */
    TextureHandle GetDepthTexture() const { return m_DepthTexture; }

    /**
     * Whether this SubViewport currently holds keyboard/click focus (Isolated mode).
     */
    bool IsFocused() const { return m_Focused; }
    void SetFocused(bool focused) { m_Focused = focused; }

    // ===== Property accessors =====

    int GetRenderWidth() const;
    void SetRenderWidth(int width);

    int GetRenderHeight() const;
    void SetRenderHeight(int height);

    WorldType GetWorldType() const;
    void SetWorldType(WorldType type);

    bool GetTransparentBackground() const;
    void SetTransparentBackground(bool transparent);

    math::Color GetClearColor() const;
    void SetClearColor(const math::Color& color);

    DisplayMode GetDisplayMode() const;
    void SetDisplayMode(DisplayMode mode);

    float GetDisplayWidth() const;
    void SetDisplayWidth(float width);

    float GetDisplayHeight() const;
    void SetDisplayHeight(float height);

    bool GetUseUISpace() const;
    void SetUseUISpace(bool useUISpace);

    math::Color GetModulate() const;
    void SetModulate(const math::Color& color);

    bool GetFlipVertical() const;
    void SetFlipVertical(bool flip);

    InputMode GetInputMode() const;
    void SetInputMode(InputMode mode);

    bool GetFocusOnClick() const;
    void SetFocusOnClick(bool focusOnClick);

private:
    // GPU resource management
    void EnsureRenderTarget(IGfxDevice* device, uint32_t width, uint32_t height);
    void ReleaseRenderTarget();

    // Sub-scene camera construction
    std::unique_ptr<RenderCamera> BuildRenderCamera(RenderWorld* world,
                                                    CameraType cameraType,
                                                    float aspectRatio) const;
    core::Node* FindCameraNode(CameraType cameraType) const;

    // Display geometry (logical canvas units): center + size after aspect handling
    void GetDisplayContentRectLogical(math::Vec2& outCenter, math::Vec2& outSize) const;

    // Off-screen target
    RenderTargetHandle m_RenderTarget;
    TextureHandle m_ColorTexture;
    TextureHandle m_DepthTexture;
    uint32_t m_RTWidth = 0;
    uint32_t m_RTHeight = 0;

    // The device that created the current target (cached for cleanup at OnDestroy).
    IGfxDevice* m_OwningDevice = nullptr;

    // Per-frame render guard (renders at most once per frame).
    uint32_t m_LastRenderedFrame = 0xFFFFFFFFu;

    // Input focus state (Isolated mode click focus).
    bool m_Focused = false;
};

} // namespace components
} // namespace lupine
