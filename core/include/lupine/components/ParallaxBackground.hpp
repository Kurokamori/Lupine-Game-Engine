#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"

namespace lupine {
namespace core { class Node; class Camera2D; }
namespace components {

/**
 * ParallaxBackground Component
 *
 * Drives a set of ParallaxLayer children to create a multi-depth scrolling
 * background. Attach to a Node2D; add child Node2Ds each carrying a
 * ParallaxLayer component. Every frame this component reads the active Camera2D
 * (or a manually supplied scroll) and repositions each direct-child layer
 * according to its motionScale, producing the parallax effect.
 *
 * The scroll fed to the layers is, by default, the active camera's effective
 * position measured relative to this node, then scaled by scrollScale and offset
 * by scrollBaseOffset. Set ignoreCameraScroll to drive the scroll entirely from
 * script via SetScrollOffset instead (e.g. for cutscenes or menus).
 *
 * Only direct children are treated as layers, matching the usual parallax setup.
 */
class ParallaxBackground : public core::Component {
public:
    ParallaxBackground();
    explicit ParallaxBackground(const std::string& name);
    virtual ~ParallaxBackground() = default;

    // ISerializable interface
    std::string GetTypeName() const override { return "ParallaxBackground"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnReady() override;
    void OnLateUpdate(float deltaTime) override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // Scripting bridge (shared by Lua / MRuby / MicroPython / C-API)
    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // ===== Configuration =====

    /** Per-axis multiplier applied to the camera-derived scroll. */
    math::Vec2 GetScrollScale() const { return m_ScrollScale; }
    void SetScrollScale(const math::Vec2& scale);

    /** Constant offset added to the scroll before it reaches the layers. */
    math::Vec2 GetScrollBaseOffset() const { return m_ScrollBaseOffset; }
    void SetScrollBaseOffset(const math::Vec2& offset);

    /** When true, the camera is ignored and the scroll comes only from SetScrollOffset. */
    bool GetIgnoreCameraScroll() const { return m_IgnoreCameraScroll; }
    void SetIgnoreCameraScroll(bool ignore) { m_IgnoreCameraScroll = ignore; }

    /** The manual scroll used when ignoreCameraScroll is set (script-driven). */
    math::Vec2 GetScrollOffset() const { return m_ManualScroll; }
    void SetScrollOffset(const math::Vec2& scroll) { m_ManualScroll = scroll; }

    /** The scroll value computed and applied to the layers on the last update. */
    math::Vec2 GetAppliedScroll() const { return m_AppliedScroll; }

    /** Recompute the scroll and reposition every layer immediately. */
    void UpdateLayers();

private:
    void SyncFromProperties();

    // Find the first active Camera2D in the current scene, or nullptr.
    core::Camera2D* FindActiveCamera2D() const;
    static core::Camera2D* FindActiveCamera2DRecursive(core::Node* node);

    math::Vec2 m_ScrollScale;
    math::Vec2 m_ScrollBaseOffset;
    bool m_IgnoreCameraScroll;
    math::Vec2 m_ManualScroll;
    math::Vec2 m_AppliedScroll;
};

} // namespace components
} // namespace lupine
