#include "lupine/components/ParallaxLayer.hpp"
#include "lupine/core/Node.hpp"
#include <cmath>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

ParallaxLayer::ParallaxLayer()
    : ParallaxLayer("ParallaxLayer") {
}

ParallaxLayer::ParallaxLayer(const std::string& name)
    : Component(name),
      m_MotionScale(0.5f, 0.5f),
      m_MotionOffset(0.0f, 0.0f),
      m_MotionMirroring(0.0f, 0.0f),
      m_HomePosition(0.0f, 0.0f),
      m_HomeCaptured(false) {
}

void ParallaxLayer::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(motionScale, Vec2, math::Vec2(0.5f, 0.5f), "Parallax"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(motionOffset, Vec2, math::Vec2(0.0f, 0.0f), "Parallax"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(motionMirroring, Vec2, math::Vec2(0.0f, 0.0f), "Parallax"));
}

void ParallaxLayer::SyncFromProperties() {
    m_MotionScale = GetPropertyValue<math::Vec2>("motionScale");
    m_MotionOffset = GetPropertyValue<math::Vec2>("motionOffset");
    m_MotionMirroring = GetPropertyValue<math::Vec2>("motionMirroring");
}

void ParallaxLayer::OnReady() {
    SyncFromProperties();
    CaptureHomePosition();
}

void ParallaxLayer::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    if (propertyName == "motionScale") {
        m_MotionScale = math::Vec2(newValue["x"].get<float>(), newValue["y"].get<float>());
    } else if (propertyName == "motionOffset") {
        m_MotionOffset = math::Vec2(newValue["x"].get<float>(), newValue["y"].get<float>());
    } else if (propertyName == "motionMirroring") {
        m_MotionMirroring = math::Vec2(newValue["x"].get<float>(), newValue["y"].get<float>());
    }
}

void ParallaxLayer::CaptureHomePosition() {
    auto* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (node2D) {
        m_HomePosition = node2D->GetPosition();
        m_HomeCaptured = true;
    }
}

void ParallaxLayer::SetMotionScale(const math::Vec2& scale) {
    m_MotionScale = scale;
    SetPropertyValue("motionScale", scale);
}

void ParallaxLayer::SetMotionOffset(const math::Vec2& offset) {
    m_MotionOffset = offset;
    SetPropertyValue("motionOffset", offset);
}

void ParallaxLayer::SetMotionMirroring(const math::Vec2& mirroring) {
    m_MotionMirroring = mirroring;
    SetPropertyValue("motionMirroring", mirroring);
}

void ParallaxLayer::ApplyScroll(const math::Vec2& scroll) {
    auto* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) return;

    if (!m_HomeCaptured) {
        CaptureHomePosition();
    }

    // The layer's displacement from its home position. A motionScale of 1 leaves
    // the layer fixed in the world (full scroll relative to the camera); a value
    // of 0 locks it to the camera (no scroll).
    Vec2 displacement = m_MotionOffset + Vec2(
        scroll.x * (1.0f - m_MotionScale.x),
        scroll.y * (1.0f - m_MotionScale.y));

    // Seamless infinite tiling: wrap each axis into [0, mirroring) so a tiling
    // sprite at least (mirroring + viewport) wide loops without drifting away.
    if (m_MotionMirroring.x > 0.0f) {
        displacement.x -= m_MotionMirroring.x * std::floor(displacement.x / m_MotionMirroring.x);
    }
    if (m_MotionMirroring.y > 0.0f) {
        displacement.y -= m_MotionMirroring.y * std::floor(displacement.y / m_MotionMirroring.y);
    }

    node2D->SetPosition(m_HomePosition + displacement);
}

nlohmann::json ParallaxLayer::CallMethod(const std::string& method, const nlohmann::json& args) {
    auto argF = [&](size_t i, float fallback) -> float {
        if (args.is_array() && i < args.size() && args[i].is_number()) {
            return args[i].get<float>();
        }
        return fallback;
    };
    auto vec2Json = [](const Vec2& v) -> nlohmann::json {
        nlohmann::json o;
        o["x"] = v.x; o["y"] = v.y;
        return o;
    };

    if (method == "set_motion_scale") {
        SetMotionScale(Vec2(argF(0, 0.0f), argF(1, 0.0f)));
    } else if (method == "get_motion_scale") {
        return vec2Json(m_MotionScale);
    } else if (method == "set_motion_offset") {
        SetMotionOffset(Vec2(argF(0, 0.0f), argF(1, 0.0f)));
    } else if (method == "get_motion_offset") {
        return vec2Json(m_MotionOffset);
    } else if (method == "set_motion_mirroring") {
        SetMotionMirroring(Vec2(argF(0, 0.0f), argF(1, 0.0f)));
    } else if (method == "get_motion_mirroring") {
        return vec2Json(m_MotionMirroring);
    } else if (method == "get_home_position") {
        return vec2Json(m_HomePosition);
    } else if (method == "capture_home_position") {
        CaptureHomePosition();
    }
    return nlohmann::json();
}

} // namespace components
} // namespace lupine
