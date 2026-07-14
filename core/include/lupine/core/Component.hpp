#pragma once

#include "lupine/core/Core.hpp"
#include "lupine/core/SignalObject.hpp"
#include "lupine/core/InterfaceRegistry.hpp"
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
class Component : public SignalObject, public ISerializable {
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
    void SetEnabled(bool enabled) {
        if (m_Enabled == enabled) return;
        m_Enabled = enabled;
        NotifyRenderStateChanged();
    }

    // Owner node
    Node* GetOwner() const { return m_Owner; }
    void SetOwner(Node* owner) { m_Owner = owner; }

    // Runs OnDestroy() exactly once, however many teardown paths reach this component -
    // SceneManager's unload sweep, Scene::Shutdown, ~Node and the QueueFree handler can all
    // reach the same component during a single scene change. Component authors override
    // OnDestroy(); the engine always calls this instead of invoking it directly, so teardown
    // that is not idempotent (unregistering a physics body, releasing a GPU handle) runs once.
    void DispatchDestroy() {
        if (m_Destroyed) {
            return;
        }
        m_Destroyed = true;
        OnDestroy();
    }

    bool IsDestroyed() const { return m_Destroyed; }

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

    // Interface conformance (capability contracts; see InterfaceRegistry).
    // The interfaces this component declares it implements. The default
    // implementation returns whatever the concrete type registered via
    // REGISTER_COMPONENT_INTERFACES. A node aggregates this across its
    // components so the runtime / editor can query "every Damageable" and
    // verify the method+signal contract. Override to add per-instance
    // interfaces (e.g. ScriptComponent returns its --@interface declarations).
    virtual std::vector<std::string> GetImplementedInterfaces() const;

    // Whether this component exposes a callable method of the given name, used
    // by interface contract verification. The default returns false; script
    // components override it to consult the script environment, and native
    // components that expose CallMethod handlers may override it to advertise
    // the methods they answer.
    virtual bool HasInterfaceMethod(const std::string& method) const {
        (void)method; return false;
    }

    // Ancestry / type-chain reflection.
    // Returns this component's type names ordered from most-derived to
    // least-derived. For a custom component "FireSprite" extending custom
    // "MySprite" extending the built-in "Sprite2D", this returns
    // {"FireSprite", "MySprite", "Sprite2D"}. The default returns a
    // single-element chain of GetTypeName(); ScriptedComponentWrapper (and any
    // other carrier that wraps a base component) overrides this to prepend its
    // own type name to the base component's chain, so the full inheritance
    // ancestry is observable from scripting, the editor and the C-API.
    virtual std::vector<std::string> GetTypeChain() const {
        return { GetTypeName() };
    }

    // True if this component is, or derives from, the named type. Walks
    // GetTypeChain(), so it answers correctly at every level of a custom
    // inheritance chain down to the built-in base component.
    bool IsInstanceOf(const std::string& typeName) const {
        for (const std::string& chainEntry : GetTypeChain()) {
            if (chainEntry == typeName) {
                return true;
            }
        }
        return false;
    }

    // Advance the global render epoch (Node::MarkRenderDirty on the owner) so the
    // renderer rebuilds its cached draw list / shadow maps. Called automatically by
    // SetEnabled and SetPropertyValue; call it directly from a component that changes
    // its rendered output another way (e.g. generating geometry each frame).
    void NotifyRenderStateChanged();

    // Lifecycle hooks - override these in derived classes
    virtual void OnAwake() {}
    virtual void OnReady() {}
    virtual void OnDestroy() {}
    virtual void OnUpdate(float deltaTime) { (void)deltaTime; }
    virtual void OnInput(float deltaTime) { (void)deltaTime; }
    virtual void OnUnhandledInput(float deltaTime) { (void)deltaTime; }  // Called for unhandled input

    // Called once per discrete input event (key/mouse/gamepad) routed through the
    // scene tree, carrying the event payload (see input::InputEvent::ToJson).
    // Distinct from the per-frame OnInput(deltaTime). Default is a no-op.
    virtual void OnInputEvent(const nlohmann::json& event) { (void)event; }

    /**
     * Input routing control.
     *
     * When a component returns true from WantsChildInputControl(), the owning
     * Node delegates input dispatch to its child nodes to this component via
     * DispatchChildInput() instead of forwarding OnInput() to each child
     * directly. This lets a component swallow, gate, or remap input destined for
     * the subtree it owns (e.g. SubViewport's no-input / isolated / shared modes).
     *
     * Only the first enabled component on a node that returns true takes control.
     * The default implementation returns false, preserving normal dispatch.
     */
    virtual bool WantsChildInputControl() const { return false; }

    /**
     * Dispatch input to the owning node's children.
     * Called by Node::OnInput() only when WantsChildInputControl() returns true.
     * The component is responsible for forwarding OnInput() to whichever children
     * should receive it (and may set up a remapped coordinate context first).
     */
    virtual void DispatchChildInput(float deltaTime,
                                    const std::vector<std::shared_ptr<Node>>& children) {
        (void)deltaTime; (void)children;
    }
    virtual void OnProcess(float deltaTime) { (void)deltaTime; }
    virtual void OnPhysicsProcess(float deltaTime) { (void)deltaTime; }
    virtual void OnRender() {}
    virtual void OnLateUpdate(float deltaTime) { (void)deltaTime; }
    virtual void OnEnterTree() {}  // Called when added to scene tree
    virtual void OnExitTree() {}   // Called when removed from scene tree
    virtual void OnVisibilityChanged(bool visible) { (void)visible; }  // Called when visibility changes

    /**
     * Phases of a wholesale physics-world rebuild.
     *
     * SceneManager::LoadScene clears the 2D/3D physics worlds outright on a scene swap,
     * destroying every body in them. Components in the outgoing scene are destroyed too,
     * so they never notice - but a component inside an `add_scene` overlay *survives* the
     * swap (overlays are re-parented onto the incoming scene), and is left holding handles
     * into a world that no longer exists. Such a component must rebuild.
     *
     * The three phases mirror the normal creation order so a collider always has a live
     * body to attach to:
     *  - SaveState:        runs while the old bodies are still alive. Cache anything whose
     *                      authoritative copy lives in the body (velocities) - it is about
     *                      to be destroyed.
     *  - RecreateBodies:   runs against the fresh, empty world (mirrors OnAwake). Drop the
     *                      stale handles and create the body again. Do NOT call the normal
     *                      Destroy path first: the body is already gone, so anything that
     *                      dereferences it is a use-after-free.
     *  - AttachColliders:  runs once every body exists (mirrors OnReady). Re-attach colliders
     *                      and re-sync the body to the node's transform.
     */
    enum class PhysicsWorldRebuildPhase {
        SaveState,
        RecreateBodies,
        AttachColliders
    };

    /**
     * Rebuild this component's physics state after the physics worlds were cleared.
     * Default is a no-op: only components that own a body or a collider need to react.
     * @param phase Which phase of the rebuild is running (see PhysicsWorldRebuildPhase)
     */
    virtual void OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) { (void)phase; }

    // Editor gizmo hooks - override these for custom gizmo behavior
    /**
     * Called when a gizmo scale operation is performed on the owner node
     * @param scaleDelta The amount of scale change (can be positive or negative)
     * @param axis The axis being scaled (0=X, 1=Y, 2=Z, -1=uniform)
     * @param is3D Whether this is a 3D scale operation (true) or 2D (false)
     * @return true if the component handled the scaling (prevents default node scale), false otherwise
     */
    virtual bool OnGizmoScale(float scaleDelta, int axis, bool is3D) {
        (void)scaleDelta; (void)axis; (void)is3D;
        return false;
    }

    /**
     * Called when a property value is changed via the editor.
     * Override this to sync property values with internal member variables and trigger side effects.
     * @param propertyName The name of the property that changed
     * @param newValue The new property value as JSON
     */
    virtual void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
        (void)propertyName; (void)newValue;
    }

    /**
     * Invoke a named control method on this component with JSON arguments, returning
     * a JSON result. This is the generic, language-agnostic entry point scripts use
     * (ComponentRef::Call) to drive components that expose behaviour beyond plain
     * property get/set (e.g. AnimationPlayer::Play, AnimationTree::SetFloat). The
     * default implementation handles nothing and returns null.
     *
     * @param method The method name.
     * @param args A JSON array of arguments (or null).
     * @return A JSON result (null when the method has no return value / is unknown).
     */
    virtual nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) {
        (void)method; (void)args; return nlohmann::json();
    }

    /**
     * Called when an asset file changes on disk.
     * The base implementation checks all file-type properties (PropertyHintType::File)
     * and triggers a reload if any match the changed path.
     *
     * @param changedPath The path of the file that changed (can be res:// or physical path)
     * @param resolvedChangedPath The resolved physical path of the changed file
     * @return true if this component was affected by the change
     */
    virtual bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath);

    /**
     * Returns true exactly once after the global font oversampling factor changes
     * (see SetFontOversample), i.e. after a resolution change re-bakes the font
     * atlases. Text components that cache glyph meshes call this each frame and
     * regenerate their mesh when it returns true so cached UVs stay in sync with
     * the repacked atlas.
     */
    bool ConsumeFontOversampleChanged();

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

    // Guards OnDestroy() against being run more than once. See DispatchDestroy().
    bool m_Destroyed = false;
    uint64_t m_FontOversampleGenSeen = 0;
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
        NotifyRenderStateChanged();
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

/**
 * Macro to declare that a native component type implements one or more
 * interfaces. Place it next to REGISTER_COMPONENT_TYPE in the component's .cpp.
 * Interface names are string literals:
 *
 *     REGISTER_COMPONENT_INTERFACES(HealthComponent, "Damageable", "Destructible")
 *
 * The registration runs at static-init time and persists across project scans,
 * so the editor can list implementers without instantiating the component and a
 * node reports the interface through Component::GetImplementedInterfaces().
 */
#define REGISTER_COMPONENT_INTERFACES(TypeName, ...) \
    namespace { \
        struct TypeName##InterfaceRegistrar { \
            TypeName##InterfaceRegistrar() { \
                const char* lupine_ifaces[] = { __VA_ARGS__ }; \
                for (const char* lupine_iface : lupine_ifaces) { \
                    lupine::core::InterfaceRegistry::GetInstance() \
                        .RegisterTypeConformance(#TypeName, lupine_iface); \
                } \
            } \
        }; \
        static TypeName##InterfaceRegistrar g_##TypeName##InterfaceRegistrar; \
    }

} // namespace core
} // namespace lupine

