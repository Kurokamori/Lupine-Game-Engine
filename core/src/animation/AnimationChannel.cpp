#include "lupine/animation/AnimationChannel.hpp"
#include "lupine/animation/AnimationInterp.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/UUID.hpp"

namespace lupine {
namespace animation {

scripting::NodeRef ResolveTargetNode(core::Node* root, const std::string& path) {
    if (!root) {
        return scripting::NodeRef();
    }
    scripting::NodeRef rootRef = scripting::NodeRef::FromRaw(root, nullptr);
    if (path.empty() || path == ".") {
        return rootRef;
    }
    if (path[0] == '/') {
        core::Scene* scene = root->GetScene();
        std::shared_ptr<core::Node> sceneRoot = scene ? scene->GetRoot() : nullptr;
        if (!sceneRoot) {
            return scripting::NodeRef();
        }
        scripting::NodeRef sceneRef = scripting::NodeRef::FromShared(sceneRoot, nullptr);
        std::string relative = path.substr(1);
        if (relative.empty()) {
            return sceneRef;
        }
        return sceneRef.FindNode(relative);
    }
    // Relative path or "%Unique" (NodeRef::FindNode handles the unique-name form).
    return rootRef.FindNode(path);
}

scripting::NodeRef ResolveTargetNode(core::Node* root, const std::string& path,
                                     const std::string& nodeUuid) {
    if (!root) {
        return scripting::NodeRef();
    }
    if (!nodeUuid.empty()) {
        core::UUID uuid = core::UUID::FromString(nodeUuid);
        if (uuid.IsValid()) {
            core::Scene* scene = root->GetScene();
            if (scene) {
                std::shared_ptr<core::Node> found = scene->FindNodeByUUID(uuid);
                if (found) {
                    return scripting::NodeRef::FromShared(found, nullptr);
                }
            }
        }
    }
    return ResolveTargetNode(root, path);
}

namespace {

bool IsVectorOrColor(TrackValueType type) {
    return type == TrackValueType::Vec2 || type == TrackValueType::Vec3 ||
           type == TrackValueType::Vec4 || type == TrackValueType::Color;
}

// Convert an animation array value ([x,y,...]) into the object representation a
// component property of this type expects ({"x":..} / {"r":..}).
nlohmann::json ArrayToTypedObject(const nlohmann::json& value, TrackValueType type) {
    if (!value.is_array()) return value;
    switch (type) {
        case TrackValueType::Vec2:
            return nlohmann::json{{"x", JsonComponent(value, 0)}, {"y", JsonComponent(value, 1)}};
        case TrackValueType::Vec3:
            return nlohmann::json{{"x", JsonComponent(value, 0)}, {"y", JsonComponent(value, 1)},
                                  {"z", JsonComponent(value, 2)}};
        case TrackValueType::Vec4:
            return nlohmann::json{{"x", JsonComponent(value, 0)}, {"y", JsonComponent(value, 1)},
                                  {"z", JsonComponent(value, 2)}, {"w", JsonComponent(value, 3)}};
        case TrackValueType::Color:
            return nlohmann::json{{"r", JsonComponent(value, 0)}, {"g", JsonComponent(value, 1)},
                                  {"b", JsonComponent(value, 2)}, {"a", JsonComponent(value, 3)}};
        default:
            return value;
    }
}

// Convert a component property object value into the animation array form.
nlohmann::json TypedObjectToArray(const nlohmann::json& value, TrackValueType type) {
    if (!value.is_object()) return value;
    switch (type) {
        case TrackValueType::Vec2:
            return nlohmann::json::array({value.value("x", 0.0f), value.value("y", 0.0f)});
        case TrackValueType::Vec3:
            return nlohmann::json::array({value.value("x", 0.0f), value.value("y", 0.0f),
                                          value.value("z", 0.0f)});
        case TrackValueType::Vec4:
            return nlohmann::json::array({value.value("x", 0.0f), value.value("y", 0.0f),
                                          value.value("z", 0.0f), value.value("w", 0.0f)});
        case TrackValueType::Color:
            return nlohmann::json::array({value.value("r", 0.0f), value.value("g", 0.0f),
                                          value.value("b", 0.0f), value.value("a", 1.0f)});
        default:
            return value;
    }
}

} // namespace

nlohmann::json ReadChannel(const scripting::NodeRef& ref, const std::string& channel,
                           TrackValueType type) {
    if (!ref.IsValid()) {
        return nlohmann::json();
    }

    if (channel == "position_2d") {
        math::Vec2 p = ref.GetPosition2D();
        return nlohmann::json::array({p.x, p.y});
    }
    if (channel == "global_position_2d") {
        math::Vec2 p = ref.GetGlobalPosition2D();
        return nlohmann::json::array({p.x, p.y});
    }
    if (channel == "rotation_2d") {
        return ref.GetRotation2D();
    }
    if (channel == "scale_2d") {
        math::Vec2 s = ref.GetScale2D();
        return nlohmann::json::array({s.x, s.y});
    }
    if (channel == "position_3d") {
        math::Vec3 p = ref.GetPosition3D();
        return nlohmann::json::array({p.x, p.y, p.z});
    }
    if (channel == "rotation_3d") {
        math::Vec3 r = ref.GetRotation3D();
        return nlohmann::json::array({r.x, r.y, r.z});
    }
    if (channel == "scale_3d") {
        math::Vec3 s = ref.GetScale3D();
        return nlohmann::json::array({s.x, s.y, s.z});
    }
    if (channel == "global_position_3d") {
        math::Vec3 p = ref.GetGlobalPosition3D();
        return nlohmann::json::array({p.x, p.y, p.z});
    }

    nlohmann::json value = ref.Get(channel);
    if (IsVectorOrColor(type) && value.is_object()) {
        return TypedObjectToArray(value, type);
    }
    return value;
}

void WriteChannel(scripting::NodeRef& ref, const std::string& channel,
                  const nlohmann::json& value, TrackValueType type) {
    if (!ref.IsValid()) {
        return;
    }

    if (channel == "position_2d") {
        ref.SetPosition2D(JsonComponent(value, 0), JsonComponent(value, 1));
    } else if (channel == "global_position_2d") {
        ref.SetGlobalPosition2D(JsonComponent(value, 0), JsonComponent(value, 1));
    } else if (channel == "rotation_2d") {
        ref.SetRotation2D(value.is_number() ? value.get<float>() : JsonComponent(value, 0));
    } else if (channel == "scale_2d") {
        ref.SetScale2D(JsonComponent(value, 0), JsonComponent(value, 1));
    } else if (channel == "position_3d") {
        ref.SetPosition3D(JsonComponent(value, 0), JsonComponent(value, 1), JsonComponent(value, 2));
    } else if (channel == "rotation_3d") {
        ref.SetRotation3D(JsonComponent(value, 0), JsonComponent(value, 1), JsonComponent(value, 2));
    } else if (channel == "scale_3d") {
        ref.SetScale3D(JsonComponent(value, 0), JsonComponent(value, 1), JsonComponent(value, 2));
    } else if (IsVectorOrColor(type) && value.is_array()) {
        ref.Set(channel, ArrayToTypedObject(value, type));
    } else {
        ref.Set(channel, value);
    }
}

} // namespace animation
} // namespace lupine
