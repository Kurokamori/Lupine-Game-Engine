#pragma once

#include "lupine/asset/Asset.hpp"
#include "lupine/math/Math.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace lupine {
namespace asset {

/**
 * ArchetypeInstance
 *
 * A concrete data asset (.ares file) created from an archetype definition.
 * This is the Lupine equivalent of a Unity ScriptableObject instance or a Godot
 * Resource: pure data, not a scene node, referenced by path and read at runtime.
 *
 * Field values are reconciled against the current archetype definition when the
 * registry is available (missing fields fall back to the definition's defaults).
 * When no definition is available (e.g. running from a packed build), the stored
 * values are used directly.
 */
class ArchetypeInstance : public Asset {
public:
    ArchetypeInstance();
    explicit ArchetypeInstance(const core::UUID& uuid);
    ~ArchetypeInstance() override;

    AssetType GetType() const override { return AssetType::Archetype; }

    /**
     * Load an archetype instance from a .ares file.
     *
     * Equivalent to ReadFileContents + ParseFromString + FinalizeResolve. This is
     * the synchronous load path; for asynchronous loading the three steps are
     * split so the file read and JSON parse run on a worker thread while only the
     * registry-dependent resolution runs on the main thread (see AsyncAssetLoader).
     */
    bool LoadFromFile(const std::string& filepath);

    /**
     * Read the raw text of a .ares file (pack-file aware, res:// resolved).
     *
     * Thread-safe: touches only the filesystem / pack reader / AssetDatabase
     * (all internally synchronized), never the ArchetypeRegistry. Safe to call
     * from a background worker thread.
     */
    static bool ReadFileContents(const std::string& filepath, std::string& outContents);

    /**
     * Parse already-read .ares text into this instance's class/definition/fields.
     *
     * Does NOT resolve field defaults against the registry (call FinalizeResolve
     * for that). Thread-safe with respect to engine singletons: it only mutates
     * this instance. Returns false if the text is not valid archetype JSON.
     */
    bool ParseFromString(const std::string& filepath, const std::string& contents);

    /**
     * Reconcile parsed fields with the archetype definition's defaults and mark
     * the instance loaded. Reads the ArchetypeRegistry, so it MUST run on the
     * main thread (the registry is not synchronized for concurrent mutation).
     */
    void FinalizeResolve();

    const std::string& GetArchetypeClass() const { return m_ClassName; }
    const std::string& GetDefinitionPath() const { return m_DefinitionPath; }
    const std::string& GetDefinitionUUID() const { return m_DefinitionUUID; }

    /**
     * The inheritance chain of this instance's archetype: [class, base, ...].
     */
    std::vector<std::string> GetArchetypeChain() const;

    /**
     * Whether this instance's archetype is, or descends from, className.
     */
    bool IsArchetype(const std::string& className) const;

    /**
     * The interfaces this instance's archetype implements (queried through the
     * ArchetypeRegistry; unioned across the archetype's base chain).
     */
    std::vector<std::string> GetImplementedInterfaces() const;

    /**
     * Whether this instance's archetype implements an interface (archetype- and
     * interface-inheritance aware).
     */
    bool ImplementsInterface(const std::string& interfaceName) const;

    /**
     * The fully resolved field values (definition defaults merged with stored values).
     */
    const nlohmann::json& GetResolvedFields() const { return m_Resolved; }

    /**
     * The raw stored field values as found in the file.
     */
    const nlohmann::json& GetRawFields() const { return m_Fields; }

    bool HasField(const std::string& name) const;
    std::vector<std::string> GetFieldNames() const;
    nlohmann::json GetFieldJson(const std::string& name) const;

    int GetInt(const std::string& name, int fallback = 0) const;
    float GetFloat(const std::string& name, float fallback = 0.0f) const;
    double GetDouble(const std::string& name, double fallback = 0.0) const;
    bool GetBool(const std::string& name, bool fallback = false) const;
    std::string GetString(const std::string& name, const std::string& fallback = std::string()) const;
    math::Vec2 GetVec2(const std::string& name, const math::Vec2& fallback = math::Vec2()) const;
    math::Vec3 GetVec3(const std::string& name, const math::Vec3& fallback = math::Vec3()) const;
    math::Vec4 GetVec4(const std::string& name, const math::Vec4& fallback = math::Vec4()) const;
    math::Color GetColor(const std::string& name, const math::Color& fallback = math::Color()) const;
    math::Quat GetQuat(const std::string& name, const math::Quat& fallback = math::Quat()) const;
    math::Rect GetRect(const std::string& name, const math::Rect& fallback = math::Rect()) const;

    /**
     * A Resource field stores a res:// path to another asset (e.g. an .ares
     * instance). This returns that path as-is; resolve/load it via the asset
     * system or load_archetype.
     */
    std::string GetResource(const std::string& name, const std::string& fallback = std::string()) const;

    std::vector<std::string> GetStringArray(const std::string& name) const;
    std::vector<int> GetIntArray(const std::string& name) const;
    std::vector<float> GetFloatArray(const std::string& name) const;

    /**
     * A generic Array field, returned as the raw JSON array (empty array if the
     * field is absent or not an array). Heterogeneous element types are preserved.
     */
    nlohmann::json GetArray(const std::string& name) const;

    /**
     * A generic Dictionary field, returned as the raw JSON object (empty object
     * if the field is absent or not an object).
     */
    nlohmann::json GetDictionary(const std::string& name) const;

private:
    /**
     * Merge stored values with the definition's defaults into m_Resolved.
     */
    void Resolve();

    std::string m_ClassName;
    std::string m_DefinitionPath;
    std::string m_DefinitionUUID;
    nlohmann::json m_Fields = nlohmann::json::object();
    nlohmann::json m_Resolved = nlohmann::json::object();
};

} // namespace asset
} // namespace lupine
