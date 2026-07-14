#include "lupine/core/InterfaceDefinition.hpp"

namespace lupine {
namespace core {

namespace {

nlohmann::json SerializeArg(const SignalArgDesc& arg) {
    nlohmann::json json;
    json["name"] = arg.name;
    json["type"] = static_cast<int>(arg.type);
    return json;
}

SignalArgDesc DeserializeArg(const nlohmann::json& json) {
    SignalArgDesc arg;
    if (json.contains("name") && json["name"].is_string()) {
        arg.name = json["name"].get<std::string>();
    }
    if (json.contains("type") && json["type"].is_number_integer()) {
        arg.type = static_cast<PropertyValueType>(json["type"].get<int>());
    }
    return arg;
}

nlohmann::json SerializeSignal(const SignalDesc& sig) {
    nlohmann::json json;
    json["name"] = sig.name;
    json["doc"] = sig.doc;
    nlohmann::json args = nlohmann::json::array();
    for (const SignalArgDesc& arg : sig.args) {
        args.push_back(SerializeArg(arg));
    }
    json["args"] = args;
    return json;
}

SignalDesc DeserializeSignal(const nlohmann::json& json) {
    SignalDesc sig;
    if (json.contains("name") && json["name"].is_string()) {
        sig.name = json["name"].get<std::string>();
    }
    if (json.contains("doc") && json["doc"].is_string()) {
        sig.doc = json["doc"].get<std::string>();
    }
    if (json.contains("args") && json["args"].is_array()) {
        for (const nlohmann::json& argJson : json["args"]) {
            sig.args.push_back(DeserializeArg(argJson));
        }
    }
    return sig;
}

nlohmann::json SerializeMethod(const InterfaceMethod& method) {
    nlohmann::json json;
    json["name"] = method.name;
    json["doc"] = method.doc;
    json["has_return"] = method.hasReturn;
    json["return_type"] = static_cast<int>(method.returnType);
    nlohmann::json params = nlohmann::json::array();
    for (const SignalArgDesc& param : method.params) {
        params.push_back(SerializeArg(param));
    }
    json["params"] = params;
    return json;
}

InterfaceMethod DeserializeMethod(const nlohmann::json& json) {
    InterfaceMethod method;
    if (json.contains("name") && json["name"].is_string()) {
        method.name = json["name"].get<std::string>();
    }
    if (json.contains("doc") && json["doc"].is_string()) {
        method.doc = json["doc"].get<std::string>();
    }
    if (json.contains("has_return") && json["has_return"].is_boolean()) {
        method.hasReturn = json["has_return"].get<bool>();
    }
    if (json.contains("return_type") && json["return_type"].is_number_integer()) {
        method.returnType = static_cast<PropertyValueType>(json["return_type"].get<int>());
    }
    if (json.contains("params") && json["params"].is_array()) {
        for (const nlohmann::json& paramJson : json["params"]) {
            method.params.push_back(DeserializeArg(paramJson));
        }
    }
    return method;
}

} // namespace

const InterfaceMethod* InterfaceDefinition::FindMethod(const std::string& methodName) const {
    for (const InterfaceMethod& method : methods) {
        if (method.name == methodName) {
            return &method;
        }
    }
    return nullptr;
}

const SignalDesc* InterfaceDefinition::FindSignal(const std::string& signalName) const {
    for (const SignalDesc& sig : signals) {
        if (sig.name == signalName) {
            return &sig;
        }
    }
    return nullptr;
}

nlohmann::json InterfaceDefinition::Serialize() const {
    nlohmann::json json;
    json["lupine_interface"] = 1;
    json["interface_name"] = name;
    json["description"] = description;

    nlohmann::json basesJson = nlohmann::json::array();
    for (const std::string& base : baseInterfaces) {
        basesJson.push_back(base);
    }
    json["extends"] = basesJson;

    nlohmann::json methodsJson = nlohmann::json::array();
    for (const InterfaceMethod& method : methods) {
        methodsJson.push_back(SerializeMethod(method));
    }
    json["methods"] = methodsJson;

    nlohmann::json signalsJson = nlohmann::json::array();
    for (const SignalDesc& sig : signals) {
        signalsJson.push_back(SerializeSignal(sig));
    }
    json["signals"] = signalsJson;

    nlohmann::json tagsJson = nlohmann::json::array();
    for (const std::string& tag : tags) {
        tagsJson.push_back(tag);
    }
    json["tags"] = tagsJson;

    return json;
}

InterfaceDefinition InterfaceDefinition::Deserialize(const nlohmann::json& json,
                                                     InterfaceSource source,
                                                     const std::string& sourcePath) {
    InterfaceDefinition def;
    def.source = source;
    def.sourcePath = sourcePath;

    if (json.contains("interface_name") && json["interface_name"].is_string()) {
        def.name = json["interface_name"].get<std::string>();
    }
    if (json.contains("description") && json["description"].is_string()) {
        def.description = json["description"].get<std::string>();
    }

    // "extends" may be a single string or an array of strings.
    if (json.contains("extends")) {
        const nlohmann::json& extends = json["extends"];
        if (extends.is_string()) {
            std::string base = extends.get<std::string>();
            if (!base.empty()) {
                def.baseInterfaces.push_back(base);
            }
        } else if (extends.is_array()) {
            for (const nlohmann::json& baseJson : extends) {
                if (baseJson.is_string() && !baseJson.get<std::string>().empty()) {
                    def.baseInterfaces.push_back(baseJson.get<std::string>());
                }
            }
        }
    }

    if (json.contains("methods") && json["methods"].is_array()) {
        for (const nlohmann::json& methodJson : json["methods"]) {
            // Accept both rich objects and bare method-name strings.
            if (methodJson.is_string()) {
                InterfaceMethod method;
                method.name = methodJson.get<std::string>();
                if (!method.name.empty()) {
                    def.methods.push_back(method);
                }
            } else if (methodJson.is_object()) {
                InterfaceMethod method = DeserializeMethod(methodJson);
                if (!method.name.empty()) {
                    def.methods.push_back(method);
                }
            }
        }
    }

    if (json.contains("signals") && json["signals"].is_array()) {
        for (const nlohmann::json& signalJson : json["signals"]) {
            if (signalJson.is_string()) {
                SignalDesc sig;
                sig.name = signalJson.get<std::string>();
                if (!sig.name.empty()) {
                    def.signals.push_back(sig);
                }
            } else if (signalJson.is_object()) {
                SignalDesc sig = DeserializeSignal(signalJson);
                if (!sig.name.empty()) {
                    def.signals.push_back(sig);
                }
            }
        }
    }

    if (json.contains("tags") && json["tags"].is_array()) {
        for (const nlohmann::json& tagJson : json["tags"]) {
            if (tagJson.is_string() && !tagJson.get<std::string>().empty()) {
                def.tags.push_back(tagJson.get<std::string>());
            }
        }
    }

    def.isValid = !def.name.empty();
    if (!def.isValid) {
        def.parseError = "Interface definition is missing 'interface_name'";
    }

    return def;
}

} // namespace core
} // namespace lupine
