#include "lupine/components/AudioListener.hpp"
#include "lupine/audio/AudioManager.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/math/MathCommon.hpp"

namespace lupine {
namespace components {

AudioListener::AudioListener() {
}

AudioListener::~AudioListener() {

    if (m_Active) {
        audio::AudioManager::GetInstance().SetListenerPosition(math::Vec3(0.0f, 0.0f, 0.0f));
        audio::AudioManager::GetInstance().SetListenerOrientation(
            math::Vec3(0.0f, 0.0f, -1.0f),
            math::Vec3(0.0f, 1.0f, 0.0f)
        );
    }
}

void AudioListener::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(active, Bool, true, "Listener"));
}

void AudioListener::OnAwake() {

    m_Active = GetPropertyValue<bool>("active");

}

void AudioListener::OnReady() {
    if (m_Active) {
        UpdateListenerTransform();
    }
}

void AudioListener::OnDestroy() {

    if (m_Active) {
        audio::AudioManager::GetInstance().SetListenerPosition(math::Vec3(0.0f, 0.0f, 0.0f));
        audio::AudioManager::GetInstance().SetListenerOrientation(
            math::Vec3(0.0f, 0.0f, -1.0f),
            math::Vec3(0.0f, 1.0f, 0.0f)
        );
    }
}

void AudioListener::OnUpdate(float deltaTime) {
    if (!m_Active) {
        return;
    }

    UpdateListenerTransform();

    auto* node = GetOwner();
    if (node && deltaTime > 0.0f) {

        math::Vec3 currentPosition(0.0f, 0.0f, 0.0f);

        if (auto* node3d = dynamic_cast<core::Node3D*>(node)) {
            currentPosition = node3d->GetGlobalPosition();
        }

        else if (auto* node2d = dynamic_cast<core::Node2D*>(node)) {
            math::Vec2 pos2d = node2d->GetGlobalPosition();
            currentPosition = math::Vec3(pos2d.x, pos2d.y, 0.0f);
        }

        math::Vec3 velocity = (currentPosition - m_PreviousPosition) / deltaTime;
        audio::AudioManager::GetInstance().SetListenerVelocity(velocity);
        m_PreviousPosition = currentPosition;
    }
}

void AudioListener::SetActive(bool active) {
    m_Active = active;
    SetPropertyValue("active", active);

    if (active) {
        UpdateListenerTransform();
    }
}

void AudioListener::UpdateListenerTransform() {
    auto* node = GetOwner();
    if (!node) {
        return;
    }

    math::Vec3 position(0.0f, 0.0f, 0.0f);
    math::Vec3 forward(0.0f, 0.0f, -1.0f);
    math::Vec3 up(0.0f, 1.0f, 0.0f);

    if (auto* node3d = dynamic_cast<core::Node3D*>(node)) {
        position = node3d->GetGlobalPosition();

    }

    else if (auto* node2d = dynamic_cast<core::Node2D*>(node)) {
        math::Vec2 pos2d = node2d->GetGlobalPosition();
        position = math::Vec3(pos2d.x, pos2d.y, 0.0f);
    }

    audio::AudioManager& audioMgr = audio::AudioManager::GetInstance();
    audioMgr.SetListenerPosition(position);
    audioMgr.SetListenerOrientation(forward, up);
}

}
}
