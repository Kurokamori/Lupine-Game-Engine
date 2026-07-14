#pragma once

#include "Component.hpp"
#include "ExtensionManager.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include <string>

namespace lupine {
namespace core {

/**
 * Bridges a native (C++ DLL) component implementation into the engine scene.
 *
 * This is the native-extension analogue of ScriptedComponentWrapper: it is a
 * real lupine::core::Component subclass that holds an opaque plugin instance and
 * forwards every lifecycle hook, property declaration, method call and the
 * property-change notification across the stable C ABI to the plugin's C++ code.
 *
 * Property values live in this wrapper's standard ComponentPropertyRegistry
 * (declared by the plugin via host->define_property during DefineProperties), so
 * the editor inspector and scene serialization round-trip them with no special
 * handling — exactly like any built-in component's custom properties.
 *
 * One wrapper is created per scene instance of the class via the TypeRegistry
 * factory installed by NativeExtensionRegistry::RegisterClass.
 */
class NativeComponentWrapper : public Component, public IRenderableComponent {
public:
    NativeComponentWrapper();
    explicit NativeComponentWrapper(const std::string& className);
    ~NativeComponentWrapper() override;

    // ISerializable / Component
    std::string GetTypeName() const override { return m_ClassName; }
    void DefineProperties() override;

    // Lifecycle hooks — each forwards to the plugin only if the class advertised it.
    void OnAwake() override;
    void OnReady() override;
    void OnDestroy() override;
    void OnUpdate(float deltaTime) override;
    void OnInput(float deltaTime) override;
    void OnInputEvent(const nlohmann::json& event) override;
    void OnProcess(float deltaTime) override;
    void OnPhysicsProcess(float deltaTime) override;
    void OnRender() override;
    void OnLateUpdate(float deltaTime) override;
    void OnEnterTree() override;
    void OnExitTree() override;

    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;
    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // ===== IRenderableComponent interface =====
    // A native class renders by advertising LUPINE_HOOK_DRAW and implementing the
    // build_draw_commands vtable slot, which is forwarded the active render
    // context across the C ABI so the plugin's render_draw_* calls record real,
    // runtime-visible geometry. World bounds come from the optional
    // get_render_bounds slot, falling back to a node-transform AABB. A class that
    // does not declare LUPINE_HOOK_DRAW contributes nothing (empty bounds, not
    // dynamic) so it never disables the renderer's static-content caching.
    // Editor-only debug drawing is a separate path (OnRender + debug_draw_*).
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    SpatialType getSpatialType() const override;
    bool isRenderContentDynamic() const override;

    /** The registered class name this wrapper instantiates. */
    const std::string& GetClassName() const { return m_ClassName; }

    /** True if a plugin instance was successfully created. */
    bool IsInstanceValid() const { return m_Instance != nullptr; }

private:
    /** Stable opaque handle this wrapper presents to the plugin (== this). */
    LupineComponentHandle SelfHandle() {
        return reinterpret_cast<LupineComponentHandle>(static_cast<Component*>(this));
    }

    std::string m_ClassName;
    const NativeComponentClass* m_Class = nullptr; // owned by NativeExtensionRegistry
    LupineInstance m_Instance = nullptr;           // plugin's C++ object
    bool m_Destroyed = false;
};

} // namespace core
} // namespace lupine
