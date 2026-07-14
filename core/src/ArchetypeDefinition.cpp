#include "lupine/core/ArchetypeDefinition.hpp"

namespace lupine {
namespace core {

const PropertyDescriptor* ArchetypeDefinition::FindField(const std::string& name) const {
    for (const PropertyDescriptor& field : fields) {
        if (field.name == name) {
            return &field;
        }
    }
    return nullptr;
}

nlohmann::json ArchetypeDefinition::Serialize() const {
    nlohmann::json json;
    json["lupine_archetype"] = 1;
    json["archetype_class"] = className;
    json["extends"] = baseClass;
    json["abstract"] = isAbstract;
    json["menu_path"] = menuPath;
    json["icon"] = iconPath;
    json["description"] = description;

    nlohmann::json fieldsJson = nlohmann::json::array();
    for (const PropertyDescriptor& field : fields) {
        fieldsJson.push_back(field.Serialize());
    }
    json["fields"] = fieldsJson;

    nlohmann::json interfacesJson = nlohmann::json::array();
    for (const std::string& iface : implementsInterfaces) {
        interfacesJson.push_back(iface);
    }
    json["implements"] = interfacesJson;

    return json;
}

ArchetypeDefinition ArchetypeDefinition::Deserialize(const nlohmann::json& json,
                                                     ArchetypeSource source,
                                                     const std::string& sourcePath) {
    ArchetypeDefinition def;
    def.source = source;
    def.sourcePath = sourcePath;

    if (json.contains("archetype_class") && json["archetype_class"].is_string()) {
        def.className = json["archetype_class"].get<std::string>();
    }
    if (json.contains("extends") && json["extends"].is_string()) {
        def.baseClass = json["extends"].get<std::string>();
    }
    if (json.contains("abstract") && json["abstract"].is_boolean()) {
        def.isAbstract = json["abstract"].get<bool>();
    }
    if (json.contains("menu_path") && json["menu_path"].is_string()) {
        def.menuPath = json["menu_path"].get<std::string>();
    }
    if (json.contains("icon") && json["icon"].is_string()) {
        def.iconPath = json["icon"].get<std::string>();
    }
    if (json.contains("description") && json["description"].is_string()) {
        def.description = json["description"].get<std::string>();
    }

    if (json.contains("fields") && json["fields"].is_array()) {
        for (const nlohmann::json& fieldJson : json["fields"]) {
            PropertyDescriptor descriptor = PropertyDescriptor::Deserialize(fieldJson);
            if (!descriptor.name.empty()) {
                def.fields.push_back(descriptor);
            }
        }
    }

    // "implements" may be a single string or an array of interface names.
    if (json.contains("implements")) {
        const nlohmann::json& implements = json["implements"];
        if (implements.is_string() && !implements.get<std::string>().empty()) {
            def.implementsInterfaces.push_back(implements.get<std::string>());
        } else if (implements.is_array()) {
            for (const nlohmann::json& ifaceJson : implements) {
                if (ifaceJson.is_string() && !ifaceJson.get<std::string>().empty()) {
                    def.implementsInterfaces.push_back(ifaceJson.get<std::string>());
                }
            }
        }
    }

    def.isValid = !def.className.empty();
    if (!def.isValid) {
        def.parseError = "Archetype definition is missing 'archetype_class'";
    }

    return def;
}

nlohmann::json ArchetypeDefinition::BuildDefaultValues() const {
    nlohmann::json values = nlohmann::json::object();
    for (const PropertyDescriptor& field : fields) {
        values[field.name] = field.GetDefaultAsJson();
    }
    return values;
}

nlohmann::json ArchetypeDefinition::BuildPropertyMetadata() const {
    nlohmann::json metadata = nlohmann::json::object();
    for (const PropertyDescriptor& field : fields) {
        metadata[field.name] = field.Serialize();
    }
    return metadata;
}

} // namespace core
} // namespace lupine
