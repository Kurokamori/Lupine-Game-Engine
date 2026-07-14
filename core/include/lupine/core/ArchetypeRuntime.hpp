#pragma once

#include "lupine/scripting/ScriptingCore.hpp"
#include "lupine/scripting/ScriptAPI.hpp"
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace lupine {
namespace core {

/**
 * Executes methods declared in script-defined archetypes.
 *
 * Script-defined archetypes (e.g., a .lua file with @archetype_class) may declare
 * functions in addition to @export fields. ArchetypeRuntime keeps one cached
 * script environment per archetype class, loads the script once, and invokes a
 * named function as `method(self, args...)` where self is the instance's field
 * data. Arguments and the return value are exchanged as JSON, so a method written
 * in one language can be invoked from any scripting language.
 *
 * Data-defined (.archetype) archetypes have no script and therefore no methods.
 */
class ArchetypeRuntime {
public:
    static ArchetypeRuntime& GetInstance();

    /**
     * Call a method on an archetype instance.
     * @param instancePath res:// (or filesystem) path to the .ares instance
     * @param methodName The function name declared in the archetype's script
     * @param args JSON array of positional arguments
     * @return The method's result as JSON, or null on failure / no such method
     */
    nlohmann::json CallMethod(const std::string& instancePath,
                              const std::string& methodName,
                              const nlohmann::json& args);

    /**
     * Whether the archetype class has a callable script method of the given name.
     */
    bool HasMethod(const std::string& className, const std::string& methodName);

    /**
     * Drop all cached script environments (call on project reload / hot-reload).
     */
    void Clear();

private:
    ArchetypeRuntime() = default;
    ~ArchetypeRuntime() = default;
    ArchetypeRuntime(const ArchetypeRuntime&) = delete;
    ArchetypeRuntime& operator=(const ArchetypeRuntime&) = delete;

    struct ClassEnvironment {
        std::unique_ptr<scripting::IScriptEnvironment> env;
        std::unique_ptr<scripting::ScriptAPI> api;
        bool loaded = false;
    };

    std::unordered_map<std::string, ClassEnvironment> m_Environments;
    std::mutex m_Mutex;

    /**
     * Get a loaded script environment for a class (creating + loading on first
     * use). Returns nullptr if the class is unknown or not script-defined.
     * Caller must hold m_Mutex.
     */
    scripting::IScriptEnvironment* EnsureEnvironment(const std::string& className);
};

} // namespace core
} // namespace lupine
