#include "lupine/core/Serialization.hpp"
#include "lupine/core/PropertyDescriptor.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/math/Math.hpp"
#include <fstream>

namespace lupine {
namespace core {

static PropertyValueType ConvertPropertyTypeToValueType(PropertyType type) {
    switch (type) {
        case PropertyType::Bool: return PropertyValueType::Bool;
        case PropertyType::Int: return PropertyValueType::Int;
        case PropertyType::Float: return PropertyValueType::Float;
        case PropertyType::Double: return PropertyValueType::Float;
        case PropertyType::String: return PropertyValueType::String;
        case PropertyType::Vec2: return PropertyValueType::Vec2;
        case PropertyType::Vec3: return PropertyValueType::Vec3;
        case PropertyType::Vec4: return PropertyValueType::Vec4;
        case PropertyType::Color: return PropertyValueType::Color;
        default: return PropertyValueType::String;
    }
}

template<>
nlohmann::json TypedProperty<bool>::Serialize() const {
    return m_Getter();
}

template<>
void TypedProperty<bool>::Deserialize(const nlohmann::json& json) {
    if (json.is_boolean()) {
        m_Setter(json.get<bool>());
    }
}

template<>
nlohmann::json TypedProperty<int>::Serialize() const {
    return m_Getter();
}

template<>
void TypedProperty<int>::Deserialize(const nlohmann::json& json) {
    if (json.is_number_integer()) {
        m_Setter(json.get<int>());
    }
}

template<>
nlohmann::json TypedProperty<float>::Serialize() const {
    return m_Getter();
}

template<>
void TypedProperty<float>::Deserialize(const nlohmann::json& json) {
    if (json.is_number()) {
        m_Setter(json.get<float>());
    }
}

template<>
nlohmann::json TypedProperty<double>::Serialize() const {
    return m_Getter();
}

template<>
void TypedProperty<double>::Deserialize(const nlohmann::json& json) {
    if (json.is_number()) {
        m_Setter(json.get<double>());
    }
}

template<>
nlohmann::json TypedProperty<std::string>::Serialize() const {
    return m_Getter();
}

template<>
void TypedProperty<std::string>::Deserialize(const nlohmann::json& json) {
    if (json.is_string()) {
        m_Setter(json.get<std::string>());
    }
}

template<>
nlohmann::json TypedProperty<math::Vec2>::Serialize() const {
    auto value = m_Getter();
    return nlohmann::json{{"x", value.x}, {"y", value.y}};
}

template<>
void TypedProperty<math::Vec2>::Deserialize(const nlohmann::json& json) {
    if (json.is_object() && json.contains("x") && json.contains("y")) {
        math::Vec2 value;
        value.x = json["x"].get<float>();
        value.y = json["y"].get<float>();
        m_Setter(value);
    }
}

template<>
nlohmann::json TypedProperty<math::Vec3>::Serialize() const {
    auto value = m_Getter();
    return nlohmann::json{{"x", value.x}, {"y", value.y}, {"z", value.z}};
}

template<>
void TypedProperty<math::Vec3>::Deserialize(const nlohmann::json& json) {
    if (json.is_object() && json.contains("x") && json.contains("y") && json.contains("z")) {
        math::Vec3 value;
        value.x = json["x"].get<float>();
        value.y = json["y"].get<float>();
        value.z = json["z"].get<float>();
        m_Setter(value);
    }
}

template<>
nlohmann::json TypedProperty<math::Vec4>::Serialize() const {
    auto value = m_Getter();
    return nlohmann::json{{"x", value.x}, {"y", value.y}, {"z", value.z}, {"w", value.w}};
}

template<>
void TypedProperty<math::Vec4>::Deserialize(const nlohmann::json& json) {
    if (json.is_object() && json.contains("x") && json.contains("y") &&
        json.contains("z") && json.contains("w")) {
        math::Vec4 value;
        value.x = json["x"].get<float>();
        value.y = json["y"].get<float>();
        value.z = json["z"].get<float>();
        value.w = json["w"].get<float>();
        m_Setter(value);
    }
}

	template<>
	nlohmann::json TypedProperty<math::Quat>::Serialize() const {
	    auto value = m_Getter();
	    return nlohmann::json{{"x", value.x()}, {"y", value.y()}, {"z", value.z()}, {"w", value.w()}};
	}

	template<>
	void TypedProperty<math::Quat>::Deserialize(const nlohmann::json& json) {
	    if (json.is_object() && json.contains("x") && json.contains("y") &&
	        json.contains("z") && json.contains("w")) {
	        float x = json["x"].get<float>();
	        float y = json["y"].get<float>();
	        float z = json["z"].get<float>();
	        float w = json["w"].get<float>();
	        math::Quat value(w, x, y, z);
	        m_Setter(value);
	    }
	}

template<>
nlohmann::json TypedProperty<math::Color>::Serialize() const {
    auto value = m_Getter();
    return nlohmann::json{{"r", value.r}, {"g", value.g}, {"b", value.b}, {"a", value.a}};
}

template<>
void TypedProperty<math::Color>::Deserialize(const nlohmann::json& json) {
    if (json.is_object() && json.contains("r") && json.contains("g") &&
        json.contains("b") && json.contains("a")) {
        math::Color value;
        value.r = json["r"].get<float>();
        value.g = json["g"].get<float>();
        value.b = json["b"].get<float>();
        value.a = json["a"].get<float>();
        m_Setter(value);
    }
}

nlohmann::json ISerializable::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();

    nlohmann::json properties = nlohmann::json::object();
    for (const auto& prop : m_Properties) {
        properties[prop->GetName()] = prop->Serialize();
    }
    json["properties"] = properties;

    return json;
}

nlohmann::json ISerializable::SerializeWithMetadata() const {
    nlohmann::json json;
    json["type"] = GetTypeName();

    nlohmann::json properties = nlohmann::json::object();
    nlohmann::json metadata = nlohmann::json::object();

    for (const auto& prop : m_Properties) {
        const std::string& propName = prop->GetName();
        properties[propName] = prop->Serialize();

        PropertyDescriptor descriptor = prop->GetDescriptor();
        if (!descriptor.name.empty()) {

            metadata[propName] = descriptor.Serialize();
        } else {

            nlohmann::json basicMeta;
            basicMeta["name"] = propName;
            basicMeta["type"] = static_cast<int>(ConvertPropertyTypeToValueType(prop->GetType()));
            metadata[propName] = basicMeta;
        }
    }

    json["properties"] = properties;
    json["property_metadata"] = metadata;

    return json;
}

void ISerializable::Deserialize(const nlohmann::json& json) {
    if (!json.contains("properties")) {

        return;
    }

    const auto& properties = json["properties"];
    for (auto& prop : m_Properties) {
        if (properties.contains(prop->GetName())) {
            prop->Deserialize(properties[prop->GetName()]);
        }
    }
}

template<>
void ISerializable::RegisterProperty<bool>(const std::string& name, PropertyType type,
                                           std::function<bool()> getter,
                                           std::function<void(const bool&)> setter) {
    m_Properties.push_back(std::make_shared<TypedProperty<bool>>(name, type, getter, setter));
}

template<>
void ISerializable::RegisterProperty<int>(const std::string& name, PropertyType type,
                                          std::function<int()> getter,
                                          std::function<void(const int&)> setter) {
    m_Properties.push_back(std::make_shared<TypedProperty<int>>(name, type, getter, setter));
}

template<>
void ISerializable::RegisterProperty<float>(const std::string& name, PropertyType type,
                                            std::function<float()> getter,
                                            std::function<void(const float&)> setter) {
    m_Properties.push_back(std::make_shared<TypedProperty<float>>(name, type, getter, setter));
}

template<>
void ISerializable::RegisterProperty<double>(const std::string& name, PropertyType type,
                                             std::function<double()> getter,
                                             std::function<void(const double&)> setter) {
    m_Properties.push_back(std::make_shared<TypedProperty<double>>(name, type, getter, setter));
}

template<>
void ISerializable::RegisterProperty<std::string>(const std::string& name, PropertyType type,
                                                  std::function<std::string()> getter,
                                                  std::function<void(const std::string&)> setter) {
    m_Properties.push_back(std::make_shared<TypedProperty<std::string>>(name, type, getter, setter));
}

	template<>
	void ISerializable::RegisterProperty<math::Quat>(const std::string& name, PropertyType type,
	                                                 std::function<math::Quat()> getter,
	                                                 std::function<void(const math::Quat&)> setter) {
	    m_Properties.push_back(std::make_shared<TypedProperty<math::Quat>>(name, type, getter, setter));
	}

template<>
void ISerializable::RegisterProperty<math::Vec2>(const std::string& name, PropertyType type,
                                                 std::function<math::Vec2()> getter,
                                                 std::function<void(const math::Vec2&)> setter) {
    m_Properties.push_back(std::make_shared<TypedProperty<math::Vec2>>(name, type, getter, setter));
}

template<>
void ISerializable::RegisterProperty<math::Vec3>(const std::string& name, PropertyType type,
                                                 std::function<math::Vec3()> getter,
                                                 std::function<void(const math::Vec3&)> setter) {
    m_Properties.push_back(std::make_shared<TypedProperty<math::Vec3>>(name, type, getter, setter));
}

template<>
void ISerializable::RegisterProperty<math::Vec4>(const std::string& name, PropertyType type,
                                                 std::function<math::Vec4()> getter,
                                                 std::function<void(const math::Vec4&)> setter) {
    m_Properties.push_back(std::make_shared<TypedProperty<math::Vec4>>(name, type, getter, setter));
}

template<>
void ISerializable::RegisterProperty<math::Color>(const std::string& name, PropertyType type,
                                                  std::function<math::Color()> getter,
                                                  std::function<void(const math::Color&)> setter) {
    m_Properties.push_back(std::make_shared<TypedProperty<math::Color>>(name, type, getter, setter));
}

template<>
void ISerializable::RegisterPropertyWithMetadata<bool>(const std::string& name, PropertyType type,
                                                       std::function<bool()> getter,
                                                       std::function<void(const bool&)> setter,
                                                       const PropertyDescriptor& descriptor) {
    m_Properties.push_back(std::make_shared<TypedProperty<bool>>(name, type, getter, setter, descriptor));
}

template<>
void ISerializable::RegisterPropertyWithMetadata<int>(const std::string& name, PropertyType type,
                                                      std::function<int()> getter,
                                                      std::function<void(const int&)> setter,
                                                      const PropertyDescriptor& descriptor) {
    m_Properties.push_back(std::make_shared<TypedProperty<int>>(name, type, getter, setter, descriptor));
}

template<>
void ISerializable::RegisterPropertyWithMetadata<float>(const std::string& name, PropertyType type,
                                                        std::function<float()> getter,
                                                        std::function<void(const float&)> setter,
                                                        const PropertyDescriptor& descriptor) {
    m_Properties.push_back(std::make_shared<TypedProperty<float>>(name, type, getter, setter, descriptor));
}

template<>
void ISerializable::RegisterPropertyWithMetadata<double>(const std::string& name, PropertyType type,
                                                         std::function<double()> getter,
                                                         std::function<void(const double&)> setter,
                                                         const PropertyDescriptor& descriptor) {
    m_Properties.push_back(std::make_shared<TypedProperty<double>>(name, type, getter, setter, descriptor));
}

template<>
void ISerializable::RegisterPropertyWithMetadata<std::string>(const std::string& name, PropertyType type,
                                                              std::function<std::string()> getter,
                                                              std::function<void(const std::string&)> setter,
                                                              const PropertyDescriptor& descriptor) {
    m_Properties.push_back(std::make_shared<TypedProperty<std::string>>(name, type, getter, setter, descriptor));
}

template<>
void ISerializable::RegisterPropertyWithMetadata<math::Vec2>(const std::string& name, PropertyType type,
                                                             std::function<math::Vec2()> getter,
                                                             std::function<void(const math::Vec2&)> setter,
                                                             const PropertyDescriptor& descriptor) {
    m_Properties.push_back(std::make_shared<TypedProperty<math::Vec2>>(name, type, getter, setter, descriptor));
}

template<>
void ISerializable::RegisterPropertyWithMetadata<math::Vec3>(const std::string& name, PropertyType type,
                                                             std::function<math::Vec3()> getter,
                                                             std::function<void(const math::Vec3&)> setter,
                                                             const PropertyDescriptor& descriptor) {
    m_Properties.push_back(std::make_shared<TypedProperty<math::Vec3>>(name, type, getter, setter, descriptor));
}

template<>
void ISerializable::RegisterPropertyWithMetadata<math::Vec4>(const std::string& name, PropertyType type,
                                                             std::function<math::Vec4()> getter,
                                                             std::function<void(const math::Vec4&)> setter,
                                                             const PropertyDescriptor& descriptor) {
    m_Properties.push_back(std::make_shared<TypedProperty<math::Vec4>>(name, type, getter, setter, descriptor));
}

template<>
void ISerializable::RegisterPropertyWithMetadata<math::Color>(const std::string& name, PropertyType type,
                                                              std::function<math::Color()> getter,
                                                              std::function<void(const math::Color&)> setter,
                                                              const PropertyDescriptor& descriptor) {
    m_Properties.push_back(std::make_shared<TypedProperty<math::Color>>(name, type, getter, setter, descriptor));
}

std::string Serializer::SerializeToString(const ISerializable& object, int indent) {
    nlohmann::json json = object.Serialize();
    return json.dump(indent);
}

bool Serializer::DeserializeFromString(ISerializable& object, const std::string& jsonStr) {
    try {
        nlohmann::json json = nlohmann::json::parse(jsonStr);
        object.Deserialize(json);
        return true;
    } catch (const std::exception& e) {

        return false;
    }
}

nlohmann::json Serializer::SerializeToJson(const ISerializable& object) {
    return object.Serialize();
}

bool Serializer::DeserializeFromJson(ISerializable& object, const nlohmann::json& json) {
    try {
        object.Deserialize(json);
        return true;
    } catch (const std::exception& e) {

        return false;
    }
}

bool Serializer::SaveToFile(const ISerializable& object, const std::string& filepath, int indent) {
    try {
        std::string jsonStr = SerializeToString(object, indent);
        auto result = platform::FileSystem::WriteFile(filepath, jsonStr);
        if (!result.success) {

            return false;
        }

        return true;
    } catch (const std::exception& e) {

        return false;
    }
}

bool Serializer::LoadFromFile(ISerializable& object, const std::string& filepath) {
    try {
        auto result = platform::FileSystem::ReadFile(filepath);
        if (!result.success) {

            return false;
        }

        bool success = DeserializeFromString(object, result.data);
        if (success) {

        }
        return success;
    } catch (const std::exception& e) {

        return false;
    }
}

TypeRegistry& TypeRegistry::GetInstance() {
    static TypeRegistry instance;
    return instance;
}

void TypeRegistry::RegisterType(const std::string& typeName, FactoryFunc factory) {
    m_Factories[typeName] = factory;

}

std::shared_ptr<ISerializable> TypeRegistry::CreateInstance(const std::string& typeName) {
    auto it = m_Factories.find(typeName);
    if (it != m_Factories.end()) {
        return it->second();
    }

    return nullptr;
}

bool TypeRegistry::IsTypeRegistered(const std::string& typeName) const {
    return m_Factories.find(typeName) != m_Factories.end();
}

}
}

