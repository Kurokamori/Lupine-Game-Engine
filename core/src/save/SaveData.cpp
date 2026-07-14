#include "lupine/save/SaveData.hpp"

#include <sstream>

namespace lupine {
namespace save {

namespace {

// Split a '/'-separated key path into its non-empty segments.
std::vector<std::string> SplitPath(const std::string& key) {
    std::vector<std::string> segments;
    std::string current;
    for (char c : key) {
        if (c == '/') {
            if (!current.empty()) {
                segments.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        segments.push_back(current);
    }
    return segments;
}

// Resolve a key path to a const pointer to the addressed value, or nullptr.
const nlohmann::json* ResolveConst(const nlohmann::json& root, const std::string& key) {
    std::vector<std::string> segments = SplitPath(key);
    if (segments.empty()) {
        return nullptr;
    }
    const nlohmann::json* node = &root;
    for (const std::string& segment : segments) {
        if (!node->is_object()) {
            return nullptr;
        }
        auto it = node->find(segment);
        if (it == node->end()) {
            return nullptr;
        }
        node = &(*it);
    }
    return node;
}

// Resolve a key path to a mutable value, creating intermediate objects as needed.
// Returns nullptr only when the path is empty.
nlohmann::json* ResolveOrCreate(nlohmann::json& root, const std::string& key) {
    std::vector<std::string> segments = SplitPath(key);
    if (segments.empty()) {
        return nullptr;
    }
    nlohmann::json* node = &root;
    for (const std::string& segment : segments) {
        if (!node->is_object()) {
            *node = nlohmann::json::object();
        }
        node = &((*node)[segment]);
    }
    return node;
}

float JsonToFloat(const nlohmann::json& json, float def) {
    if (json.is_number()) {
        return json.get<float>();
    }
    return def;
}

} // namespace

SaveData::SaveData()
    : m_Root(nlohmann::json::object()) {}

SaveData::SaveData(nlohmann::json root)
    : m_Root(std::move(root)) {
    if (!m_Root.is_object()) {
        m_Root = nlohmann::json::object();
    }
}

nlohmann::json& SaveData::Root() {
    return m_Root;
}

const nlohmann::json& SaveData::Root() const {
    return m_Root;
}

SaveData SaveData::FromJson(const nlohmann::json& json) {
    return SaveData(json);
}

void SaveData::Clear() {
    m_Root = nlohmann::json::object();
}

bool SaveData::Empty() const {
    return m_Root.empty();
}

bool SaveData::Has(const std::string& key) const {
    return ResolveConst(m_Root, key) != nullptr;
}

void SaveData::Erase(const std::string& key) {
    std::vector<std::string> segments = SplitPath(key);
    if (segments.empty()) {
        return;
    }
    nlohmann::json* node = &m_Root;
    for (size_t i = 0; i + 1 < segments.size(); ++i) {
        if (!node->is_object()) {
            return;
        }
        auto it = node->find(segments[i]);
        if (it == node->end()) {
            return;
        }
        node = &(*it);
    }
    if (node->is_object()) {
        node->erase(segments.back());
    }
}

std::vector<std::string> SaveData::Keys() const {
    std::vector<std::string> keys;
    for (auto it = m_Root.begin(); it != m_Root.end(); ++it) {
        keys.push_back(it.key());
    }
    return keys;
}

std::vector<std::string> SaveData::KeysAt(const std::string& key) const {
    std::vector<std::string> keys;
    const nlohmann::json* node = ResolveConst(m_Root, key);
    if (node && node->is_object()) {
        for (auto it = node->begin(); it != node->end(); ++it) {
            keys.push_back(it.key());
        }
    }
    return keys;
}

bool SaveData::GetBool(const std::string& key, bool def) const {
    const nlohmann::json* node = ResolveConst(m_Root, key);
    if (node) {
        if (node->is_boolean()) {
            return node->get<bool>();
        }
        if (node->is_number()) {
            return node->get<double>() != 0.0;
        }
    }
    return def;
}

int SaveData::GetInt(const std::string& key, int def) const {
    const nlohmann::json* node = ResolveConst(m_Root, key);
    if (node && node->is_number()) {
        return node->get<int>();
    }
    return def;
}

int64_t SaveData::GetInt64(const std::string& key, int64_t def) const {
    const nlohmann::json* node = ResolveConst(m_Root, key);
    if (node && node->is_number()) {
        return node->get<int64_t>();
    }
    return def;
}

float SaveData::GetFloat(const std::string& key, float def) const {
    const nlohmann::json* node = ResolveConst(m_Root, key);
    if (node && node->is_number()) {
        return node->get<float>();
    }
    return def;
}

double SaveData::GetDouble(const std::string& key, double def) const {
    const nlohmann::json* node = ResolveConst(m_Root, key);
    if (node && node->is_number()) {
        return node->get<double>();
    }
    return def;
}

std::string SaveData::GetString(const std::string& key, const std::string& def) const {
    const nlohmann::json* node = ResolveConst(m_Root, key);
    if (node && node->is_string()) {
        return node->get<std::string>();
    }
    return def;
}

math::Vec2 SaveData::GetVec2(const std::string& key, const math::Vec2& def) const {
    const nlohmann::json* node = ResolveConst(m_Root, key);
    if (node && node->is_array() && node->size() >= 2) {
        return math::Vec2(JsonToFloat((*node)[0], def.x), JsonToFloat((*node)[1], def.y));
    }
    return def;
}

math::Vec3 SaveData::GetVec3(const std::string& key, const math::Vec3& def) const {
    const nlohmann::json* node = ResolveConst(m_Root, key);
    if (node && node->is_array() && node->size() >= 3) {
        return math::Vec3(JsonToFloat((*node)[0], def.x),
                          JsonToFloat((*node)[1], def.y),
                          JsonToFloat((*node)[2], def.z));
    }
    return def;
}

math::Vec4 SaveData::GetVec4(const std::string& key, const math::Vec4& def) const {
    const nlohmann::json* node = ResolveConst(m_Root, key);
    if (node && node->is_array() && node->size() >= 4) {
        return math::Vec4(JsonToFloat((*node)[0], def.x),
                          JsonToFloat((*node)[1], def.y),
                          JsonToFloat((*node)[2], def.z),
                          JsonToFloat((*node)[3], def.w));
    }
    return def;
}

math::Color SaveData::GetColor(const std::string& key, const math::Color& def) const {
    const nlohmann::json* node = ResolveConst(m_Root, key);
    if (node && node->is_array() && node->size() >= 4) {
        return math::Color(JsonToFloat((*node)[0], def.r),
                           JsonToFloat((*node)[1], def.g),
                           JsonToFloat((*node)[2], def.b),
                           JsonToFloat((*node)[3], def.a));
    }
    return def;
}

nlohmann::json SaveData::GetJson(const std::string& key, const nlohmann::json& def) const {
    const nlohmann::json* node = ResolveConst(m_Root, key);
    if (node) {
        return *node;
    }
    return def;
}

void SaveData::SetBool(const std::string& key, bool value) {
    nlohmann::json* node = ResolveOrCreate(m_Root, key);
    if (node) {
        *node = value;
    }
}

void SaveData::SetInt(const std::string& key, int value) {
    nlohmann::json* node = ResolveOrCreate(m_Root, key);
    if (node) {
        *node = value;
    }
}

void SaveData::SetInt64(const std::string& key, int64_t value) {
    nlohmann::json* node = ResolveOrCreate(m_Root, key);
    if (node) {
        *node = value;
    }
}

void SaveData::SetFloat(const std::string& key, float value) {
    nlohmann::json* node = ResolveOrCreate(m_Root, key);
    if (node) {
        *node = value;
    }
}

void SaveData::SetDouble(const std::string& key, double value) {
    nlohmann::json* node = ResolveOrCreate(m_Root, key);
    if (node) {
        *node = value;
    }
}

void SaveData::SetString(const std::string& key, const std::string& value) {
    nlohmann::json* node = ResolveOrCreate(m_Root, key);
    if (node) {
        *node = value;
    }
}

void SaveData::SetVec2(const std::string& key, const math::Vec2& value) {
    nlohmann::json* node = ResolveOrCreate(m_Root, key);
    if (node) {
        *node = nlohmann::json::array({value.x, value.y});
    }
}

void SaveData::SetVec3(const std::string& key, const math::Vec3& value) {
    nlohmann::json* node = ResolveOrCreate(m_Root, key);
    if (node) {
        *node = nlohmann::json::array({value.x, value.y, value.z});
    }
}

void SaveData::SetVec4(const std::string& key, const math::Vec4& value) {
    nlohmann::json* node = ResolveOrCreate(m_Root, key);
    if (node) {
        *node = nlohmann::json::array({value.x, value.y, value.z, value.w});
    }
}

void SaveData::SetColor(const std::string& key, const math::Color& value) {
    nlohmann::json* node = ResolveOrCreate(m_Root, key);
    if (node) {
        *node = nlohmann::json::array({value.r, value.g, value.b, value.a});
    }
}

void SaveData::SetJson(const std::string& key, const nlohmann::json& value) {
    nlohmann::json* node = ResolveOrCreate(m_Root, key);
    if (node) {
        *node = value;
    }
}

nlohmann::json SaveData::GetArray(const std::string& key) const {
    const nlohmann::json* node = ResolveConst(m_Root, key);
    if (node && node->is_array()) {
        return *node;
    }
    return nlohmann::json::array();
}

void SaveData::SetArray(const std::string& key, const nlohmann::json& array) {
    nlohmann::json* node = ResolveOrCreate(m_Root, key);
    if (node) {
        *node = array.is_array() ? array : nlohmann::json::array();
    }
}

void SaveData::Append(const std::string& key, const nlohmann::json& value) {
    nlohmann::json* node = ResolveOrCreate(m_Root, key);
    if (!node) {
        return;
    }
    if (!node->is_array()) {
        *node = nlohmann::json::array();
    }
    node->push_back(value);
}

size_t SaveData::ArraySize(const std::string& key) const {
    const nlohmann::json* node = ResolveConst(m_Root, key);
    if (node && node->is_array()) {
        return node->size();
    }
    return 0;
}

bool SaveData::HasSection(const std::string& key) const {
    const nlohmann::json* node = ResolveConst(m_Root, key);
    return node != nullptr && node->is_object();
}

SaveData SaveData::GetSection(const std::string& key) const {
    const nlohmann::json* node = ResolveConst(m_Root, key);
    if (node && node->is_object()) {
        return SaveData(*node);
    }
    return SaveData();
}

void SaveData::SetSection(const std::string& key, const SaveData& section) {
    nlohmann::json* node = ResolveOrCreate(m_Root, key);
    if (node) {
        *node = section.ToJson();
    }
}

void SaveData::Merge(const SaveData& other, bool overwrite) {
    for (auto it = other.m_Root.begin(); it != other.m_Root.end(); ++it) {
        if (overwrite || m_Root.find(it.key()) == m_Root.end()) {
            m_Root[it.key()] = it.value();
        }
    }
}

std::string SaveData::ToString(bool pretty) const {
    return m_Root.dump(pretty ? 2 : -1);
}

SaveData SaveData::Parse(const std::string& jsonText, bool* ok) {
    try {
        nlohmann::json json = nlohmann::json::parse(jsonText);
        if (ok) {
            *ok = true;
        }
        return SaveData(json);
    } catch (const std::exception&) {
        if (ok) {
            *ok = false;
        }
        return SaveData();
    }
}

} // namespace save
} // namespace lupine
