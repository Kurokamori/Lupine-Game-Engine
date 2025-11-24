#pragma once

#include "lupine/core/Core.hpp"
#include "lupine/math/Math.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace lupine {
namespace core {

// Forward declarations
class Component;
class Scene;

/**
 * Base Node class - similar to Godot's Node system
 * Nodes form a tree hierarchy and can have components attached
 */
class Node : public ISerializable {
public:
    Node();
    explicit Node(const std::string& name);
    virtual ~Node();

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
    
    Node* GetParent() const { return m_Parent; }
    void SetParent(Node* parent);
    
    // Find nodes in hierarchy
    std::shared_ptr<Node> FindNode(const std::string& path) const;
    std::string GetPath() const;

    // Component management
    void AddComponent(std::shared_ptr<Component> component);
    void RemoveComponent(std::shared_ptr<Component> component);
    template<typename T>
    std::shared_ptr<T> GetComponent() const;
    std::shared_ptr<Component> GetComponent(const std::string& typeName) const;
    const std::vector<std::shared_ptr<Component>>& GetComponents() const { return m_Components; }

    // Properties
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    const UUID& GetUUID() const { return m_UUID; }

    bool IsActive() const { return m_Active; }
    void SetActive(bool active);
    bool IsActiveInHierarchy() const;

    bool IsVisible() const { return m_Visible; }
    void SetVisible(bool visible) { m_Visible = visible; }
    bool IsVisibleInHierarchy() const;

    // Scene ownership
    Scene* GetScene() const { return m_Scene; }
    void SetScene(Scene* scene);

    // Lifecycle methods (called by Scene)
    virtual void OnReady();
    virtual void OnDestroy();
    virtual void OnInput(float deltaTime);
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
    bool m_Active;
    bool m_Visible;
    bool m_Ready;
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
    void SetPosition(const math::Vec2& position) { m_Position = position; }

    float GetRotation() const { return m_Rotation; }
    void SetRotation(float rotation) { m_Rotation = rotation; }

    const math::Vec2& GetScale() const { return m_Scale; }
    void SetScale(const math::Vec2& scale) { m_Scale = scale; }

    int GetZIndex() const { return m_ZIndex; }
    void SetZIndex(int zIndex) { m_ZIndex = zIndex; }

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
    void SetPosition(const math::Vec3& position) { m_Position = position; }

    const math::Quat& GetRotation() const { return m_Rotation; }
    void SetRotation(const math::Quat& rotation) { m_Rotation = rotation; }

    const math::Vec3& GetScale() const { return m_Scale; }
    void SetScale(const math::Vec3& scale) { m_Scale = scale; }

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

