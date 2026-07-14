#include "lupine/core/Component.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/PropertyDescriptor.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/rendering/FontBaker.hpp"

namespace lupine {
namespace core {

Component::Component()
    : m_UUID(), m_Name("Component"), m_Owner(nullptr), m_Enabled(true), m_PropertiesDefined(false) {
}

Component::Component(const std::string& name)
    : m_UUID(), m_Name(name), m_Owner(nullptr), m_Enabled(true), m_PropertiesDefined(false) {
}

Component::~Component() {
}

void Component::NotifyRenderStateChanged() {
    if (m_Owner) {
        m_Owner->MarkRenderDirty();
    }
}

std::vector<std::string> Component::GetImplementedInterfaces() const {
    return InterfaceRegistry::GetInstance().GetTypeInterfaces(GetTypeName());
}

void Component::RegisterProperties() {

    m_Properties.clear();

    RegisterProperty<std::string>("name", core::PropertyType::String,
        [this]() { return m_Name; },
        [this](const std::string& value) { m_Name = value; });

    RegisterProperty<bool>("enabled", core::PropertyType::Bool,
        [this]() { return m_Enabled; },
        [this](const bool& value) { m_Enabled = value; });

    if (!m_PropertiesDefined) {
        DefineProperties();
        m_PropertiesDefined = true;
    }

    auto descriptors = m_CustomProperties.GetPropertyDescriptors();
    for (const auto& desc : descriptors) {

        switch (desc.type) {
            case PropertyValueType::Bool: {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    PropertyDescriptor metadata = desc;
                    RegisterPropertyWithMetadata<bool>(desc.name, PropertyType::Bool,
                        [this, name = desc.name]() {
                            auto p = m_CustomProperties.GetProperty(name);
                            return p ? p->GetValue<bool>() : false;
                        },
                        [this, name = desc.name](const bool& value) {
                            auto p = m_CustomProperties.GetProperty(name);
                            if (p) p->SetValue(value);
                        },
                        metadata);
                }
                break;
            }
            case PropertyValueType::Int: {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    PropertyDescriptor metadata = desc;
                    RegisterPropertyWithMetadata<int>(desc.name, PropertyType::Int,
                        [this, name = desc.name]() {
                            auto p = m_CustomProperties.GetProperty(name);
                            return p ? p->GetValue<int>() : 0;
                        },
                        [this, name = desc.name](const int& value) {
                            auto p = m_CustomProperties.GetProperty(name);
                            if (p) p->SetValue(value);
                        },
                        metadata);
                }
                break;
            }
            case PropertyValueType::Float: {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    PropertyDescriptor metadata = desc;
                    RegisterPropertyWithMetadata<float>(desc.name, PropertyType::Float,
                        [this, name = desc.name]() {
                            auto p = m_CustomProperties.GetProperty(name);
                            return p ? p->GetValue<float>() : 0.0f;
                        },
                        [this, name = desc.name](const float& value) {
                            auto p = m_CustomProperties.GetProperty(name);
                            if (p) p->SetValue(value);
                        },
                        metadata);
                }
                break;
            }
            case PropertyValueType::String: {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    PropertyDescriptor metadata = desc;
                    RegisterPropertyWithMetadata<std::string>(desc.name, PropertyType::String,
                        [this, name = desc.name]() {
                            auto p = m_CustomProperties.GetProperty(name);
                            return p ? p->GetValue<std::string>() : std::string("");
                        },
                        [this, name = desc.name](const std::string& value) {
                            auto p = m_CustomProperties.GetProperty(name);
                            if (p) p->SetValue(value);
                        },
                        metadata);
                }
                break;
            }
            case PropertyValueType::Vec2: {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    PropertyDescriptor metadata = desc;
                    RegisterPropertyWithMetadata<math::Vec2>(desc.name, PropertyType::Vec2,
                        [this, name = desc.name]() {
                            auto p = m_CustomProperties.GetProperty(name);
                            return p ? p->GetValue<math::Vec2>() : math::Vec2();
                        },
                        [this, name = desc.name](const math::Vec2& value) {
                            auto p = m_CustomProperties.GetProperty(name);
                            if (p) p->SetValue(value);
                        },
                        metadata);
                }
                break;
            }
            case PropertyValueType::Vec3: {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    PropertyDescriptor metadata = desc;
                    RegisterPropertyWithMetadata<math::Vec3>(desc.name, PropertyType::Vec3,
                        [this, name = desc.name]() {
                            auto p = m_CustomProperties.GetProperty(name);
                            return p ? p->GetValue<math::Vec3>() : math::Vec3();
                        },
                        [this, name = desc.name](const math::Vec3& value) {
                            auto p = m_CustomProperties.GetProperty(name);
                            if (p) p->SetValue(value);
                        },
                        metadata);
                }
                break;
            }
            case PropertyValueType::Vec4: {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    PropertyDescriptor metadata = desc;
                    RegisterPropertyWithMetadata<math::Vec4>(desc.name, PropertyType::Vec4,
                        [this, name = desc.name]() {
                            auto p = m_CustomProperties.GetProperty(name);
                            return p ? p->GetValue<math::Vec4>() : math::Vec4();
                        },
                        [this, name = desc.name](const math::Vec4& value) {
                            auto p = m_CustomProperties.GetProperty(name);
                            if (p) p->SetValue(value);
                        },
                        metadata);
                }
                break;
            }
            case PropertyValueType::Color: {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    PropertyDescriptor metadata = desc;
                    RegisterPropertyWithMetadata<math::Color>(desc.name, PropertyType::Color,
                        [this, name = desc.name]() {
                            auto p = m_CustomProperties.GetProperty(name);
                            return p ? p->GetValue<math::Color>() : math::Color();
                        },
                        [this, name = desc.name](const math::Color& value) {
                            auto p = m_CustomProperties.GetProperty(name);
                            if (p) p->SetValue(value);
                        },
                        metadata);
                }
                break;
            }
            case PropertyValueType::Enum: {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    PropertyDescriptor metadata = desc;
                    RegisterPropertyWithMetadata<int>(desc.name, PropertyType::Int,
                        [this, name = desc.name]() {
                            auto p = m_CustomProperties.GetProperty(name);
                            return p ? p->GetValue<int>() : 0;
                        },
                        [this, name = desc.name](const int& value) {
                            auto p = m_CustomProperties.GetProperty(name);
                            if (p) p->SetValue(value);
                        },
                        metadata);
                }
                break;
            }
            default:

                break;
        }
    }
}

nlohmann::json Component::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();

    if (!m_CustomProperties.GetProperties().empty()) {
        json["properties"] = m_CustomProperties.SerializeProperties();

        // "name" and "enabled" are registered on the ISerializable property list, not on
        // m_CustomProperties, so this branch would otherwise drop them - and nearly every
        // gameplay component defines custom properties. Deserialize reads both back from here.
        // A component that declares its own property under either name keeps it.
        nlohmann::json& properties = json["properties"];
        if (!properties.contains("name")) {
            properties["name"] = m_Name;
        }
        if (!properties.contains("enabled")) {
            properties["enabled"] = m_Enabled;
        }
    } else {

        json = core::ISerializable::Serialize();
    }

    // Add asset UUIDs for file path properties (for robust asset referencing)
    auto& assetDb = asset::AssetDatabase::GetInstance();
    if (assetDb.IsInitialized()) {
        nlohmann::json assetUuids = nlohmann::json::object();

        for (const auto& desc : m_CustomProperties.GetPropertyDescriptors()) {
            // Check if this is a file property
            if (desc.hint.type == PropertyHintType::File) {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    std::string path = prop->GetValue<std::string>();
                    if (!path.empty()) {
                        // Get UUID for this asset path
                        const auto* meta = assetDb.GetAssetByPath(path);
                        if (meta && meta->uuid.IsValid()) {
                            assetUuids[desc.name] = meta->uuid.ToString();
                        }
                    }
                }
            }
        }

        if (!assetUuids.empty()) {
            json["asset_uuids"] = assetUuids;
        }
    }

    // Persist declarative (object+method) signal connections originating from
    // this component (e.g. a Button's "pressed" wired to a handler node).
    SignalObject::TargetResolveFn resolver = [](SignalObject* target) -> SignalObject::TargetLocator {
        if (Node* n = dynamic_cast<Node*>(target)) {
            return { n->GetUUID().ToString(), n->GetPath() };
        }
        return {};
    };
    nlohmann::json connections = SerializeConnections(resolver);
    if (!connections.empty()) {
        json["connections"] = connections;
    }

    return json;
}

void Component::Deserialize(const nlohmann::json& json) {

    if (!m_PropertiesDefined) {
        DefineProperties();
        m_PropertiesDefined = true;
    }

    if (!m_CustomProperties.GetProperties().empty()) {

        if (json.contains("properties") && json["properties"].is_object()) {
            m_CustomProperties.DeserializeProperties(json["properties"]);
        }

        else if (json.contains("custom_properties")) {
            m_CustomProperties.DeserializeProperties(json["custom_properties"]);
        }

        if (json.contains("properties")) {
            const auto& props = json["properties"];
            if (props.contains("enabled") && props["enabled"].is_boolean()) {
                m_Enabled = props["enabled"].get<bool>();
            }
            if (props.contains("name") && props["name"].is_string()) {
                m_Name = props["name"].get<std::string>();
            }
        }

        // Resolve asset paths using UUIDs for robust asset referencing
        // This ensures assets can be found even if moved/renamed
        auto& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized() && json.contains("asset_uuids")) {
            const auto& assetUuids = json["asset_uuids"];

            for (const auto& desc : m_CustomProperties.GetPropertyDescriptors()) {
                if (desc.hint.type == PropertyHintType::File) {
                    auto prop = m_CustomProperties.GetProperty(desc.name);
                    if (prop && assetUuids.contains(desc.name)) {
                        std::string uuidStr = assetUuids[desc.name].get<std::string>();
                        core::UUID uuid = core::UUID::FromString(uuidStr);

                        if (uuid.IsValid()) {
                            // Try to resolve path from UUID
                            const auto* meta = assetDb.GetAssetByUUID(uuid);
                            if (meta && !meta->path.empty()) {
                                // UUID resolved successfully - update path
                                prop->SetValue<std::string>(meta->path);
                            }
                            // If UUID didn't resolve, keep the path from deserialization
                        }
                    }
                }
            }
        }
    } else {

        core::ISerializable::Deserialize(json);
    }

    // Stash declarative connections; resolved once the tree is built.
    if (json.contains("connections")) {
        LoadConnectionData(json["connections"]);
    }
}

std::vector<core::PropertyDescriptor> Component::GetPropertyDescriptors() const {
    return m_CustomProperties.GetPropertyDescriptors();
}

bool Component::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
    bool affected = false;

    // Get the asset database for path resolution
    auto& assetDb = asset::AssetDatabase::GetInstance();

    // Iterate through all property descriptors
    auto descriptors = m_CustomProperties.GetPropertyDescriptors();
    for (const auto& desc : descriptors) {
        // Only check file-type properties
        if (desc.hint.type != PropertyHintType::File) {
            continue;
        }

        // Only string properties can hold file paths
        if (desc.type != PropertyValueType::String) {
            continue;
        }

        // Get current property value
        auto prop = m_CustomProperties.GetProperty(desc.name);
        if (!prop) continue;

        std::string propValue = prop->GetValue<std::string>();
        if (propValue.empty()) continue;

        // Resolve the property path for comparison
        std::string resolvedPropPath;
        if (assetDb.IsInitialized()) {
            resolvedPropPath = assetDb.ResolveAsset(propValue);
        }

        // Check if this property references the changed file
        bool matches = false;
        if (propValue == changedPath) {
            matches = true;
        } else if (!resolvedPropPath.empty() && !resolvedChangedPath.empty() &&
                   resolvedPropPath == resolvedChangedPath) {
            matches = true;
        }

        if (matches) {

            // Trigger reload by clearing and re-setting the property
            // This forces the component to reload the asset from disk
            prop->SetValue<std::string>("");
            prop->SetValue<std::string>(propValue);

            affected = true;
        }
    }

    return affected;
}

bool Component::ConsumeFontOversampleChanged() {
    uint64_t generation = GetFontOversampleGeneration();
    if (generation != m_FontOversampleGenSeen) {
        m_FontOversampleGenSeen = generation;
        return true;
    }
    return false;
}

}
}

