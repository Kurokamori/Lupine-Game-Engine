#include "lupine/components/ParallaxBackground.hpp"
#include "lupine/components/ParallaxLayer.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/CameraNodes.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/core/Scene.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

ParallaxBackground::ParallaxBackground()
    : ParallaxBackground("ParallaxBackground") {
}

ParallaxBackground::ParallaxBackground(const std::string& name)
    : Component(name),
      m_ScrollScale(1.0f, 1.0f),
      m_ScrollBaseOffset(0.0f, 0.0f),
      m_IgnoreCameraScroll(false),
      m_ManualScroll(0.0f, 0.0f),
      m_AppliedScroll(0.0f, 0.0f) {
}

void ParallaxBackground::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(scrollScale, Vec2, math::Vec2(1.0f, 1.0f), "Parallax"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(scrollBaseOffset, Vec2, math::Vec2(0.0f, 0.0f), "Parallax"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(ignoreCameraScroll, Bool, false, "Parallax"));
}

void ParallaxBackground::SyncFromProperties() {
    m_ScrollScale = GetPropertyValue<math::Vec2>("scrollScale");
    m_ScrollBaseOffset = GetPropertyValue<math::Vec2>("scrollBaseOffset");
    m_IgnoreCameraScroll = GetPropertyValue<bool>("ignoreCameraScroll");
}

void ParallaxBackground::OnReady() {
    SyncFromProperties();
}

void ParallaxBackground::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    if (propertyName == "scrollScale") {
        m_ScrollScale = math::Vec2(newValue["x"].get<float>(), newValue["y"].get<float>());
    } else if (propertyName == "scrollBaseOffset") {
        m_ScrollBaseOffset = math::Vec2(newValue["x"].get<float>(), newValue["y"].get<float>());
    } else if (propertyName == "ignoreCameraScroll") {
        m_IgnoreCameraScroll = newValue.get<bool>();
    }
}

void ParallaxBackground::SetScrollScale(const math::Vec2& scale) {
    m_ScrollScale = scale;
    SetPropertyValue("scrollScale", scale);
}

void ParallaxBackground::SetScrollBaseOffset(const math::Vec2& offset) {
    m_ScrollBaseOffset = offset;
    SetPropertyValue("scrollBaseOffset", offset);
}

void ParallaxBackground::OnLateUpdate(float /*deltaTime*/) {
    if (!IsEnabled()) return;
    UpdateLayers();
}

core::Camera2D* ParallaxBackground::FindActiveCamera2DRecursive(Node* node) {
    if (!node) return nullptr;
    if (!node->IsActiveInHierarchy() || !node->IsVisibleInHierarchy()) {
        return nullptr;
    }

    auto* cam = dynamic_cast<core::Camera2D*>(node);
    if (cam && cam->IsActive()) {
        return cam;
    }

    for (const auto& child : node->GetChildren()) {
        core::Camera2D* found = FindActiveCamera2DRecursive(child.get());
        if (found) {
            return found;
        }
    }
    return nullptr;
}

core::Camera2D* ParallaxBackground::FindActiveCamera2D() const {
    auto* sceneManager = SceneManager::GetInstance();
    if (!sceneManager) return nullptr;
    auto* scene = sceneManager->GetCurrentScene();
    if (!scene) return nullptr;
    return FindActiveCamera2DRecursive(scene->GetRoot().get());
}

void ParallaxBackground::UpdateLayers() {
    Vec2 scroll;
    if (m_IgnoreCameraScroll) {
        scroll = m_ManualScroll;
    } else {
        core::Camera2D* camera = FindActiveCamera2D();
        Vec2 anchor(0.0f, 0.0f);
        if (auto* node2D = dynamic_cast<Node2D*>(m_Owner)) {
            anchor = node2D->GetGlobalPosition();
        }
        if (camera) {
            scroll = camera->GetEffectivePosition() - anchor;
        } else {
            scroll = m_ManualScroll;
        }
    }

    scroll = Vec2(scroll.x * m_ScrollScale.x, scroll.y * m_ScrollScale.y) + m_ScrollBaseOffset;
    m_AppliedScroll = scroll;

    if (!m_Owner) return;
    for (const auto& child : m_Owner->GetChildren()) {
        if (!child) continue;
        auto layer = child->GetComponent<ParallaxLayer>();
        if (layer) {
            layer->ApplyScroll(scroll);
        }
    }
}

nlohmann::json ParallaxBackground::CallMethod(const std::string& method, const nlohmann::json& args) {
    auto argF = [&](size_t i, float fallback) -> float {
        if (args.is_array() && i < args.size() && args[i].is_number()) {
            return args[i].get<float>();
        }
        return fallback;
    };
    auto argB = [&](size_t i, bool fallback) -> bool {
        if (args.is_array() && i < args.size() && args[i].is_boolean()) {
            return args[i].get<bool>();
        }
        return fallback;
    };
    auto vec2Json = [](const Vec2& v) -> nlohmann::json {
        nlohmann::json o;
        o["x"] = v.x; o["y"] = v.y;
        return o;
    };

    if (method == "set_scroll_offset") {
        SetScrollOffset(Vec2(argF(0, 0.0f), argF(1, 0.0f)));
    } else if (method == "get_scroll_offset") {
        return vec2Json(m_ManualScroll);
    } else if (method == "set_scroll_scale") {
        SetScrollScale(Vec2(argF(0, 1.0f), argF(1, 1.0f)));
    } else if (method == "get_scroll_scale") {
        return vec2Json(m_ScrollScale);
    } else if (method == "set_scroll_base_offset") {
        SetScrollBaseOffset(Vec2(argF(0, 0.0f), argF(1, 0.0f)));
    } else if (method == "get_scroll_base_offset") {
        return vec2Json(m_ScrollBaseOffset);
    } else if (method == "set_ignore_camera_scroll") {
        SetIgnoreCameraScroll(argB(0, false));
    } else if (method == "get_ignore_camera_scroll") {
        return m_IgnoreCameraScroll;
    } else if (method == "get_applied_scroll") {
        return vec2Json(m_AppliedScroll);
    } else if (method == "update_layers") {
        UpdateLayers();
    }
    return nlohmann::json();
}

} // namespace components
} // namespace lupine
