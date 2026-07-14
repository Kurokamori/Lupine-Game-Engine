#pragma once

#include "lupine/scripting/ScriptingCore.hpp"
#include "PropertyDescriptor.hpp"
#include "SignalObject.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace lupine {
namespace core {

/**
 * How an interface definition was authored.
 */
enum class InterfaceSource {
    DataFile,   // Authored as a .interface JSON schema
    Script,     // Declared via @interface directives inside a script file
    Native      // Registered from C++ at startup (built-in / native extension)
};

/**
 * A single method an interface requires its implementers to expose.
 *
 * Methods are matched by name (and, advisorily, by parameter arity) against the
 * functions a script declares or the CallMethod handlers a component exposes.
 * Parameter descriptors reuse SignalArgDesc so they round-trip through the same
 * reflection/editor infrastructure as signal arguments.
 */
struct InterfaceMethod {
    std::string name;
    std::vector<SignalArgDesc> params;
    bool hasReturn = false;
    PropertyValueType returnType = PropertyValueType::Float;
    std::string doc;
};

/**
 * Metadata describing an interface type (the equivalent of a C# interface or a
 * Godot "is this object a Damageable?" capability tag, but with a verifiable
 * contract). An interface is a named set of required methods plus required
 * signals. Scripts, archetypes, and native components declare that they
 * implement an interface; the engine indexes those declarations so the running
 * game (and the editor) can query "every Damageable in the scene" and connect /
 * emit across them through the existing signal system.
 *
 * Interfaces may extend other interfaces; an implementer of a derived interface
 * also satisfies every base interface in the chain.
 */
struct InterfaceDefinition {
    // The interface name declared by the author (e.g., "Damageable")
    std::string name;

    // Where the definition came from
    InterfaceSource source = InterfaceSource::DataFile;

    // The scripting language (only meaningful when source == Script)
    scripting::ScriptLanguage language = scripting::ScriptLanguage::Lua;

    // res:// path to the .interface file or the declaring script
    // (empty for Native interfaces)
    std::string sourcePath;

    // Interfaces this one extends (empty = none). The effective contract of a
    // derived interface is the union of its base interfaces' contracts
    // (recursively) plus its own. Cycle-safe.
    std::vector<std::string> baseInterfaces;

    // Optional human-readable description
    std::string description;

    // Methods implementers are required to expose
    std::vector<InterfaceMethod> methods;

    // Signals implementers are required to declare. Reuses the engine's
    // SignalDesc so the signal system, editor, and serialization all agree.
    std::vector<SignalDesc> signals;

    // Optional free-form tags for organisation / filtering in the editor
    std::vector<std::string> tags;

    // File modification time for hot-reload detection
    std::filesystem::file_time_type lastModified;

    // Whether parsing produced a usable definition
    bool isValid = false;

    // Error message populated when parsing fails
    std::string parseError;

    /**
     * Find a method descriptor by name, or nullptr if absent (own methods only).
     */
    const InterfaceMethod* FindMethod(const std::string& methodName) const;

    /**
     * Find a required-signal descriptor by name, or nullptr if absent (own only).
     */
    const SignalDesc* FindSignal(const std::string& signalName) const;

    /**
     * Serialize this definition to the .interface JSON schema format.
     */
    nlohmann::json Serialize() const;

    /**
     * Build a definition from a parsed .interface JSON schema.
     * @param json The parsed schema object
     * @param source The source classification to record
     * @param sourcePath The res:// path the schema was read from
     */
    static InterfaceDefinition Deserialize(const nlohmann::json& json,
                                           InterfaceSource source,
                                           const std::string& sourcePath);
};

} // namespace core
} // namespace lupine
