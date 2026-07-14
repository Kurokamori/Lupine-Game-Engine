#pragma once

#include "lupine/core/Core.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/NodeInstancing.hpp"
#include "lupine/logger/Logger.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>

namespace lupine {
namespace core {

/**
 * PrefabInstance - places a prefab in a scene by reference instead of by copy.
 *
 * The instance node *becomes* the prefab's root: it adopts the prefab root's components,
 * children and groups rather than parenting them under a wrapper. A prefab instance is
 * therefore indistinguishable at runtime from the same subtree pasted into the scene by hand -
 * which matters, because gameplay code addresses objects by node name and reaches out of them
 * with `%`-scoped lookups. Both keep working.
 *
 * What the scene file stores is only what makes this instance *this* instance:
 *
 *   - prefab_reference   the prefab to instantiate
 *   - name / transform   its identity and placement
 *   - groups             groups this instance adds on top of the prefab's own
 *   - overrides          per-instance property patches (see instancing::PropertyOverride)
 *
 * The adopted subtree is deliberately *not* serialized. Saving a scene from the editor writes
 * the reference back out, never a fresh copy of the prefab's contents, so the prefab stays the
 * single source of truth and scenes cannot silently drift from it.
 *
 * Three variants exist so an instance can carry the transform its content needs:
 * PrefabInstance (no transform), PrefabInstance2D (Node2D), PrefabInstance3D (Node3D).
 */
template<typename BaseNode>
class PrefabInstanceImpl : public BaseNode {
public:
    PrefabInstanceImpl() : BaseNode("PrefabInstance"), m_IsLoaded(false), m_Deserializing(false) {}
    explicit PrefabInstanceImpl(const std::string& name)
        : BaseNode(name), m_IsLoaded(false), m_Deserializing(false) {}

    void RegisterProperties() override {
        BaseNode::RegisterProperties();

        this->template RegisterProperty<std::string>("prefab_reference", core::PropertyType::String,
            [this]() { return m_PrefabFilePath; },
            [this](const std::string& value) {
                if (value == m_PrefabFilePath) {
                    return;
                }
                m_PrefabFilePath = value;

                // While a scene is loading, the overrides have not been read yet - instantiating
                // here would build the subtree once without them and again with them. Deserialize
                // drives the single instantiation instead.
                if (!m_Deserializing) {
                    ReloadPrefab();
                }
            });
    }

    nlohmann::json Serialize() const override {
        nlohmann::json json = BaseNode::Serialize();

        // The adopted subtree belongs to the prefab, not to this scene. Writing it back out is
        // what turns instances into local copies; emptying these keys is what prevents it.
        json["components"] = nlohmann::json::array();
        json["children"] = nlohmann::json::array();

        if (m_AuthoredGroups.empty()) {
            json.erase("groups");
        } else {
            json["groups"] = m_AuthoredGroups;
        }

        if (!m_Overrides.empty()) {
            json["overrides"] = instancing::SerializeOverrides(m_Overrides);
        }

        return json;
    }

    void Deserialize(const nlohmann::json& json) override {
        m_Deserializing = true;

        m_AuthoredProperties = json.contains("properties") && json["properties"].is_object()
            ? json["properties"]
            : nlohmann::json::object();

        m_AuthoredGroups.clear();
        if (json.contains("groups") && json["groups"].is_array()) {
            for (const nlohmann::json& groupJson : json["groups"]) {
                if (groupJson.is_string()) {
                    m_AuthoredGroups.push_back(groupJson.get<std::string>());
                }
            }
        }

        m_Overrides = instancing::ParseOverrides(json);

        BaseNode::Deserialize(json);

        m_Deserializing = false;

        LoadAndInstantiate();
    }

    bool SetPrefabReference(const std::string& filepath) {
        m_PrefabFilePath = filepath;
        return LoadAndInstantiate();
    }

    const std::string& GetPrefabReference() const { return m_PrefabFilePath; }

    const std::vector<instancing::PropertyOverride>& GetOverrides() const { return m_Overrides; }

    void SetOverrides(const std::vector<instancing::PropertyOverride>& overrides) {
        m_Overrides = overrides;
    }

    /** Rebuild the instance from the prefab on disk, picking up any edits to it. */
    bool ReloadPrefab() { return LoadAndInstantiate(); }

    /** Drop the adopted subtree, leaving the instance node itself in place. */
    void ClearInstance() {
        const std::vector<std::shared_ptr<Node>> children = this->GetChildren();
        for (const std::shared_ptr<Node>& child : children) {
            this->RemoveChild(child);
        }

        const std::vector<std::shared_ptr<Component>> components = this->GetComponents();
        for (const std::shared_ptr<Component>& component : components) {
            this->RemoveComponent(component);
        }

        m_IsLoaded = false;
    }

    bool HasValidReference() const { return !m_PrefabFilePath.empty() && m_IsLoaded; }

protected:
    bool LoadAndInstantiate() {
        ClearInstance();

        if (m_PrefabFilePath.empty()) {
            return false;
        }

        std::shared_ptr<Node> self;
        try {
            self = this->shared_from_this();
        } catch (const std::bad_weak_ptr&) {
            LOG_ERROR(LogCategory::Core,
                      "PrefabInstance: '{}' is not owned by a shared_ptr; cannot instantiate '{}'",
                      this->GetName(), m_PrefabFilePath);
            return false;
        }

        nlohmann::json rootJson;
        if (!instancing::LoadRootNodeJson(m_PrefabFilePath, instancing::SourceKind::Prefab, rootJson)) {
            return false;
        }

        instancing::ApplyOverrides(rootJson, m_Overrides, m_PrefabFilePath);
        instancing::AdoptRootNode(self, rootJson, m_AuthoredProperties, m_AuthoredGroups);

        m_IsLoaded = true;
        return true;
    }

    std::string m_PrefabFilePath;
    std::vector<instancing::PropertyOverride> m_Overrides;

    // What this instance declared for itself in the scene file. These win over the prefab's own
    // root values, so the prefab supplies defaults and the instance overrides them.
    nlohmann::json m_AuthoredProperties;
    std::vector<std::string> m_AuthoredGroups;

    bool m_IsLoaded;
    bool m_Deserializing;
};

class PrefabInstance : public PrefabInstanceImpl<Node> {
public:
    PrefabInstance() : PrefabInstanceImpl<Node>("PrefabInstance") {}
    explicit PrefabInstance(const std::string& name) : PrefabInstanceImpl<Node>(name) {}

    std::string GetTypeName() const override { return "PrefabInstance"; }
};

class PrefabInstance2D : public PrefabInstanceImpl<Node2D> {
public:
    PrefabInstance2D() : PrefabInstanceImpl<Node2D>("PrefabInstance2D") {}
    explicit PrefabInstance2D(const std::string& name) : PrefabInstanceImpl<Node2D>(name) {}

    std::string GetTypeName() const override { return "PrefabInstance2D"; }
};

class PrefabInstance3D : public PrefabInstanceImpl<Node3D> {
public:
    PrefabInstance3D() : PrefabInstanceImpl<Node3D>("PrefabInstance3D") {}
    explicit PrefabInstance3D(const std::string& name) : PrefabInstanceImpl<Node3D>(name) {}

    std::string GetTypeName() const override { return "PrefabInstance3D"; }
};

} // namespace core
} // namespace lupine
