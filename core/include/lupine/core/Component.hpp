#pragma once

#include "lupine/core/Core.hpp"
#include "ComponentProperty.hpp"
#include <string>
#include <memory>
#include <vector>

namespace lupine {
namespace core {

// Forward declaration
class Node;

/**
 * Base Component class - similar to Unity's component system
 * Components define behavior and can be attached to nodes
 * 
 * Components now support declarative properties with types, defaults, and hints
 */
class Component : public ISerializable {
public:
    Component();
    explicit Component(const std::string& name);
    virtual ~Component();

    // ISerializable interface
    std::string GetTypeName() const override { return "Component"; }
    void RegisterProperties() override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;

    // Component properties (built-in)
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    const UUID& GetUUID() const { return m_UUID; }

    bool IsEnabled() const { return m_Enabled; }
    void SetEnabled(bool enabled) { m_Enabled = enabled; }

    // Owner node
    Node* GetOwner() const { return m_Owner; }
    void SetOwner(Node* owner) { m_Owner = owner; }

    // Custom property system
    // Override DefineProperties in derived classes to declare custom properties
    virtual void DefineProperties() {}
    
    // Access custom properties
    ComponentPropertyRegistry& GetPropertyRegistry() { return m_CustomProperties; }
    const ComponentPropertyRegistry& GetPropertyRegistry() const { return m_CustomProperties; }
    
    // Helper to get/set custom property values
    template<typename T>
    T GetPropertyValue(const std::string& name) const;
    
    template<typename T>
    void SetPropertyValue(const std::string& name, const T& value);
    
    // Get property descriptors (for editor/reflection)
    std::vector<PropertyDescriptor> GetPropertyDescriptors() const;

    // Lifecycle hooks - override these in derived classes
    virtual void OnAwake() {}
    virtual void OnReady() {}
    virtual void OnDestroy() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnInput(float deltaTime) {}
    virtual void OnProcess(float deltaTime) {}
    virtual void OnPhysicsProcess(float deltaTime) {}
    virtual void OnRender() {}
    virtual void OnLateUpdate(float deltaTime) {}

    // Editor gizmo hooks - override these for custom gizmo behavior
    /**
     * Called when a gizmo scale operation is performed on the owner node
     * @param scaleDelta The amount of scale change (can be positive or negative)
     * @param axis The axis being scaled (0=X, 1=Y, 2=Z, -1=uniform)
     * @param is3D Whether this is a 3D scale operation (true) or 2D (false)
     * @return true if the component handled the scaling (prevents default node scale), false otherwise
     */
    virtual bool OnGizmoScale(float scaleDelta, int axis, bool is3D) { return false; }

    /**
     * Called when a property value is changed via the editor.
     * Override this to sync property values with internal member variables and trigger side effects.
     * @param propertyName The name of the property that changed
     * @param newValue The new property value as JSON
     */
    virtual void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {}

protected:
    // Helper method to define a property (call in DefineProperties override)
    void DefineProperty(const PropertyDescriptor& desc) {
        m_CustomProperties.DefineProperty(desc);
    }

    UUID m_UUID;
    std::string m_Name;
    Node* m_Owner;
    bool m_Enabled;
    
    ComponentPropertyRegistry m_CustomProperties;
    bool m_PropertiesDefined = false;
};

// Template implementations
template<typename T>
T Component::GetPropertyValue(const std::string& name) const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty(name);
    if (prop) {
        return prop->GetValue<T>();
    }
    return T{};
}

template<typename T>
void Component::SetPropertyValue(const std::string& name, const T& value) {
    ComponentProperty* prop = m_CustomProperties.GetProperty(name);
    if (prop) {
        prop->SetValue<T>(value);
    }
}

/**
 * Macro to help register component types
 */
#define REGISTER_COMPONENT_TYPE(TypeName) \
    namespace { \
        struct TypeName##Registrar { \
            TypeName##Registrar() { \
                lupine::core::TypeRegistry::GetInstance().RegisterType( \
                    #TypeName, \
                    []() -> std::shared_ptr<lupine::core::ISerializable> { \
                        return std::make_shared<TypeName>(); \
                    } \
                ); \
            } \
        }; \
        static TypeName##Registrar g_##TypeName##Registrar; \
    }

} // namespace core
} // namespace lupine

