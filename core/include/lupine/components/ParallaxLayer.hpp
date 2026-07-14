#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"

namespace lupine {
namespace components {

/**
 * ParallaxLayer Component
 *
 * One depth layer of a parallax background. Attach to a Node2D that is a direct
 * child of the node carrying a ParallaxBackground component; the background
 * repositions this layer every frame from the camera scroll so it appears to
 * move at a fraction of the camera's speed.
 *
 * motionScale controls the scroll rate per axis:
 *   - (1, 1)  the layer is fixed in the world and scrolls past at full speed
 *             (same plane as gameplay).
 *   - (0, 0)  the layer is locked to the screen and does not scroll at all
 *             (e.g. a far sky).
 *   - (0.5, 0.5) the layer scrolls at half speed (a mid-distance background).
 * Smaller values read as "further away".
 *
 * motionMirroring enables seamless infinite tiling: when an axis value is > 0
 * the layer's scroll wraps modulo that distance, so a single tiling sprite at
 * least (mirroring + viewport) wide loops forever. Leave at 0 for no tiling.
 *
 * The layer's authored local position is captured as its "home" and all motion
 * is applied relative to it, so designing the scene is unaffected.
 */
class ParallaxLayer : public core::Component {
public:
    ParallaxLayer();
    explicit ParallaxLayer(const std::string& name);
    virtual ~ParallaxLayer() = default;

    // ISerializable interface
    std::string GetTypeName() const override { return "ParallaxLayer"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnReady() override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // Scripting bridge (shared by Lua / MRuby / MicroPython / C-API)
    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // ===== Configuration =====

    math::Vec2 GetMotionScale() const { return m_MotionScale; }
    void SetMotionScale(const math::Vec2& scale);

    math::Vec2 GetMotionOffset() const { return m_MotionOffset; }
    void SetMotionOffset(const math::Vec2& offset);

    math::Vec2 GetMotionMirroring() const { return m_MotionMirroring; }
    void SetMotionMirroring(const math::Vec2& mirroring);

    // ===== Driven by ParallaxBackground =====

    /**
     * Reposition the layer for the given background scroll. Called once per
     * frame by the owning ParallaxBackground; not normally invoked directly.
     */
    void ApplyScroll(const math::Vec2& scroll);

    /** The captured authored local position the motion is applied relative to. */
    math::Vec2 GetHomePosition() const { return m_HomePosition; }

    /** Re-capture the current node local position as the home position. */
    void CaptureHomePosition();

private:
    void SyncFromProperties();

    math::Vec2 m_MotionScale;
    math::Vec2 m_MotionOffset;
    math::Vec2 m_MotionMirroring;

    math::Vec2 m_HomePosition;
    bool m_HomeCaptured;
};

} // namespace components
} // namespace lupine
