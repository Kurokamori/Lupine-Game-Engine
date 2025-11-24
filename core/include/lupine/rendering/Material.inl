#pragma once

#include "Material.hpp"

namespace lupine {

// MaterialPropertyBlock inline implementations

inline void MaterialPropertyBlock::setFloat(const std::string& name, float value) {
    m_properties[name] = value;
}

inline void MaterialPropertyBlock::setInt(const std::string& name, int value) {
    m_properties[name] = value;
}

inline void MaterialPropertyBlock::setBool(const std::string& name, bool value) {
    m_properties[name] = value;
}

inline void MaterialPropertyBlock::setVec2(const std::string& name, const Vec2& value) {
    m_properties[name] = value;
}

inline void MaterialPropertyBlock::setVec3(const std::string& name, const Vec3& value) {
    m_properties[name] = value;
}

inline void MaterialPropertyBlock::setVec4(const std::string& name, const Vec4& value) {
    m_properties[name] = value;
}

inline void MaterialPropertyBlock::setColor(const std::string& name, const Color& value) {
    m_properties[name] = value;
}

inline void MaterialPropertyBlock::setTexture(const std::string& name, TextureHandle value) {
    m_properties[name] = value;
}

inline bool MaterialPropertyBlock::hasProperty(const std::string& name) const {
    return m_properties.find(name) != m_properties.end();
}

inline const MaterialPropertyValue* MaterialPropertyBlock::getProperty(const std::string& name) const {
    auto it = m_properties.find(name);
    if (it != m_properties.end()) {
        return &it->second;
    }
    return nullptr;
}

inline void MaterialPropertyBlock::clear() {
    m_properties.clear();
}

} // namespace lupine
