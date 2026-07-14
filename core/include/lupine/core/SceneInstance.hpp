#pragma once

#include "lupine/core/Core.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/NodeInstancing.hpp"
#include "lupine/core/Scene.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>

namespace lupine {
namespace core {

// Import types from core namespace
using core::Node;
using core::Scene;

/**
 * SceneInstance - nests an external scene inside another scene by reference.
 *
 * The referenced scene's root is instantiated as a *child* of this node (child semantics), so
 * the instance acts as a handle to a whole nested tree. Contrast PrefabInstance, which *becomes*
 * the root of what it instantiates.
 *
 * An instanced scene scopes its own unique node names (Godot owner semantics), so a `%Name`
 * lookup inside it does not escape into the host scene.
 *
 * Only the reference, this node's own placement, and any per-instance overrides are serialized -
 * never the instantiated subtree, so a scene saved from the editor keeps pointing at the
 * referenced file instead of accumulating a stale copy of it.
 *
 * Three variants exist so an instance can carry the transform its content needs:
 * SceneInstance (no transform), SceneInstance2D (Node2D), SceneInstance3D (Node3D).
 */
template<typename BaseNode>
class SceneInstanceImpl : public BaseNode {
public:
    SceneInstanceImpl() : BaseNode("SceneInstance"), m_IsLoaded(false), m_AutoReload(false),
                          m_Deserializing(false) {}
    explicit SceneInstanceImpl(const std::string& name)
        : BaseNode(name), m_IsLoaded(false), m_AutoReload(false), m_Deserializing(false) {}

    ~SceneInstanceImpl() override { ClearInstance(); }

    // Instanced scenes scope their own unique node names (Godot owner semantics).
    bool IsSceneInstanceBoundary() const override { return true; }

    void RegisterProperties() override {
        BaseNode::RegisterProperties();

        this->template RegisterProperty<std::string>("scene_reference", core::PropertyType::String,
            [this]() { return m_SceneFilePath; },
            [this](const std::string& value) {
                if (value == m_SceneFilePath) {
                    return;
                }
                m_SceneFilePath = value;

                // Mid-load the overrides have not been read yet; Deserialize drives the single
                // instantiation once everything is in hand.
                if (!m_Deserializing) {
                    LoadAndInstantiate();
                }
            });

        this->template RegisterProperty<bool>("auto_reload", core::PropertyType::Bool,
            [this]() { return m_AutoReload; },
            [this](const bool& value) { m_AutoReload = value; });
    }

    nlohmann::json Serialize() const override {
        nlohmann::json json = BaseNode::Serialize();

        // The instantiated subtree belongs to the referenced scene. Persisting it is what lets a
        // scene file drift into a stale copy of its reference, so it is deliberately dropped.
        json["children"] = nlohmann::json::array();

        if (!m_Overrides.empty()) {
            json["overrides"] = instancing::SerializeOverrides(m_Overrides);
        }

        return json;
    }

    void Deserialize(const nlohmann::json& json) override {
        m_Deserializing = true;

        m_Overrides = instancing::ParseOverrides(json);

        BaseNode::Deserialize(json);

        m_Deserializing = false;

        LoadAndInstantiate();
    }

    /**
     * Point this instance at a scene file and instantiate it.
     * @return true if the scene was loaded successfully
     */
    bool SetSceneReference(const std::string& filepath) {
        m_SceneFilePath = filepath;
        return LoadAndInstantiate();
    }

    const std::string& GetSceneReference() const { return m_SceneFilePath; }

    /** Rebuild from the scene file on disk, picking up any edits to it. */
    bool ReloadScene() { return LoadAndInstantiate(); }

    /** Remove the instantiated nodes, leaving the instance node itself in place. */
    void ClearInstance() {
        const std::vector<std::shared_ptr<Node>> children = this->GetChildren();
        for (const std::shared_ptr<Node>& child : children) {
            this->RemoveChild(child);
        }

        m_InstancedRoot = nullptr;
        m_IsLoaded = false;
    }

    bool HasValidReference() const { return !m_SceneFilePath.empty() && m_IsLoaded; }

    std::shared_ptr<Node> GetInstancedRoot() const { return m_InstancedRoot; }

    const std::vector<instancing::PropertyOverride>& GetOverrides() const { return m_Overrides; }

    void SetOverrides(const std::vector<instancing::PropertyOverride>& overrides) {
        m_Overrides = overrides;
    }

protected:
    bool LoadAndInstantiate() {
        ClearInstance();

        if (m_SceneFilePath.empty()) {
            return false;
        }

        nlohmann::json rootJson;
        if (!instancing::LoadRootNodeJson(m_SceneFilePath, instancing::SourceKind::Scene, rootJson)) {
            return false;
        }

        instancing::ApplyOverrides(rootJson, m_Overrides, m_SceneFilePath);

        m_InstancedRoot = instancing::CloneNodeTree(rootJson);
        if (!m_InstancedRoot) {
            return false;
        }

        this->AddChild(m_InstancedRoot);
        m_IsLoaded = true;

        return true;
    }

    std::string m_SceneFilePath;
    std::shared_ptr<Node> m_InstancedRoot;
    std::vector<instancing::PropertyOverride> m_Overrides;
    bool m_IsLoaded;
    bool m_AutoReload;
    bool m_Deserializing;
};

class SceneInstance : public SceneInstanceImpl<Node> {
public:
    SceneInstance() : SceneInstanceImpl<Node>("SceneInstance") {}
    explicit SceneInstance(const std::string& name) : SceneInstanceImpl<Node>(name) {}

    std::string GetTypeName() const override { return "SceneInstance"; }
};

class SceneInstance2D : public SceneInstanceImpl<Node2D> {
public:
    SceneInstance2D() : SceneInstanceImpl<Node2D>("SceneInstance2D") {}
    explicit SceneInstance2D(const std::string& name) : SceneInstanceImpl<Node2D>(name) {}

    std::string GetTypeName() const override { return "SceneInstance2D"; }
};

class SceneInstance3D : public SceneInstanceImpl<Node3D> {
public:
    SceneInstance3D() : SceneInstanceImpl<Node3D>("SceneInstance3D") {}
    explicit SceneInstance3D(const std::string& name) : SceneInstanceImpl<Node3D>(name) {}

    std::string GetTypeName() const override { return "SceneInstance3D"; }
};

} // namespace core
} // namespace lupine
