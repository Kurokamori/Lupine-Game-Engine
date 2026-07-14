#pragma once

#include "ScriptingCore.hpp"
#include "ScriptAPI.hpp"
#include <memory>
#include <string>
#include <vector>

#ifdef LUPINE_HAS_MICROPYTHON

// Forward declarations for MicroPython types
extern "C" {
    struct _mp_state_ctx_t;
    typedef void* mp_obj_t;  // MicroPython object handle
}

namespace lupine {
namespace scripting {

class MicroPythonEnvironment;

/**
 * Shared MicroPython host.
 *
 * MicroPython's embed port has a single global VM (mp_embed_init initialises one
 * process-wide interpreter), so there can only ever be one. This host owns that
 * VM and initialises it exactly once - previously every component re-ran
 * mp_embed_init on its own heap, silently re-initialising the shared VM and
 * clobbering earlier scripts' state.
 *
 * The lupine module, the inheritable component classes and the coroutine
 * scheduler are installed once into the VM's __main__ namespace (SharedGlobals).
 * Each script instance (MicroPythonEnvironment) then runs in its own module
 * globals dict seeded from __main__, so two scripts never share a function table.
 */
class MicroPythonHost {
public:
    static MicroPythonHost& Instance();

    // Initialise the one VM if needed. heapSize applies only to the first call.
    bool EnsureInitialized(size_t heapSize);
    bool IsInitialized() const { return m_Initialized; }

    // Drive the process-global coroutine/await scheduler once per frame.
    void Pump(float deltaTime);

    // The VM's __main__ globals dict: holds lupine, the component classes, the
    // scheduler, and seeds every instance namespace.
    mp_obj_t SharedGlobals() const { return m_SharedGlobals; }

    void SetHeapSize(size_t bytes) { if (!m_Initialized) m_HeapSize = bytes; }
    size_t GetHeapSize() const { return m_HeapSize; }

    // A process-lifetime ScriptAPI used to wrap singleton nodes bound into the
    // shared __main__ (which seeds future instances), so those handles never carry
    // a component API that could be freed. Refreshed from `treeSource` on use.
    ScriptAPI* SharedNodeApi(ScriptAPI* treeSource);

    // Instance-module ids. A script instance's namespace is a module named
    // __lup_inst_<id>; its name is interned as a qstr, and MicroPython's qstr pool
    // is append-only, so ids (and therefore names) are recycled through a free list
    // rather than handed out monotonically - otherwise every spawn/despawn cycle
    // would permanently intern another string into the fixed heap.
    int AcquireModuleId();
    void ReleaseModuleId(int id);

    // Define every custom component class the CustomComponentRegistry knows about into
    // the shared __main__ namespace, so `class SimBoulder(SimObject):` resolves a
    // custom base just like a built-in one. Each class is created under its actual
    // base (recursively, so a chain is defined base-first), which makes Python's own
    // isinstance()/__mro__ agree with the engine's type chain. Idempotent: a name
    // already in __main__ is left alone. The built-in classes are defined once at
    // start-up, but custom ones only exist after a project scan, so this runs again
    // before each custom component's script loads.
    void RegisterCustomComponentTypes();

private:
    MicroPythonHost() = default;

    bool m_Initialized = false;
    bool m_ModuleReady = false;      // lupine module + component classes registered
    std::vector<char> m_Heap;
    size_t m_HeapSize = 256 * 1024;  // 256KB default
    char m_StackTop[64];             // persistent stack marker for MicroPython GC
    mp_obj_t m_SharedGlobals = nullptr;
    ScriptAPI m_SharedNodeApi;       // stable API for shared-VM node handles
    int m_NextModuleId = 0;
    std::vector<int> m_FreeModuleIds;
};

/**
 * MicroPython script execution environment.
 *
 * A thin per-component handle into the shared host: it owns its own module
 * globals dict (the instance namespace). The script's top-level defs and bare
 * variables live there, isolated from other scripts, while lupine/the component
 * classes (and any bound singletons) are seeded in so the API resolves.
 */
class MicroPythonEnvironment : public IScriptEnvironment {
public:
    MicroPythonEnvironment();
    ~MicroPythonEnvironment() override;

    ScriptLanguage GetLanguage() const override { return ScriptLanguage::MicroPython; }

    bool Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    void RegisterCustomComponentTypes() override;

    ScriptResult ExecuteFile(const std::string& filepath) override;
    ScriptResult ExecuteString(const std::string& script) override;
    ScriptResult CallFunction(const std::string& functionName) override;

    nlohmann::json CallMethod(const std::string& functionName,
                              const nlohmann::json& selfData,
                              const nlohmann::json& args) override;

    bool CallFunctionArgs(const std::string& functionName,
                          const nlohmann::json& args) override;

    nlohmann::json CallFunctionResult(const std::string& functionName,
                                      const nlohmann::json& args) override;

    bool HasFunction(const std::string& functionName) const override;

    void SetGlobal(const std::string& name, int value) override;
    void SetGlobal(const std::string& name, float value) override;
    void SetGlobal(const std::string& name, const std::string& value) override;
    void SetGlobal(const std::string& name, bool value) override;
    void SetGlobalNode(const std::string& name, core::Node* node) override;

    int GetGlobalInt(const std::string& name, int defaultValue = 0) override;
    float GetGlobalFloat(const std::string& name, float defaultValue = 0.0f) override;
    std::string GetGlobalString(const std::string& name, const std::string& defaultValue = "") override;
    bool GetGlobalBool(const std::string& name, bool defaultValue = false) override;
    void SetGlobalJson(const std::string& name, const nlohmann::json& value) override;
    nlohmann::json GetGlobalJson(const std::string& name, const nlohmann::json& defaultValue) override;

    // MicroPython-specific: Get/set heap size (must be called before Initialize)
    void SetHeapSize(size_t bytes);
    size_t GetHeapSize() const { return MicroPythonHost::Instance().GetHeapSize(); }

    // MicroPython-specific: Run garbage collection
    void RunGC();

    // The instance's module globals dict (its sandboxed namespace).
    mp_obj_t GetGlobalsDict() const { return m_Globals; }

    // Identity of this instance in the host's live-instance registry. The coroutine
    // scheduler tags each entry with this id (never the pointer) so a coroutine of a
    // destroyed instance is retired instead of resumed. 0 while not initialised.
    int GetEnvId() const { return m_EnvId; }

    // Set the script API context
    void SetScriptAPI(ScriptAPI* api) override;
    ScriptAPI* GetScriptAPI() const { return m_ScriptAPI; }

    // Get last error message
    const std::string& GetLastError() const { return m_LastError; }

private:
    bool m_Initialized = false;
    ScriptAPI* m_ScriptAPI = nullptr;
    std::string m_LastError;
    mp_obj_t m_Globals = nullptr;  // this instance's module globals dict
    std::string m_ModuleName;      // __lup_inst_<id>, unregistered on Shutdown
    int m_ModuleId = -1;           // recycled through MicroPythonHost's free list
    int m_EnvId = 0;               // live-instance registry id (0 = unregistered)

    // Create the instance globals dict, seeding it from the shared __main__.
    bool EnsureGlobals();

    ScriptResult HandleError(mp_obj_t exc);
};

} // namespace scripting
} // namespace lupine

#endif // LUPINE_HAS_MICROPYTHON
