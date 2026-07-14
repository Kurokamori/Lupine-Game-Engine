#pragma once

#include "lupine/lupine_extension_interface.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace lupine {
namespace core {

/**
 * Host-side copy of a component class registered by a native extension.
 *
 * The engine deep-copies everything it needs out of the plugin-provided
 * LupineComponentClassDesc so the descriptor's lifetime is decoupled from the
 * plugin's. The vtable is copied by value; its function pointers remain valid as
 * long as the owning shared library stays loaded (which it does for the whole
 * session — see ExtensionManager).
 */
struct NativeComponentClass {
    std::string className;
    std::string baseClassName;     // empty => lupine::core::Component
    std::string displayCategory;   // editor subcategory hint, may be empty
    uint32_t hookFlags = 0;        // bitmask of LupineHookFlags
    LupineComponentVTable vtable{}; // plugin's per-instance function table

    bool HasHook(LupineHookFlags flag) const {
        return (hookFlags & static_cast<uint32_t>(flag)) != 0;
    }
};

/**
 * Registry of native (C++ DLL) component classes.
 *
 * This is the native-extension analogue of CustomComponentRegistry (which serves
 * scripted components). RegisterClass copies the descriptor and installs a
 * TypeRegistry factory that produces a NativeComponentWrapper bound to the class,
 * exactly mirroring CustomComponentRegistry::RegisterWithTypeRegistry.
 */
class NativeExtensionRegistry {
public:
    static NativeExtensionRegistry& GetInstance();

    /**
     * Register a component class from a plugin descriptor.
     * Validates the descriptor, copies it, and registers a TypeRegistry factory.
     * @return true on success; false on invalid descriptor or duplicate type name.
     *
     * Note: not named RegisterClass to avoid the Win32 <windows.h> macro of that name.
     */
    bool RegisterComponentClass(const LupineComponentClassDesc* desc);

    /** Look up a registered class by name, or nullptr. */
    const NativeComponentClass* GetClass(const std::string& className) const;

    /** True if a native class with this name is registered. */
    bool IsNativeClass(const std::string& className) const;

    /** All registered native classes (for editor enumeration). */
    std::vector<const NativeComponentClass*> GetAllClasses() const;

private:
    NativeExtensionRegistry() = default;
    ~NativeExtensionRegistry() = default;
    NativeExtensionRegistry(const NativeExtensionRegistry&) = delete;
    NativeExtensionRegistry& operator=(const NativeExtensionRegistry&) = delete;

    std::unordered_map<std::string, std::unique_ptr<NativeComponentClass>> m_Classes;
};

/**
 * Loads, tracks and unloads native extension shared libraries for a project.
 *
 * Discovery: LoadExtensionsForProject scans the project directory for *.lupineext
 * manifest files, resolves the per-platform binary path from each, loads the
 * library (LoadLibrary / dlopen), resolves lupine_extension_init, checks the ABI
 * version, and calls it with the host interface so the plugin can register its
 * component classes into the engine TypeRegistry.
 *
 * This object lives in lupine_core, so every host that statically links the core
 * (runtime executable, console, and the editor's embedded engine module) gets
 * extension loading for free against its own engine singletons.
 */
class ExtensionManager {
public:
    /** Outcome of loading a single extension binary. */
    struct LoadedExtension {
        std::string manifestPath;
        std::string binaryPath;
        std::string name;
        void* libraryHandle = nullptr;          // HMODULE / void* from dlopen
        LupineExtensionHandle pluginState = nullptr;
        LupineExtensionDeinitFn deinitFn = nullptr;
        std::vector<std::string> registeredClasses;
        bool ok = false;
        std::string error;
    };

    static ExtensionManager& GetInstance();

    /**
     * Scan a project directory for *.lupineext manifests and load every
     * extension found. Safe to call once per project open. Idempotent per
     * binary path: an already-loaded binary is skipped.
     * @param projectDir absolute path to the project root directory.
     * @return number of extensions successfully loaded.
     */
    int LoadExtensionsForProject(const std::string& projectDir);

    /**
     * Load a single extension binary by absolute path (bypassing manifests).
     * @return a LoadedExtension record (check .ok).
     */
    const LoadedExtension& LoadExtensionBinary(const std::string& binaryPath,
                                               const std::string& manifestPath = std::string(),
                                               const std::string& name = std::string());

    /** Unload all libraries (calls each deinit, then frees the module). */
    void UnloadAll();

    const std::vector<LoadedExtension>& GetLoadedExtensions() const { return m_Loaded; }

    /** The shared host interface table handed to every plugin. */
    static const LupineHostInterface* HostInterface();

private:
    ExtensionManager() = default;
    ~ExtensionManager();
    ExtensionManager(const ExtensionManager&) = delete;
    ExtensionManager& operator=(const ExtensionManager&) = delete;

    bool IsBinaryLoaded(const std::string& binaryPath) const;

    std::vector<LoadedExtension> m_Loaded;
};

} // namespace core
} // namespace lupine
