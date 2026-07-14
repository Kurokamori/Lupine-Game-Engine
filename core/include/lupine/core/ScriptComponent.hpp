#pragma once

#include "Component.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/scripting/ScriptingCore.hpp"
#include "lupine/scripting/ScriptAPI.hpp"
#include "lupine/scripting/LuaEnvironment.hpp"
#include "lupine/network/NetworkTypes.hpp"
#ifdef LUPINE_HAS_MRUBY
#include "lupine/scripting/MRubyEnvironment.hpp"
#endif
#ifdef LUPINE_HAS_MICROPYTHON
#include "lupine/scripting/MicroPythonEnvironment.hpp"
#endif
#include <memory>
#include <string>
#include <map>

// Forward declare MicroPythonEnvironment if header not available
#ifndef LUPINE_HAS_MICROPYTHON
namespace lupine { namespace scripting { class MicroPythonEnvironment; } }
#endif

namespace lupine {
namespace core {

/**
 * Parse a value returned by a script's get_render_bounds() callback into a
 * world-space AABB. Accepts these shapes (vectors as {x,y,z} objects or [x,y,z]
 * arrays):
 *   { "min": vec, "max": vec }
 *   { "center": vec, "size": vec }     (size is the full extent, not half)
 *   [minx, miny, minz, maxx, maxy, maxz]
 * Returns false (leaving outBounds untouched) if the value cannot be parsed.
 */
bool ParseScriptRenderBounds(const nlohmann::json& value, AABB& outBounds);

/**
 * Base class for script components
 * Provides common functionality for Python and Lua script components
 */
class ScriptComponent : public Component, public IRenderableComponent {
public:
    ScriptComponent();
    explicit ScriptComponent(const std::string& name);
    virtual ~ScriptComponent();

    // ISerializable interface
    std::string GetTypeName() const override { return "ScriptComponent"; }
    void RegisterProperties() override;
    void DefineProperties() override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;

    // Script management
    const std::string& GetScriptPath() const { return m_ScriptPath; }
    void SetScriptPath(const std::string& path);
    
    bool LoadScript();
    bool ReloadScript();

    // A "tool" script (declared with --@tool / #@tool at the top of the file) is
    // permitted to run its lifecycle callbacks inside the editor, mirroring
    // Godot's @tool. A regular ("game") script only runs at runtime; when the
    // owning scene is in editor mode its callbacks are suppressed so merely
    // opening a scene never executes gameplay logic. Parsed during LoadScript.
    bool IsTool() const { return m_IsTool; }

    virtual scripting::ScriptLanguage GetLanguage() const = 0;
    
    // Get the script's display name (used in editor)
    std::string GetScriptDisplayName() const;
    
    // Export properties (parsed from script)
    struct ExportProperty {
        std::string name;
        std::string group;  // Export group for organizing properties in the inspector
        PropertyDescriptor descriptor;
    };

    const std::vector<ExportProperty>& GetExportProperties() const { return m_ExportProperties; }

    // Get the ScriptAPI instance
    scripting::ScriptAPI* GetScriptAPI() { return m_ScriptAPI.get(); }
    const scripting::ScriptAPI* GetScriptAPI() const { return m_ScriptAPI.get(); }

    // Public access to the underlying script environment. Used by the SceneManager
    // to inject singleton/autoload globals once the script has been loaded.
    scripting::IScriptEnvironment* GetScriptEnvironment() { return GetEnvironment(); }

    // Lifecycle hooks - override these in derived classes
    void OnAwake() override;
    void OnReady() override;
    void OnDestroy() override;
    void OnInput(float deltaTime) override;
    void OnInputEvent(const nlohmann::json& event) override;
    void OnProcess(float deltaTime) override;
    void OnPhysicsProcess(float deltaTime) override;
    void OnRender() override;
    void OnEnterTree() override;
    void OnExitTree() override;
    void OnVisibilityChanged(bool visible) override;
    void OnUnhandledInput(float deltaTime) override;
    void OnLateUpdate(float deltaTime) override;

    // Property change notification
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // Generic method dispatch: route a scripted call (ComponentRef::Call /
    // NodeRef::Call) to the matching top-level function in the script. Unknown
    // method names return null without entering the script VM.
    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // ===== IRenderableComponent interface =====
    // A pure script component renders by defining an on_draw(...) function, which
    // is called during the gather stage with the active render context bound so
    // the script's draw_* calls record real, runtime-visible geometry. A script
    // with no on_draw contributes nothing (empty bounds, not dynamic) so it does
    // not disable the renderer's static caching. Editor-only debug drawing is a
    // separate path (on_render + editor_draw_*), independent of on_draw.
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    SpatialType getSpatialType() const override;
    bool isRenderContentDynamic() const override;

    // Signal dispatch: route a connected signal to the matching script function.
    bool InvokeSignalMethod(const std::string& method, const SignalArgs& args) override;

    // Interface conformance. A script declares the interfaces it implements with
    // --@interface (Lua) / #@interface (Ruby/Python) annotations; these are
    // returned here so the node aggregates them. HasInterfaceMethod consults the
    // script environment so contract verification can tell whether a required
    // method is actually defined.
    std::vector<std::string> GetImplementedInterfaces() const override;
    bool HasInterfaceMethod(const std::string& method) const override;

    // The interfaces declared directly in this script via @interface annotations.
    const std::vector<std::string>& GetScriptInterfaces() const { return m_Interfaces; }

    // RPC configuration parsed from `--@rpc` annotations in the script, keyed by
    // function name. Used by the networking RPC layer to decide authority,
    // reliability, and call-local behaviour. Returns nullptr if `method` was not
    // declared as an RPC.
    const network::RpcConfig* GetRpcConfig(const std::string& method) const;
    const std::map<std::string, network::RpcConfig>& GetRpcConfigs() const { return m_RpcConfigs; }

protected:
    std::string m_ScriptPath;
    std::string m_ExtendsPath;  // Path to base script (for inheritance)
    std::vector<ExportProperty> m_ExportProperties;
    std::map<std::string, network::RpcConfig> m_RpcConfigs;  // RPC fn name -> config
    std::vector<std::string> m_Interfaces;  // Interfaces declared via @interface
    bool m_ScriptLoaded = false;
    bool m_IsTool = false;  // Declared via --@tool / #@tool; runs in editor too
    std::unique_ptr<scripting::ScriptAPI> m_ScriptAPI;

    // Cache of the world-space bounds returned by the script's get_render_bounds()
    // callback, keyed by the global render epoch so the script is only re-invoked
    // when something in the scene actually changed (transform/visibility/property
    // edits all advance the epoch). UINT64_MAX means "not yet computed".
    mutable uint64_t m_RenderBoundsEpoch = UINT64_MAX;
    mutable AABB m_RenderBoundsCache;
    mutable bool m_RenderBoundsValid = false;

    // Pending export property values from deserialization (applied after script loads)
    nlohmann::json m_PendingExportProperties;

    // Parse export properties and extends directive from script content
    virtual void ParseExportProperties(const std::string& scriptContent) = 0;
    virtual void ParseExtendsDirective(const std::string& scriptContent);

    // Parse the --@tool / #@tool directive, setting m_IsTool when present so the
    // script's callbacks are allowed to run inside the editor.
    void ParseToolDirective(const std::string& scriptContent);

    // Shared implementation of ParseExportProperties: runs the unified
    // ScriptAnnotationParser with the language's comment prefix ("--" for Lua,
    // "#" for Python/Ruby), populating m_ExportProperties and registering each
    // parsed descriptor via DefineProperty.
    void ParseExportsWithPrefix(const std::string& scriptContent, const std::string& commentPrefix);

    // Parse @signal annotations and register them as user signals on this
    // component. Comment prefix is selected from the script language.
    // Example: --@signal health_changed(int amount)   /   #@signal died
    void ParseSignals(const std::string& scriptContent);

    // Parse @rpc annotations into m_RpcConfigs. Comment prefix is language-agnostic.
    // Example: --@rpc any_peer unreliable call_local move(float x, float y)
    //          #@rpc authority reliable take_damage
    // The mode/transfer/call_local tokens are optional and order-independent; the
    // last token is the RPC function name.
    void ParseRpcs(const std::string& scriptContent);

    // Parse @interface annotations declaring the interfaces this script
    // implements, and register each interface's required signals on this
    // component so connect/emit works without boilerplate.
    // Example: --@interface Damageable    /    #@interface Damageable, Destructible
    void ParseInterfaces(const std::string& scriptContent);

    // Load base script (if extends is set)
    bool LoadBaseScript();

    // Get script environment
    virtual scripting::IScriptEnvironment* GetEnvironment() = 0;

    // Helper to call lifecycle functions in script
    bool CallScriptFunction(const std::string& functionName);
    bool CallScriptFunctionWithDelta(const std::string& functionName, float deltaTime);

    // True when this script's callbacks may run in the current context: always at
    // runtime, and inside the editor only for @tool scripts. The single gate that
    // keeps game scripts from executing when a scene is opened in the editor.
    bool ScriptCallbacksAllowed() const;
};

#ifdef LUPINE_HAS_MICROPYTHON
/**
 * Python script component (uses MicroPython for embedded scripting)
 */
class PythonScriptComponent : public ScriptComponent {
public:
    PythonScriptComponent();
    explicit PythonScriptComponent(const std::string& name);
    ~PythonScriptComponent() override;

    std::string GetTypeName() const override { return "PythonScriptComponent"; }

    scripting::ScriptLanguage GetLanguage() const override { return scripting::ScriptLanguage::Python; }

    // Get the MicroPython environment
    scripting::MicroPythonEnvironment* GetMicroPythonEnvironment() { return m_MicroPythonEnv.get(); }

protected:
    void ParseExportProperties(const std::string& scriptContent) override;
    scripting::IScriptEnvironment* GetEnvironment() override { return m_MicroPythonEnv.get(); }

private:
    std::unique_ptr<scripting::MicroPythonEnvironment> m_MicroPythonEnv;
};
#endif // LUPINE_HAS_MICROPYTHON

/**
 * Lua script component
 */
class LuaScriptComponent : public ScriptComponent {
public:
    LuaScriptComponent();
    explicit LuaScriptComponent(const std::string& name);
    ~LuaScriptComponent() override;

    std::string GetTypeName() const override { return "LuaScriptComponent"; }

    scripting::ScriptLanguage GetLanguage() const override { return scripting::ScriptLanguage::Lua; }

    // Get the Lua environment
    scripting::LuaEnvironment* GetLuaEnvironment() { return m_LuaEnv.get(); }

protected:
    void ParseExportProperties(const std::string& scriptContent) override;
    scripting::IScriptEnvironment* GetEnvironment() override { return m_LuaEnv.get(); }

private:
    std::unique_ptr<scripting::LuaEnvironment> m_LuaEnv;
};

#ifdef LUPINE_HAS_MRUBY
/**
 * mRuby script component
 */
class MRubyScriptComponent : public ScriptComponent {
public:
    MRubyScriptComponent();
    explicit MRubyScriptComponent(const std::string& name);
    ~MRubyScriptComponent() override;

    std::string GetTypeName() const override { return "MRubyScriptComponent"; }

    scripting::ScriptLanguage GetLanguage() const override { return scripting::ScriptLanguage::MRuby; }

    // Get the mRuby environment
    scripting::MRubyEnvironment* GetMRubyEnvironment() { return m_MRubyEnv.get(); }

protected:
    void ParseExportProperties(const std::string& scriptContent) override;
    scripting::IScriptEnvironment* GetEnvironment() override { return m_MRubyEnv.get(); }

private:
    std::unique_ptr<scripting::MRubyEnvironment> m_MRubyEnv;
};
#endif // LUPINE_HAS_MRUBY

} // namespace core
} // namespace lupine
