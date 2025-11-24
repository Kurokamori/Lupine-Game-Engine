#include "lupine/core/ScriptComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/PropertyDescriptor.hpp"
#include "lupine/logger/Logger.hpp"
#ifdef LUPINE_HAS_MRUBY
#include "lupine/scripting/MRubyEnvironment.hpp"
#endif
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>

namespace lupine {
namespace core {

ScriptComponent::ScriptComponent()
    : Component("ScriptComponent"),
      m_ScriptAPI(std::make_unique<scripting::ScriptAPI>()) {
}

ScriptComponent::ScriptComponent(const std::string& name)
    : Component(name),
      m_ScriptAPI(std::make_unique<scripting::ScriptAPI>()) {
}

ScriptComponent::~ScriptComponent() {
}

void ScriptComponent::RegisterProperties() {
    Component::RegisterProperties();

    PropertyDescriptor scriptPathDesc("script_path", PropertyValueType::String);
    scriptPathDesc.defaultValue = std::string("");
    scriptPathDesc.hint = PropertyHint(PropertyHintType::File, "*.lua,*.py");
    scriptPathDesc.description = "Path to the script file";

    RegisterPropertyWithMetadata<std::string>("script_path", PropertyType::String,
        [this]() { return m_ScriptPath; },
        [this](const std::string& value) { SetScriptPath(value); },
        scriptPathDesc);
}

nlohmann::json ScriptComponent::Serialize() const {
    nlohmann::json json = Component::Serialize();
    json["script_path"] = m_ScriptPath;

    nlohmann::json exportProps = nlohmann::json::object();
    for (const auto& prop : m_ExportProperties) {
        if (m_CustomProperties.HasProperty(prop.name)) {
            const ComponentProperty* compProp = m_CustomProperties.GetProperty(prop.name);
            if (compProp) {
                exportProps[prop.name] = compProp->GetValueAsJson();
            }
        }
    }
    if (!exportProps.empty()) {
        json["export_properties"] = exportProps;
    }

    return json;
}

void ScriptComponent::Deserialize(const nlohmann::json& json) {
    Component::Deserialize(json);

    if (json.contains("script_path")) {
        SetScriptPath(json["script_path"].get<std::string>());
    }

    if (json.contains("export_properties")) {
        const auto& exportProps = json["export_properties"];
        for (const auto& [key, value] : exportProps.items()) {
            if (m_CustomProperties.HasProperty(key)) {
                ComponentProperty* prop = m_CustomProperties.GetProperty(key);
                if (prop) {
                    prop->SetValueFromJson(value);
                }
            }
        }
    }
}

void ScriptComponent::SetScriptPath(const std::string& path) {
    m_ScriptPath = path;
    m_ScriptLoaded = false;
}

bool ScriptComponent::LoadScript() {
    if (m_ScriptPath.empty()) {

        return false;
    }

    std::ifstream file(m_ScriptPath);
    if (!file.is_open()) {

        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string scriptContent = buffer.str();
    file.close();

    ParseExportProperties(scriptContent);

    auto* env = GetEnvironment();
    if (!env) {

        return false;
    }

    if (!env->Initialize()) {

        return false;
    }

    if (m_ScriptAPI && m_Owner) {
        m_ScriptAPI->SetOwner(m_Owner);

        if (auto* luaEnv = dynamic_cast<scripting::LuaEnvironment*>(env)) {
            luaEnv->SetScriptAPI(m_ScriptAPI.get());
        } else if (auto* mrubyEnv = dynamic_cast<scripting::MRubyEnvironment*>(env)) {
            mrubyEnv->SetScriptAPI(m_ScriptAPI.get());
        }
    }

    for (const auto& prop : m_ExportProperties) {
        const ComponentProperty* compProp = m_CustomProperties.GetProperty(prop.name);
        if (!compProp) continue;

        switch (prop.descriptor.type) {
            case core::PropertyValueType::Int:
                env->SetGlobal(prop.name, compProp->GetValue<int>());
                break;
            case core::PropertyValueType::Float:
                env->SetGlobal(prop.name, compProp->GetValue<float>());
                break;
            case core::PropertyValueType::String:
                env->SetGlobal(prop.name, compProp->GetValue<std::string>());
                break;
            case core::PropertyValueType::Bool:
                env->SetGlobal(prop.name, compProp->GetValue<bool>());
                break;
            default:
                break;
        }
    }

    auto result = env->ExecuteString(scriptContent);
    if (!result) {

        return false;
    }

    m_ScriptLoaded = true;

    return true;
}

bool ScriptComponent::ReloadScript() {
    m_ScriptLoaded = false;
    m_ExportProperties.clear();
    return LoadScript();
}

std::string ScriptComponent::GetScriptDisplayName() const {
    if (m_ScriptPath.empty()) {
        return GetTypeName();
    }

    std::filesystem::path path(m_ScriptPath);
    return path.stem().string();
}

void ScriptComponent::OnAwake() {
    if (!m_ScriptLoaded) {
        LoadScript();
    }
    CallScriptFunction("on_awake");
}

void ScriptComponent::OnReady() {
    CallScriptFunction("on_ready");
}

void ScriptComponent::OnDestroy() {
    CallScriptFunction("on_destroy");
}

void ScriptComponent::OnInput(float deltaTime) {
    if (m_ScriptAPI) {
        m_ScriptAPI->SetDeltaTime(deltaTime);
    }
    CallScriptFunctionWithDelta("on_input", deltaTime);
}

void ScriptComponent::OnProcess(float deltaTime) {
    if (m_ScriptAPI) {
        m_ScriptAPI->SetDeltaTime(deltaTime);
    }

    auto* env = GetEnvironment();
    if (env) {
        for (const auto& prop : m_ExportProperties) {
            ComponentProperty* compProp = m_CustomProperties.GetProperty(prop.name);
            if (!compProp) continue;

            switch (prop.descriptor.type) {
                case core::PropertyValueType::Int:
                    compProp->SetValue(env->GetGlobalInt(prop.name, compProp->GetValue<int>()));
                    break;
                case core::PropertyValueType::Float:
                    compProp->SetValue(env->GetGlobalFloat(prop.name, compProp->GetValue<float>()));
                    break;
                case core::PropertyValueType::String:
                    compProp->SetValue(env->GetGlobalString(prop.name, compProp->GetValue<std::string>()));
                    break;
                case core::PropertyValueType::Bool:
                    compProp->SetValue(env->GetGlobalBool(prop.name, compProp->GetValue<bool>()));
                    break;
                default:
                    break;
            }
        }
    }

    CallScriptFunctionWithDelta("on_process", deltaTime);
}

void ScriptComponent::OnPhysicsProcess(float deltaTime) {
    if (m_ScriptAPI) {
        m_ScriptAPI->SetDeltaTime(deltaTime);
    }
    CallScriptFunctionWithDelta("on_physics_process", deltaTime);
}

void ScriptComponent::OnRender() {
    CallScriptFunction("on_render");
}

bool ScriptComponent::CallScriptFunction(const std::string& functionName) {
    if (!m_ScriptLoaded) {
        return false;
    }

    auto* env = GetEnvironment();
    if (!env || !env->HasFunction(functionName)) {
        return false;
    }

    auto result = env->CallFunction(functionName);
    if (!result) {

        return false;
    }

    return true;
}

bool ScriptComponent::CallScriptFunctionWithDelta(const std::string& functionName, float deltaTime) {
    if (!m_ScriptLoaded) {
        return false;
    }

    auto* env = GetEnvironment();
    if (!env || !env->HasFunction(functionName)) {
        return false;
    }

    env->SetGlobal("delta_time", deltaTime);

    for (const auto& prop : m_ExportProperties) {
        const ComponentProperty* compProp = m_CustomProperties.GetProperty(prop.name);
        if (!compProp) continue;

        switch (prop.descriptor.type) {
            case core::PropertyValueType::Int:
                env->SetGlobal(prop.name, compProp->GetValue<int>());
                break;
            case core::PropertyValueType::Float:
                env->SetGlobal(prop.name, compProp->GetValue<float>());
                break;
            case core::PropertyValueType::String:
                env->SetGlobal(prop.name, compProp->GetValue<std::string>());
                break;
            case core::PropertyValueType::Bool:
                env->SetGlobal(prop.name, compProp->GetValue<bool>());
                break;
            default:
                break;
        }
    }

    auto result = env->CallFunction(functionName);
    if (!result) {

        return false;
    }

    return true;
}

PythonScriptComponent::PythonScriptComponent()
    : ScriptComponent("PythonScriptComponent"),
      m_PythonEnv(std::make_unique<scripting::PythonEnvironment>()) {
}

PythonScriptComponent::PythonScriptComponent(const std::string& name)
    : ScriptComponent(name),
      m_PythonEnv(std::make_unique<scripting::PythonEnvironment>()) {
}

PythonScriptComponent::~PythonScriptComponent() {
}

void PythonScriptComponent::ParseExportProperties(const std::string& scriptContent) {

    std::regex exportRegex(R"(#\s*@export\s+(\w+)\s+(\w+)\s+(.+))");
    std::smatch match;

    std::string::const_iterator searchStart(scriptContent.cbegin());
    while (std::regex_search(searchStart, scriptContent.cend(), match, exportRegex)) {
        std::string varName = match[1].str();
        std::string typeName = match[2].str();
        std::string defaultStr = match[3].str();

        defaultStr.erase(0, defaultStr.find_first_not_of(" \t"));
        defaultStr.erase(defaultStr.find_last_not_of(" \t\n\r") + 1);

        core::PropertyDescriptor descriptor;
        descriptor.name = varName;

        if (typeName == "int" || typeName == "integer") {
            descriptor.type = core::PropertyValueType::Int;
            descriptor.defaultValue = std::stoi(defaultStr);
        }
        else if (typeName == "float" || typeName == "number") {
            descriptor.type = core::PropertyValueType::Float;
            descriptor.defaultValue = std::stof(defaultStr);
        }
        else if (typeName == "str" || typeName == "string") {
            descriptor.type = core::PropertyValueType::String;

            if (defaultStr.size() >= 2 &&
                ((defaultStr.front() == '"' && defaultStr.back() == '"') ||
                 (defaultStr.front() == '\'' && defaultStr.back() == '\''))) {
                defaultStr = defaultStr.substr(1, defaultStr.size() - 2);
            }
            descriptor.defaultValue = defaultStr;
        }
        else if (typeName == "bool" || typeName == "boolean") {
            descriptor.type = core::PropertyValueType::Bool;
            descriptor.defaultValue = (defaultStr == "True" || defaultStr == "true");
        }
        else {

            searchStart = match.suffix().first;
            continue;
        }

        ExportProperty exportProp;
        exportProp.name = varName;
        exportProp.descriptor = descriptor;
        m_ExportProperties.push_back(exportProp);

        DefineProperty(descriptor);

        searchStart = match.suffix().first;
    }
}

LuaScriptComponent::LuaScriptComponent()
    : ScriptComponent("LuaScriptComponent"),
      m_LuaEnv(std::make_unique<scripting::LuaEnvironment>()) {
}

LuaScriptComponent::LuaScriptComponent(const std::string& name)
    : ScriptComponent(name),
      m_LuaEnv(std::make_unique<scripting::LuaEnvironment>()) {
}

LuaScriptComponent::~LuaScriptComponent() {
}

void LuaScriptComponent::ParseExportProperties(const std::string& scriptContent) {

    std::regex exportRegex(R"(--@export\s+(\w+)\s+(\w+)\s+(.+))");
    std::smatch match;

    std::string::const_iterator searchStart(scriptContent.cbegin());
    while (std::regex_search(searchStart, scriptContent.cend(), match, exportRegex)) {
        std::string varName = match[1].str();
        std::string typeName = match[2].str();
        std::string defaultStr = match[3].str();

        defaultStr.erase(0, defaultStr.find_first_not_of(" \t"));
        defaultStr.erase(defaultStr.find_last_not_of(" \t\n\r") + 1);

        core::PropertyDescriptor descriptor;
        descriptor.name = varName;

        if (typeName == "int" || typeName == "integer") {
            descriptor.type = core::PropertyValueType::Int;
            descriptor.defaultValue = std::stoi(defaultStr);
        }
        else if (typeName == "float" || typeName == "number") {
            descriptor.type = core::PropertyValueType::Float;
            descriptor.defaultValue = std::stof(defaultStr);
        }
        else if (typeName == "string") {
            descriptor.type = core::PropertyValueType::String;

            if (defaultStr.size() >= 2 &&
                ((defaultStr.front() == '"' && defaultStr.back() == '"') ||
                 (defaultStr.front() == '\'' && defaultStr.back() == '\''))) {
                defaultStr = defaultStr.substr(1, defaultStr.size() - 2);
            }
            descriptor.defaultValue = defaultStr;
        }
        else if (typeName == "bool" || typeName == "boolean") {
            descriptor.type = core::PropertyValueType::Bool;
            descriptor.defaultValue = (defaultStr == "true");
        }
        else {

            searchStart = match.suffix().first;
            continue;
        }

        ExportProperty exportProp;
        exportProp.name = varName;
        exportProp.descriptor = descriptor;
        m_ExportProperties.push_back(exportProp);

        DefineProperty(descriptor);

        searchStart = match.suffix().first;
    }
}

#ifdef LUPINE_HAS_MRUBY

MRubyScriptComponent::MRubyScriptComponent()
    : ScriptComponent("MRubyScriptComponent"),
      m_MRubyEnv(std::make_unique<scripting::MRubyEnvironment>()) {
}

MRubyScriptComponent::MRubyScriptComponent(const std::string& name)
    : ScriptComponent(name),
      m_MRubyEnv(std::make_unique<scripting::MRubyEnvironment>()) {
}

MRubyScriptComponent::~MRubyScriptComponent() {
}

void MRubyScriptComponent::ParseExportProperties(const std::string& scriptContent) {

    std::regex exportRegex(R"(#@export\s+(\w+)\s+(\w+)\s+(.+))");
    std::smatch match;

    std::string::const_iterator searchStart(scriptContent.cbegin());
    while (std::regex_search(searchStart, scriptContent.cend(), match, exportRegex)) {
        std::string varName = match[1].str();
        std::string typeName = match[2].str();
        std::string defaultStr = match[3].str();

        defaultStr.erase(0, defaultStr.find_first_not_of(" \t"));
        defaultStr.erase(defaultStr.find_last_not_of(" \t\n\r") + 1);

        core::PropertyDescriptor descriptor;
        descriptor.name = varName;

        if (typeName == "int" || typeName == "integer") {
            descriptor.type = core::PropertyValueType::Int;
            descriptor.defaultValue = std::stoi(defaultStr);
        }
        else if (typeName == "float" || typeName == "number") {
            descriptor.type = core::PropertyValueType::Float;
            descriptor.defaultValue = std::stof(defaultStr);
        }
        else if (typeName == "string" || typeName == "str") {
            descriptor.type = core::PropertyValueType::String;

            if (defaultStr.front() == '"' && defaultStr.back() == '"') {
                defaultStr = defaultStr.substr(1, defaultStr.length() - 2);
            }
            descriptor.defaultValue = defaultStr;
        }
        else if (typeName == "bool" || typeName == "boolean") {
            descriptor.type = core::PropertyValueType::Bool;
            descriptor.defaultValue = (defaultStr == "true" || defaultStr == "True");
        }
        else {

            searchStart = match.suffix().first;
            continue;
        }

        ExportProperty exportProp;
        exportProp.name = varName;
        exportProp.descriptor = descriptor;
        m_ExportProperties.push_back(exportProp);

        DefineProperty(descriptor);

        searchStart = match.suffix().first;
    }
}

#endif

}
}
