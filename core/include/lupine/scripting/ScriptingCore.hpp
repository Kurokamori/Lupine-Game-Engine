#pragma once

#include "lupine/core/Core.hpp"
#include <memory>
#include <string>
#include <functional>
#include <nlohmann/json_fwd.hpp>

namespace lupine {

namespace core { class Node; }

namespace scripting {

class ScriptAPI;

/**
 * Scripting language types supported by the engine
 *
 * Note: Python (CPython via pybind11) is only available in the editor.
 * For export templates/runtime, use MicroPython, Lua, or MRuby which
 * are fully embeddable without external dependencies.
 */
enum class ScriptLanguage {
    Python,         // CPython via pybind11 - editor only
    MicroPython,    // Embedded MicroPython - runtime/templates
    Lua,            // Lua via sol2 - everywhere
    MRuby           // mRuby - everywhere
};

/**
 * Script execution result
 */
struct ScriptResult {
    bool success = false;
    std::string error;
    
    ScriptResult() = default;
    ScriptResult(bool s, const std::string& e = "") : success(s), error(e) {}
    
    operator bool() const { return success; }
};

/**
 * Base interface for script execution environments
 */
class IScriptEnvironment {
public:
    virtual ~IScriptEnvironment() = default;
    
    virtual ScriptLanguage GetLanguage() const = 0;
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;

    // Per-frame tick for the environment itself (drives the coroutine/await
    // scheduler). Called once per frame per owning ScriptComponent. Default is a
    // no-op for environments without a scheduler.
    virtual void Update(float deltaTime) { (void)deltaTime; }

    /**
     * Make every custom component class currently known to the CustomComponentRegistry
     * resolvable by name inside this environment's VM, so a script can inherit from a
     * custom component the same way it inherits from a built-in one
     * (`SimBoulder = SimObject:extend()`, `class SimBoulder(SimObject):`,
     * `class SimBoulder < SimObject`). The built-in base types are registered once at
     * host start-up, but custom classes are only discovered when the project is
     * scanned, and new ones appear as scripts are added or renamed - so this is called
     * before each custom component's script is executed. Idempotent and cheap to
     * repeat: a class already present in the VM is left alone.
     * Default implementation is a no-op for environments without class inheritance.
     */
    virtual void RegisterCustomComponentTypes() {}
    
    // Execute a script from file
    virtual ScriptResult ExecuteFile(const std::string& filepath) = 0;
    
    // Execute a script from string
    virtual ScriptResult ExecuteString(const std::string& script) = 0;
    
    // Call a function in the script
    virtual ScriptResult CallFunction(const std::string& functionName) = 0;
    
    // Check if a function exists
    virtual bool HasFunction(const std::string& functionName) const = 0;

    /**
     * Call a function as an archetype method: invokes functionName(self, args...)
     * where self is a table/dict built from selfData and args is a JSON array of
     * positional arguments. Returns the function's result as JSON (null if the
     * environment does not support method calls or the function is absent).
     * Default implementation is a no-op so non-supporting environments are safe.
     */
    virtual nlohmann::json CallMethod(const std::string& functionName,
                                      const nlohmann::json& selfData,
                                      const nlohmann::json& args);

    /**
     * Call a global function with positional arguments supplied as a JSON array.
     * Each argument is converted to the environment's native value type; a node
     * argument encoded as {"__lupine_node__": "<uuid>"} is delivered as a native
     * node handle. This is how signal connections invoke script-side handlers.
     * Returns true if the function existed and was invoked without error.
     * Default implementation is a no-op returning false.
     */
    virtual bool CallFunctionArgs(const std::string& functionName,
                                  const nlohmann::json& args);

    /**
     * Call a global function with positional arguments (as CallFunctionArgs does)
     * and return its result as JSON. Unlike CallMethod there is no implicit self
     * parameter, so a script's top-level function receives exactly its declared
     * arguments. This is the dispatch path behind ComponentRef::Call / NodeRef::Call
     * for script components, including autoload singletons.
     * Returns null when the function is absent or the call fails.
     * Default implementation is a no-op returning null.
     */
    virtual nlohmann::json CallFunctionResult(const std::string& functionName,
                                              const nlohmann::json& args);

    /**
     * Connect a ScriptAPI so the Lupine.* API is available to scripts in this
     * environment. Default no-op; concrete environments override this.
     */
    virtual void SetScriptAPI(ScriptAPI* api) { (void)api; }

    /**
     * Bind a singleton/autoload node to a global identifier in this environment so
     * scripts can reference it by name (Godot autoload style). The bound value is a
     * live node object; re-binding the same name replaces it. Default no-op for
     * environments that do not support node-typed globals.
     */
    virtual void SetGlobalNode(const std::string& name, core::Node* node) { (void)name; (void)node; }

    // Get/Set global variables
    virtual void SetGlobal(const std::string& name, int value) = 0;
    virtual void SetGlobal(const std::string& name, float value) = 0;
    virtual void SetGlobal(const std::string& name, const std::string& value) = 0;
    virtual void SetGlobal(const std::string& name, bool value) = 0;
    
    virtual int GetGlobalInt(const std::string& name, int defaultValue = 0) = 0;
    virtual float GetGlobalFloat(const std::string& name, float defaultValue = 0.0f) = 0;
    virtual std::string GetGlobalString(const std::string& name, const std::string& defaultValue = "") = 0;
    virtual bool GetGlobalBool(const std::string& name, bool defaultValue = false) = 0;

    /**
     * Set/get a global as an arbitrary JSON value. Used for export properties whose
     * type is richer than the scalar setters above (vectors, colors, quaternions,
     * rects, arrays, dictionaries, and inline structs). The JSON is converted to/from
     * the environment's native value type (objects -> tables/dicts, arrays -> lists).
     * Default implementations are a no-op / passthrough so environments that do not
     * support structured globals remain safe.
     */
    virtual void SetGlobalJson(const std::string& name, const nlohmann::json& value);
    virtual nlohmann::json GetGlobalJson(const std::string& name, const nlohmann::json& defaultValue);
};

/**
 * Initialize scripting subsystem
 */
void InitializeScripting();

/**
 * Shutdown scripting subsystem
 */
void ShutdownScripting();

} // namespace scripting
} // namespace lupine
