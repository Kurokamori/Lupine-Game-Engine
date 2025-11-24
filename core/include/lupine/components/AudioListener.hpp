#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/math/Vec3.hpp"

namespace lupine {
namespace components {

/**
 * AudioListener Component
 * Represents the "ears" in a 3D audio scene
 *
 * This component updates the audio system's listener position and orientation
 * based on the node's transform. Typically attached to a camera node.
 *
 * Only one AudioListener should be active in a scene at a time.
 */
class AudioListener : public core::Component {
public:
    AudioListener();
    virtual ~AudioListener();

    // Component interface
    std::string GetTypeName() const override { return "AudioListener"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnDestroy() override;
    void OnUpdate(float deltaTime) override;

    // Active state
    void SetActive(bool active);
    bool IsActive() const { return m_Active; }

private:
    void UpdateListenerTransform();

    bool m_Active{true};
    math::Vec3 m_PreviousPosition{0.0f, 0.0f, 0.0f};
};

} // namespace components
} // namespace lupine
