#pragma once

#include "lupine/scripting/ScriptingCore.hpp"
#include "PropertyDescriptor.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace lupine {
namespace core {

/**
 * How an archetype definition was authored.
 */
enum class ArchetypeSource {
    DataFile,   // Authored as a .archetype JSON schema
    Script      // Declared via @archetype_class directives inside a script file
};

/**
 * Metadata describing a user-defined archetype type (the equivalent of a Unity
 * ScriptableObject class or a Godot custom Resource). An archetype is a named set
 * of typed fields plus a Create-menu location. Concrete data is stored separately
 * in archetype instance assets (.ares files).
 *
 * Each field reuses the engine's PropertyDescriptor so it round-trips through the
 * same serialization and inspector infrastructure as component properties.
 */
struct ArchetypeDefinition {
    // The archetype type name declared by the author (e.g., "EnemyStats")
    std::string className;

    // Where the definition came from
    ArchetypeSource source = ArchetypeSource::DataFile;

    // The scripting language (only meaningful when source == Script)
    scripting::ScriptLanguage language = scripting::ScriptLanguage::Lua;

    // res:// path to the .archetype file or the declaring script
    std::string sourcePath;

    // Class name of the archetype this one extends (empty = no base).
    // The effective field set of a derived archetype is its base's fields
    // (recursively) followed by its own; same-named fields override the base.
    std::string baseClass;

    // When true this archetype is a base/abstract type and is hidden from the
    // "create instance" menu (it can still be extended and edited).
    bool isAbstract = false;

    // Slash-separated location in the right-click Create menu (e.g., "Gameplay/Enemies")
    std::string menuPath = "Archetypes";

    // Optional res:// path to an icon shown in the editor
    std::string iconPath;

    // Optional human-readable description
    std::string description;

    // The typed fields that make up this archetype
    std::vector<PropertyDescriptor> fields;

    // Names of the interfaces this archetype declares it implements (see
    // InterfaceRegistry). A derived archetype implements the union of its own and
    // its base chain's declarations; query through ArchetypeRegistry.
    std::vector<std::string> implementsInterfaces;

    // File modification time for hot-reload detection
    std::filesystem::file_time_type lastModified;

    // Whether parsing produced a usable definition
    bool isValid = false;

    // Error message populated when parsing fails
    std::string parseError;

    /**
     * Find a field descriptor by name, or nullptr if absent.
     */
    const PropertyDescriptor* FindField(const std::string& name) const;

    /**
     * Serialize this definition to the .archetype JSON schema format.
     */
    nlohmann::json Serialize() const;

    /**
     * Build a definition from a parsed .archetype JSON schema.
     * @param json The parsed schema object
     * @param source The source classification to record
     * @param sourcePath The res:// path the schema was read from
     */
    static ArchetypeDefinition Deserialize(const nlohmann::json& json,
                                           ArchetypeSource source,
                                           const std::string& sourcePath);

    /**
     * Build a JSON object mapping each field name to its default value
     * (encoded the same way component property values are). Used when stamping
     * out a new instance.
     */
    nlohmann::json BuildDefaultValues() const;

    /**
     * Build a JSON object mapping each field name to its PropertyDescriptor
     * metadata, matching the property_metadata block the inspector consumes.
     */
    nlohmann::json BuildPropertyMetadata() const;
};

} // namespace core
} // namespace lupine
