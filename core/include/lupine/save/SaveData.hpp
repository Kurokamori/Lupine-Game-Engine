#pragma once

#include "lupine/math/Math.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace lupine {
namespace save {

/**
 * SaveData - the flexible document model for a single save payload.
 *
 * A SaveData wraps a JSON object and exposes typed, default-aware accessors so
 * games can store arbitrary structured state without writing serialization code.
 * Keys address nested objects with '/' as a separator: SetInt("player/stats/hp",
 * 100) creates the intermediate objects automatically, and GetInt("player/stats/hp")
 * walks them back. Missing keys (or type mismatches) return the supplied default,
 * never throw.
 *
 * SaveData is a value type (copyable). It is the unit passed to and returned from
 * SaveGameManager; games may also nest SaveData sub-documents via GetSection /
 * SetSection. The backing JSON is always an object, so it round-trips cleanly
 * through every SaveFormat.
 */
class SaveData {
public:
    SaveData();
    explicit SaveData(nlohmann::json root);

    // Direct access to the backing JSON (always an object).
    nlohmann::json& Root();
    const nlohmann::json& Root() const;
    const nlohmann::json& ToJson() const { return m_Root; }
    static SaveData FromJson(const nlohmann::json& json);

    void Clear();
    bool Empty() const;

    // Structure / presence. Keys may be nested ("a/b/c").
    bool Has(const std::string& key) const;
    void Erase(const std::string& key);
    std::vector<std::string> Keys() const;                          // top-level keys
    std::vector<std::string> KeysAt(const std::string& key) const;  // keys of a nested object

    // Typed getters. Return `def` when the key is absent or not convertible.
    bool GetBool(const std::string& key, bool def = false) const;
    int GetInt(const std::string& key, int def = 0) const;
    int64_t GetInt64(const std::string& key, int64_t def = 0) const;
    float GetFloat(const std::string& key, float def = 0.0f) const;
    double GetDouble(const std::string& key, double def = 0.0) const;
    std::string GetString(const std::string& key, const std::string& def = std::string()) const;
    math::Vec2 GetVec2(const std::string& key, const math::Vec2& def = math::Vec2()) const;
    math::Vec3 GetVec3(const std::string& key, const math::Vec3& def = math::Vec3()) const;
    math::Vec4 GetVec4(const std::string& key, const math::Vec4& def = math::Vec4()) const;
    math::Color GetColor(const std::string& key, const math::Color& def = math::Color()) const;
    nlohmann::json GetJson(const std::string& key, const nlohmann::json& def = nlohmann::json()) const;

    // Typed setters. Intermediate objects are created as needed.
    void SetBool(const std::string& key, bool value);
    void SetInt(const std::string& key, int value);
    void SetInt64(const std::string& key, int64_t value);
    void SetFloat(const std::string& key, float value);
    void SetDouble(const std::string& key, double value);
    void SetString(const std::string& key, const std::string& value);
    void SetVec2(const std::string& key, const math::Vec2& value);
    void SetVec3(const std::string& key, const math::Vec3& value);
    void SetVec4(const std::string& key, const math::Vec4& value);
    void SetColor(const std::string& key, const math::Color& value);
    void SetJson(const std::string& key, const nlohmann::json& value);

    // Array helpers.
    nlohmann::json GetArray(const std::string& key) const;             // empty array if absent
    void SetArray(const std::string& key, const nlohmann::json& array);
    void Append(const std::string& key, const nlohmann::json& value);  // push onto array (creates)
    size_t ArraySize(const std::string& key) const;

    // Nested sub-documents.
    bool HasSection(const std::string& key) const;        // present and is an object
    SaveData GetSection(const std::string& key) const;    // copy; empty SaveData if absent
    void SetSection(const std::string& key, const SaveData& section);

    // Merge another document's top-level keys into this one (overwrite on collision
    // when `overwrite` is true, otherwise keep existing values).
    void Merge(const SaveData& other, bool overwrite = true);

    // Interchange helpers.
    std::string ToString(bool pretty = true) const;
    static SaveData Parse(const std::string& jsonText, bool* ok = nullptr);

private:
    nlohmann::json m_Root;  // always an object
};

} // namespace save
} // namespace lupine
