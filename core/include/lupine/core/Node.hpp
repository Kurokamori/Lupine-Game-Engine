#pragma once

#include "lupine/core/Core.hpp"
#include "lupine/core/SignalObject.hpp"
#include "lupine/math/Math.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

namespace lupine {
namespace core {

// Forward declarations
class Component;
class Scene;

/**
 * Base Node class
 * Nodes form a tree hierarchy and can have components attached
 */
class Node : public SignalObject, public ISerializable, public std::enable_shared_from_this<Node> {
public:
    Node();
    explicit Node(const std::string& name);
    virtual ~Node();

    // SignalObject
    void DefineSignals() override;
    bool InvokeSignalMethod(const std::string& method, const SignalArgs& args) override;

    // A signal the node does not declare itself belongs to the first attached
    // component that does (Button `pressed`, area `body_entered`, a script's
    // @signal), so connecting on the node reaches the component that emits it.
    SignalObject* ResolveSignalOwner(const std::string& signal) override;

    // Resolve declarative connections loaded from a scene file. Walks this node
    // and its components, resolving each pending target (by UUID then path) and
    // establishing the connection. Safe to call once the full tree is built.
    void ResolvePendingConnections();

    // Resolve pending connections for this node AND its entire subtree. The
    // recursive form used when a fully-built subtree is attached to a live scene
    // at runtime (the scene-load path drives this from SceneManager instead).
    void ResolvePendingConnectionsRecursive();

    // ISerializable interface
    std::string GetTypeName() const override { return "Node"; }
    void RegisterProperties() override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;

    // Hierarchy management
    void AddChild(std::shared_ptr<Node> child);
    void InsertChild(std::shared_ptr<Node> child, size_t index);
    void RemoveChild(std::shared_ptr<Node> child);
    void RemoveChild(const std::string& name);
    void ReparentTo(Node* newParent);  // Atomic reparent operation
    std::shared_ptr<Node> GetChild(const std::string& name) const;
    std::shared_ptr<Node> GetChild(size_t index) const;
    size_t GetChildCount() const { return m_Children.size(); }
    const std::vector<std::shared_ptr<Node>>& GetChildren() const { return m_Children; }

    // Reorder an existing direct child to a new position within this node's child
    // list. Pure reorder (the child stays in the tree, no parent/scene change);
    // newIndex is clamped to [0, child count - 1].
    void MoveChild(const std::shared_ptr<Node>& child, size_t newIndex);
    // Index of a direct child within this node's child list, or -1 if not a child.
    int GetChildIndex(const Node* child) const;
    // This node's index within its parent's child list, or -1 if it has no parent.
    int GetIndexInParent() const;
    // Move this node to a new position among its siblings (clamped to range).
    void SetSiblingIndex(size_t index);

    // Process-wide monotonic counter that advances whenever the parent/child
    // topology changes anywhere in the engine (a node is added, inserted,
    // removed, reparented, or detached on destruction). Subsystems that cache a
    // result derived from a node's ancestor chain — e.g. UIControl's effective
    // theme — capture this value when they compute and recompute when it differs,
    // turning "an ancestor changed" into a single integer comparison instead of
    // per-node notification wiring. Pure sibling reorders (MoveChild /
    // SetSiblingIndex) do NOT advance it: the ancestor chain is unchanged, so any
    // ancestry-derived cache stays valid.
    static uint64_t GetTreeStructureVersion();

    // Global "render state" epoch. Advanced whenever something that affects the
    // rendered output of any node changes: a transform (position/rotation/scale),
    // visibility/active state, or a component property (mesh/material/text/etc. via
    // SetPropertyValue). Per-frame dynamic renderables (skeletal poses, particles,
    // animated sprites) also advance it from their update. The renderer captures this
    // value when it gathers draw items / renders shadow maps and skips that work while
    // it is unchanged (combined with GetTreeStructureVersion() for topology changes and
    // the camera state for view changes). Like the tree version this is process-global;
    // a change in one scene may invalidate another's render cache, which only costs a
    // redundant rebuild, never correctness.
    static uint64_t GetRenderEpoch();

    // Advance the render epoch. Call when this node's rendered output changes outside
    // the instrumented transform/visibility/property paths (e.g. a component that
    // regenerates geometry or animation each frame).
    void MarkRenderDirty();

    Node* GetParent() const { return m_Parent; }
    std::shared_ptr<Node> GetParentShared() const;  // Safe version for Python bindings
    void SetParent(Node* parent);
    
    // Find nodes in hierarchy
    std::shared_ptr<Node> FindNode(const std::string& path) const;
    std::string GetPath() const;

    // Unique node names
    // When enabled, this node can be resolved from scripts by its name alone
    // (e.g. "%Health") regardless of its position within its owner scope.
    bool IsUniqueNameInOwner() const { return m_UniqueNameInOwner; }
    void SetUniqueNameInOwner(bool unique) { m_UniqueNameInOwner = unique; }

    // Marks a boundary that scopes unique-name resolution (overridden by SceneInstance).
    // Nodes inside an instanced scene form their own unique-name scope, matching
    // Per-owner semantics. The base Node is never a boundary.
    virtual bool IsSceneInstanceBoundary() const { return false; }

    // Resolve a node within this node's unique-name scope by its name.
    // The scope root is the instanced subtree of the nearest SceneInstance
    // boundary ancestor, or the top-most ancestor (scene root) otherwise.
    // Returns the first node within scope whose unique flag is set and whose
    // name matches, without crossing into nested SceneInstance subtrees.
    Node* ResolveUniqueName(const std::string& name) const;

    // Component management
    void AddComponent(std::shared_ptr<Component> component);
    void RemoveComponent(std::shared_ptr<Component> component);
    template<typename T>
    std::shared_ptr<T> GetComponent() const;
    std::shared_ptr<Component> GetComponent(const std::string& typeName) const;
    const std::vector<std::shared_ptr<Component>>& GetComponents() const { return m_Components; }

    // Properties
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name);

    // Encode a node as a signal argument. Node-bearing signals (child_entered_tree,
    // body_entered, ...) pass nodes through the JSON payload as
    // {"__lupine_node__": "<uuid>"}; scripting bindings recognise this shape and
    // hand the handler a real node handle. Returns null json when node is null.
    static nlohmann::json NodeArg(const Node* node);

    const UUID& GetUUID() const { return m_UUID; }

    bool IsActive() const { return m_Active; }
    void SetActive(bool active);
    bool IsActiveInHierarchy() const;

    // True once this node's ready pass has run (components awoken/readied, "ready" emitted).
    bool IsReady() const { return m_Ready; }

    bool IsVisible() const { return m_Visible; }
    void SetVisible(bool visible);
    bool IsVisibleInHierarchy() const;

    // Scene ownership
    Scene* GetScene() const { return m_Scene; }
    void SetScene(Scene* scene);

    // Groups (Godot-style node tagging). A node may belong to any number of named
    // groups; the scene tree can collect all nodes in a group. Group membership is
    // serialized with the node so it persists in .scene files.
    void AddToGroup(const std::string& group);
    void RemoveFromGroup(const std::string& group);
    bool IsInGroup(const std::string& group) const;
    std::vector<std::string> GetGroups() const;
    const std::unordered_set<std::string>& GetGroupSet() const { return m_Groups; }

    // Interface conformance (capability contracts; see InterfaceRegistry). A node
    // implements an interface when any of its components declares it — directly or
    // through interface inheritance. This is derived state (there is no per-node
    // interface list), so it always reflects the node's current components/scripts.
    bool ImplementsInterface(const std::string& interfaceName) const;
    // The interfaces declared directly by this node's components, NOT expanded to
    // include the base interfaces they inherit from.
    std::vector<std::string> GetDeclaredInterfaces() const;
    // The full set of interfaces this node satisfies, including base interfaces
    // implied by inheritance, de-duplicated.
    std::vector<std::string> GetImplementedInterfaces() const;
    // Verify this node against an interface's effective method+signal contract.
    // Returns { interface, exists, conforms, missing_methods, missing_signals }.
    nlohmann::json VerifyInterface(const std::string& interfaceName) const;

    // Lifecycle methods (called by Scene)
    virtual void OnReady();
    virtual void OnDestroy();
    virtual void OnInput(float deltaTime);
    // Dispatch a single discrete input event to this node's components and,
    // recursively, its children (event-based input; see input::InputEvent).
    virtual void OnInputEvent(const nlohmann::json& event);
    virtual void OnProcess(float deltaTime);
    virtual void OnPhysicsProcess(float deltaTime);
    virtual void OnRender();

protected:
    UUID m_UUID;
    std::string m_Name;
    Node* m_Parent;
    Scene* m_Scene;
    std::vector<std::shared_ptr<Node>> m_Children;
    std::vector<std::shared_ptr<Component>> m_Components;
    std::unordered_set<std::string> m_Groups;
    bool m_Active;
    bool m_Visible;
    bool m_Ready;
    bool m_UniqueNameInOwner;

private:
    // Walks ancestors to find the root of this node's unique-name scope.
    Node* GetUniqueNameScopeRoot() const;
    // Depth-first search for a uniquely-named node within a scope, stopping at
    // nested SceneInstance boundaries.
    static Node* FindUniqueNameInScope(Node* root, const std::string& name);

    // Backing store for GetTreeStructureVersion(); advanced by BumpTreeStructureVersion()
    // from every topology-changing operation.
    static uint64_t s_TreeStructureVersion;
    static void BumpTreeStructureVersion() { ++s_TreeStructureVersion; }

    // Backing store for GetRenderEpoch(); advanced by MarkRenderDirty().
    static uint64_t s_RenderEpoch;
};

/**
 * Node2D - Base class for 2D nodes
 */
class Node2D : public Node {
public:
    Node2D();
    explicit Node2D(const std::string& name);

    std::string GetTypeName() const override { return "Node2D"; }
    void RegisterProperties() override;

    // 2D Transform
    const math::Vec2& GetPosition() const { return m_Position; }
    void SetPosition(const math::Vec2& position) {
        if (m_Position == position) return;
        m_Position = position;
        MarkRenderDirty();
    }

    float GetRotation() const { return m_Rotation; }
    void SetRotation(float rotation) {
        if (m_Rotation == rotation) return;
        m_Rotation = rotation;
        MarkRenderDirty();
    }

    const math::Vec2& GetScale() const { return m_Scale; }
    void SetScale(const math::Vec2& scale) {
        if (m_Scale == scale) return;
        m_Scale = scale;
        MarkRenderDirty();
    }

    int GetZIndex() const { return m_ZIndex; }
    void SetZIndex(int zIndex) {
        if (m_ZIndex == zIndex) return;
        m_ZIndex = zIndex;
        MarkRenderDirty();
    }

    // Global transform (considering parent hierarchy)
    math::Vec2 GetGlobalPosition() const;
    float GetGlobalRotation() const;
    math::Vec2 GetGlobalScale() const;
    math::Mat4 GetTransformMatrix() const;
    math::Mat4 GetGlobalTransformMatrix() const;

protected:
    math::Vec2 m_Position;
    float m_Rotation;
    math::Vec2 m_Scale;
    int m_ZIndex;
};

/**
 * Node3D - Base class for 3D nodes
 */
class Node3D : public Node {
public:
    Node3D();
    explicit Node3D(const std::string& name);

    std::string GetTypeName() const override { return "Node3D"; }
    void RegisterProperties() override;

    // 3D Transform
    const math::Vec3& GetPosition() const { return m_Position; }
    void SetPosition(const math::Vec3& position) {
        if (m_Position == position) return;
        m_Position = position;
        MarkRenderDirty();
    }

    const math::Quat& GetRotation() const { return m_Rotation; }
    void SetRotation(const math::Quat& rotation) {
        if (m_Rotation == rotation) return;
        m_Rotation = rotation;
        MarkRenderDirty();
    }

    const math::Vec3& GetScale() const { return m_Scale; }
    void SetScale(const math::Vec3& scale) {
        if (m_Scale == scale) return;
        m_Scale = scale;
        MarkRenderDirty();
    }

    // Global transform (considering parent hierarchy)
    math::Vec3 GetGlobalPosition() const;
    math::Quat GetGlobalRotation() const;
    math::Vec3 GetGlobalScale() const;

    math::Mat4 GetTransformMatrix() const;
    math::Mat4 GetGlobalTransformMatrix() const;

protected:
    math::Vec3 m_Position;
    math::Quat m_Rotation;
    math::Vec3 m_Scale;
};

// Template implementation must be in header
template<typename T>
std::shared_ptr<T> Node::GetComponent() const {
    for (const auto& component : m_Components) {
        auto typed = std::dynamic_pointer_cast<T>(component);
        if (typed) {
            return typed;
        }
    }
    return nullptr;
}

} // namespace core
} // namespace lupine

