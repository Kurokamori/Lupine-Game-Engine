#pragma once

#include "Component.hpp"
#include "CustomComponentDefinition.hpp"
#include "ScriptComponent.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/scripting/ScriptingCore.hpp"
#include "lupine/scripting/ScriptAPI.hpp"
#include "lupine/scripting/LuaEnvironment.hpp"
#ifdef LUPINE_HAS_MRUBY
#include "lupine/scripting/MRubyEnvironment.hpp"
#endif
#ifdef LUPINE_HAS_MICROPYTHON
#include "lupine/scripting/MicroPythonEnvironment.hpp"
#endif
#include <memory>
#include <string>
#include <set>
#include <vector>

namespace lupine {
namespace core {

/**
 * A component wrapper that combines a base C++ component with script behavior.
 *
 * This allows users to create custom component types via scripts that can:
 * 1. Inherit from built-in components (Sprite2D, Label, Button, etc.)
 * 2. Add custom script behavior via lifecycle hooks
 * 3. Define custom export properties
 * 4. Override or extend base component behavior
 *
 * The wrapper manages:
 * - Instantiation of the base component (if any)
 * - Script environment setup (Lua, mRuby, or MicroPython)
 * - Property merging between base and script
 * - Lifecycle delegation with override support
 */
class ScriptedComponentWrapper : public Component, public IRenderableComponent {
public:
    /**
     * Default constructor - creates an uninitialized wrapper
     */
    ScriptedComponentWrapper();

    /**
     * Construct a wrapper for a specific custom component type
     * @param customTypeName The name of the custom component class (e.g., "MySprite")
     */
    explicit ScriptedComponentWrapper(const std::string& customTypeName);

    ~ScriptedComponentWrapper() override;

    // ISerializable interface
    std::string GetTypeName() const override { return m_CustomTypeName; }

    // Ancestry: this custom type name followed by the full type chain of the
    // wrapped base component. Because the base may itself be a
    // ScriptedComponentWrapper, this recurses to produce the complete
    // inheritance chain (e.g. {"FireSprite", "MySprite", "Sprite2D"}).
    std::vector<std::string> GetTypeChain() const override;

    void RegisterProperties() override;
    void DefineProperties() override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;

    // Lifecycle hooks - implement dual model (extend vs override)
    void OnAwake() override;
    void OnReady() override;
    void OnDestroy() override;
    void OnUpdate(float deltaTime) override;
    void OnInput(float deltaTime) override;
    void OnInputEvent(const nlohmann::json& event) override;
    void OnUnhandledInput(float deltaTime) override;
    void OnProcess(float deltaTime) override;
    void OnPhysicsProcess(float deltaTime) override;
    void OnRender() override;
    void OnLateUpdate(float deltaTime) override;
    void OnEnterTree() override;
    void OnExitTree() override;
    void OnVisibilityChanged(bool visible) override;

    // Property change notification
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // Asset file change notification
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    // A custom component may extend a physics component (directly, or through a chain of
    // other custom components), in which case the wrapped base owns the body and has to
    // rebuild it when the physics worlds are cleared. Forwarding down the chain is all this
    // needs to do - the script itself has no physics handles of its own.
    void OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) override;

    // Editor gizmo hooks
    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // ===== IRenderableComponent interface =====
    // The wrapper is renderable so that custom components which extend a
    // renderable base (Sprite2D, Label, ...) actually draw, and so pure custom
    // components can draw via an on_draw script hook. Each call forwards to the
    // wrapped base component when it is itself renderable (which, for a nested
    // wrapper, recurses down the inheritance chain to the built-in renderable),
    // then layers script drawing on top following the same extend-vs-override
    // model used by the lifecycle hooks (on_draw extends, on_draw_override
    // replaces the base's drawing).
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;
    math::OBB getOrientedBounds() const override;
    bool isRenderContentDynamic() const override;
    bool IntersectRay(const math::Ray& ray, float& outDistance) const override;

    /**
     * Get the underlying base component (if any)
     */
    Component* GetBaseComponent() { return m_BaseComponent.get(); }
    const Component* GetBaseComponent() const { return m_BaseComponent.get(); }

    /**
     * Get the base component as a specific type
     */
    template<typename T>
    T* GetBaseComponentAs() {
        return dynamic_cast<T*>(m_BaseComponent.get());
    }

    template<typename T>
    const T* GetBaseComponentAs() const {
        return dynamic_cast<const T*>(m_BaseComponent.get());
    }

    /**
     * Get the script environment
     */
    scripting::IScriptEnvironment* GetScriptEnvironment() { return m_ScriptEnv.get(); }
    const scripting::IScriptEnvironment* GetScriptEnvironment() const { return m_ScriptEnv.get(); }

    /**
     * Get the ScriptAPI instance
     */
    scripting::ScriptAPI* GetScriptAPI() { return m_ScriptAPI.get(); }
    const scripting::ScriptAPI* GetScriptAPI() const { return m_ScriptAPI.get(); }

    /**
     * Get the custom component definition
     */
    const CustomComponentDefinition* GetDefinition() const;

    /**
     * Reload the script (for hot-reload support)
     */
    bool ReloadScript();

    /**
     * Check if the script is loaded
     */
    bool IsScriptLoaded() const { return m_ScriptLoaded; }

    /**
     * Whether this custom component is a tool component (declared with
     * --@tool / #@tool). Tool components run their script lifecycle hooks in the
     * editor as well as at runtime; game components run them only at runtime.
     */
    bool IsTool() const { return m_IsTool; }

    /**
     * Get the script path
     */
    const std::string& GetScriptPath() const { return m_ScriptPath; }

    /**
     * Get the custom type name
     */
    const std::string& GetCustomTypeName() const { return m_CustomTypeName; }

    /**
     * Get export properties defined in the script
     */
    const std::vector<CustomComponentDefinition::ExportProperty>& GetExportProperties() const {
        return m_ExportProperties;
    }

private:
    std::string m_CustomTypeName;
    std::string m_ScriptPath;

    // The wrapped base component (nullptr if no inheritance)
    std::shared_ptr<Component> m_BaseComponent;

    // Script environment (Lua, mRuby, or MicroPython)
    std::unique_ptr<scripting::IScriptEnvironment> m_ScriptEnv;
    std::unique_ptr<scripting::ScriptAPI> m_ScriptAPI;

    // Export properties parsed from script
    std::vector<CustomComponentDefinition::ExportProperty> m_ExportProperties;

    // Override hooks detected in the script
    std::set<std::string> m_OverrideHooks;

    // Regular hooks defined in the script
    std::set<std::string> m_DefinedHooks;

    // Script loaded flag
    bool m_ScriptLoaded = false;

    // Tool flag copied from the component's definition (--@tool / #@tool). When
    // false (a game component), the script hooks are suppressed while the owning
    // scene is open in the editor; the wrapped base component still runs so the
    // built-in keeps rendering/initializing in the editor as before.
    bool m_IsTool = false;

    // Cache for the script's optional get_render_bounds() callback, keyed by the
    // global render epoch (see ScriptComponent for the same scheme).
    mutable uint64_t m_RenderBoundsEpoch = UINT64_MAX;
    mutable AABB m_RenderBoundsCache;
    mutable bool m_RenderBoundsValid = false;

    // Pending export property values from deserialization
    nlohmann::json m_PendingExportProperties;

    // Pending base component properties from deserialization
    nlohmann::json m_PendingBaseProperties;

    /**
     * Return the wrapped base component as an IRenderableComponent, or nullptr
     * when there is no base or the base is not renderable. For a nested wrapper
     * this resolves through the chain because each wrapper is itself renderable.
     */
    IRenderableComponent* GetRenderableBase() const;

    /**
     * Ensure the wrapped base component's owner node matches this wrapper's
     * owner. OnAwake propagates the owner, but rendering in the editor can occur
     * before OnAwake runs, so the render/bounds entry points call this first.
     */
    void EnsureBaseOwner() const;

    /**
     * Initialize the base component based on the definition
     */
    void InitializeBaseComponent(const CustomComponentDefinition& def);

    /**
     * Initialize the script environment based on language
     */
    void InitializeScriptEnvironment(scripting::ScriptLanguage language);

    /**
     * Load and execute the script file
     */
    bool LoadScript();

    /**
     * Call a script function if it exists
     */
    bool CallScriptFunction(const std::string& name);

    /**
     * Call a script function with delta time parameter
     */
    bool CallScriptFunctionWithDelta(const std::string& name, float deltaTime);

    /**
     * Whether this component's script hooks may run in the current context:
     * always at runtime, and inside the editor only for tool components. Game
     * components' hooks are suppressed when the owning scene is in editor mode so
     * opening a scene never executes gameplay logic.
     */
    bool ScriptCallbacksAllowed() const;

    /**
     * Check if the script has an override for a lifecycle hook
     * @param hookName Base hook name without "_override" suffix (e.g., "on_render")
     */
    bool HasOverride(const std::string& hookName) const;

    /**
     * Check if the script has a regular hook defined
     */
    bool HasHook(const std::string& hookName) const;

    /**
     * Merge properties from base component and script exports
     */
    void MergeProperties();

    /**
     * Sync export property values to script environment
     */
    void SyncExportPropertiesToScript();

    /**
     * Apply pending deserialized values to properties
     */
    void ApplyPendingPropertyValues();
};

/**
 * Factory function to create ScriptedComponentWrapper instances.
 * This is registered with TypeRegistry for each custom component type.
 */
std::shared_ptr<ISerializable> CreateScriptedComponentWrapper(const std::string& customTypeName);

} // namespace core
} // namespace lupine
