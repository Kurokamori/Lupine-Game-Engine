#include "lupine/rendering/Material.hpp"

namespace lupine {

void MaterialPropertyBlock::setFloat(const std::string& name, float value) {
    m_properties[name] = value;
}

void MaterialPropertyBlock::setInt(const std::string& name, int value) {
    m_properties[name] = value;
}

void MaterialPropertyBlock::setBool(const std::string& name, bool value) {
    m_properties[name] = value;
}

void MaterialPropertyBlock::setVec2(const std::string& name, const Vec2& value) {
    m_properties[name] = value;
}

void MaterialPropertyBlock::setVec3(const std::string& name, const Vec3& value) {
    m_properties[name] = value;
}

void MaterialPropertyBlock::setVec4(const std::string& name, const Vec4& value) {
    m_properties[name] = value;
}

void MaterialPropertyBlock::setColor(const std::string& name, const Color& value) {
    m_properties[name] = value;
}

void MaterialPropertyBlock::setTexture(const std::string& name, TextureHandle value) {
    m_properties[name] = value;
}

void MaterialPropertyBlock::setMat4Array(const std::string& name, const Mat4* values, size_t count) {
    std::vector<Mat4> array(values, values + count);
    m_properties[name] = std::move(array);
}

bool MaterialPropertyBlock::hasProperty(const std::string& name) const {
    return m_properties.find(name) != m_properties.end();
}

const MaterialPropertyValue* MaterialPropertyBlock::getProperty(const std::string& name) const {
    auto it = m_properties.find(name);
    if (it != m_properties.end()) {
        return &it->second;
    }
    return nullptr;
}

void MaterialPropertyBlock::clear() {
    m_properties.clear();
}

}

