#include "lupine/core/ScriptComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/PropertyDescriptor.hpp"
#include "lupine/core/ScriptAnnotationParser.hpp"
#include "lupine/asset/Asset.hpp"
#include "lupine/platform/VirtualFileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/logger/Logger.hpp"
#ifdef LUPINE_HAS_MRUBY
#include "lupine/scripting/MRubyEnvironment.hpp"
#endif
#ifdef LUPINE_HAS_MICROPYTHON
#include "lupine/scripting/MicroPythonEnvironment.hpp"
#endif
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <cmath>

namespace lupine {
namespace core {

namespace {

// Push a single export property value into the script environment as a global.
// Scalars use the typed setters; every richer type (vectors, colors, quaternions,
// rects, doubles, arrays, dictionaries, inline structs) routes through SetGlobalJson.
void PushExportGlobal(scripting::IScriptEnvironment* env, const ComponentProperty* compProp) {
    const PropertyDescriptor& d = compProp->GetDescriptor();
    switch (d.type) {
        case PropertyValueType::Int:
        case PropertyValueType::Enum:
            env->SetGlobal(d.name, compProp->GetValue<int>());
            break;
        case PropertyValueType::Float:
            env->SetGlobal(d.name, compProp->GetValue<float>());
            break;
        case PropertyValueType::String:
        case PropertyValueType::NodePath:
        case PropertyValueType::ScenePath:
        case PropertyValueType::Resource:
            env->SetGlobal(d.name, compProp->GetValue<std::string>());
            break;
        case PropertyValueType::Bool:
            env->SetGlobal(d.name, compProp->GetValue<bool>());
            break;
        default:
            env->SetGlobalJson(d.name, compProp->GetValueAsJson());
            break;
    }
}

// Read a single export property value back from the script environment, mirroring
// PushExportGlobal's type routing. The current value is the fallback so an absent
// or unreadable global leaves the property unchanged.
void PullExportGlobal(scripting::IScriptEnvironment* env, ComponentProperty* compProp) {
    const PropertyDescriptor& d = compProp->GetDescriptor();
    switch (d.type) {
        case PropertyValueType::Int:
        case PropertyValueType::Enum:
            compProp->SetValue(env->GetGlobalInt(d.name, compProp->GetValue<int>()));
            break;
        case PropertyValueType::Float:
            compProp->SetValue(env->GetGlobalFloat(d.name, compProp->GetValue<float>()));
            break;
        case PropertyValueType::String:
        case PropertyValueType::NodePath:
        case PropertyValueType::ScenePath:
        case PropertyValueType::Resource:
            compProp->SetValue(env->GetGlobalString(d.name, compProp->GetValue<std::string>()));
            break;
        case PropertyValueType::Bool:
            compProp->SetValue(env->GetGlobalBool(d.name, compProp->GetValue<bool>()));
            break;
        default: {
            nlohmann::json current = compProp->GetValueAsJson();
            compProp->SetValueFromJson(env->GetGlobalJson(d.name, current));
            break;
        }
    }
}

} // namespace

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

    // Note: script_path is also defined in DefineProperties() for the custom property registry.
    // Here we register it for serialization with proper metadata.
    PropertyDescriptor scriptPathDesc("script_path", PropertyValueType::String);
    scriptPathDesc.defaultValue = std::string("");
    scriptPathDesc.hint = PropertyHint(PropertyHintType::File, "*.lua,*.py,*.rb");
    scriptPathDesc.description = "Path to the script file";

    RegisterPropertyWithMetadata<std::string>("script_path", PropertyType::String,
        [this]() { return m_ScriptPath; },
        [this](const std::string& value) { SetScriptPath(value); },
        scriptPathDesc);
}

void ScriptComponent::DefineProperties() {
    // Define script_path in the custom property registry so it can be set via the editor
    PropertyDescriptor scriptPathDesc("script_path", PropertyValueType::String);
    scriptPathDesc.defaultValue = std::string("");
    scriptPathDesc.hint = PropertyHint(PropertyHintType::File, "*.lua,*.py,*.rb");
    scriptPathDesc.description = "Path to the script file";
    DefineProperty(scriptPathDesc);
}

void ScriptComponent::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    // Handle script_path changes from the custom property registry
    if (propertyName == "script_path") {
        if (newValue.is_string()) {
            SetScriptPath(newValue.get<std::string>());
        }
        return;
    }

    // Handle export property changes - push new value to script environment
    if (m_ScriptLoaded) {
        auto* env = GetEnvironment();
        if (env) {
            // Find the export property to get its type
            for (const auto& prop : m_ExportProperties) {
                if (prop.name == propertyName) {
                    switch (prop.descriptor.type) {
                        case core::PropertyValueType::Int:
                        case core::PropertyValueType::Enum:
                            if (newValue.is_number_integer()) {
                                env->SetGlobal(propertyName, newValue.get<int>());
                            } else if (newValue.is_number()) {
                                env->SetGlobal(propertyName, static_cast<int>(newValue.get<double>()));
                            }
                            break;
                        case core::PropertyValueType::Float:
                            if (newValue.is_number()) {
                                env->SetGlobal(propertyName, newValue.get<float>());
                            }
                            break;
                        case core::PropertyValueType::String:
                            if (newValue.is_string()) {
                                env->SetGlobal(propertyName, newValue.get<std::string>());
                            }
                            break;
                        case core::PropertyValueType::Bool:
                            if (newValue.is_boolean()) {
                                env->SetGlobal(propertyName, newValue.get<bool>());
                            }
                            break;
                        default:
                            break;
                    }
                    break;
                }
            }
        }
    }
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

    // Store export properties for later - they'll be applied after LoadScript
    // parses the script and creates the property definitions
    if (json.contains("export_properties")) {
        m_PendingExportProperties = json["export_properties"];
    }
}

void ScriptComponent::SetScriptPath(const std::string& path) {
    m_ScriptPath = path;
    m_ScriptLoaded = false;

    // Also sync with custom property registry if it exists
    if (m_CustomProperties.HasProperty("script_path")) {
        ComponentProperty* prop = m_CustomProperties.GetProperty("script_path");
        if (prop && prop->GetValue<std::string>() != path) {
            prop->SetValue(path);
        }
    }
}

void ScriptComponent::ParseExportsWithPrefix(const std::string& scriptContent,
                                             const std::string& commentPrefix) {
    ScriptExportParseResult parsed =
        ScriptAnnotationParser::ParseExports(scriptContent, commentPrefix);

    for (const PropertyDescriptor& descriptor : parsed.properties) {
        ExportProperty exportProp;
        exportProp.name = descriptor.name;
        exportProp.group = descriptor.group;
        exportProp.descriptor = descriptor;
        m_ExportProperties.push_back(exportProp);

        DefineProperty(descriptor);
    }
}

void ScriptComponent::ParseExtendsDirective(const std::string& scriptContent) {
    // Default implementation - parses extends directives for all languages
    // Lua: --@extends "path/to/base.lua"
    // Ruby: #@extends "path/to/base.rb"
    // Python: #@extends "path/to/base.py"

    std::regex extendsRegex(R"((?:--|#)\s*@extends\s+[\"']([^\"']+)[\"'])");
    std::smatch match;

    if (std::regex_search(scriptContent, match, extendsRegex)) {
        m_ExtendsPath = match[1].str();
    }
}

void ScriptComponent::ParseToolDirective(const std::string& scriptContent) {
    // Language-agnostic prefix (-- for Lua, # for Python/Ruby). The presence of
    // the directive anywhere in the file marks the script as a tool script:
    //   --@tool
    //   #@tool
    // Only ever sets the flag (never clears it), so a @tool base script keeps a
    // derived script in tool mode even if the derived file omits the directive.
    std::regex toolRegex(R"((?:--|#)\s*@tool\b)");
    if (std::regex_search(scriptContent, toolRegex)) {
        m_IsTool = true;
    }
}

bool ScriptComponent::ScriptCallbacksAllowed() const {
    // Tool scripts always run; for everything else, running is gated on NOT being
    // in an editor-mode scene. The owning node's scene carries the editor flag
    // (set by EditorBridge when a scene is opened for editing); an editor-launched
    // runtime is a separate process whose scenes are never flagged in-editor.
    if (m_IsTool) {
        return true;
    }
    const Node* owner = GetOwner();
    const Scene* scene = owner ? owner->GetScene() : nullptr;
    return !(scene && scene->IsInEditor());
}

void ScriptComponent::ParseSignals(const std::string& scriptContent) {
    // Language-agnostic prefix (-- for Lua, # for Python/Ruby), an optional
    // typed argument list:
    //   --@signal health_changed(int amount, string who)
    //   #@signal died
    std::regex signalRegex(R"((?:--|#)\s*@signal\s+(\w+)\s*(?:\(([^)]*)\))?)");
    std::regex argRegex(R"(\s*(?:(\w+)\s+)?(\w+)\s*)");

    auto mapType = [](const std::string& typeName) -> core::PropertyValueType {
        if (typeName == "int" || typeName == "integer") return core::PropertyValueType::Int;
        if (typeName == "bool" || typeName == "boolean") return core::PropertyValueType::Bool;
        if (typeName == "string") return core::PropertyValueType::String;
        if (typeName == "vec2") return core::PropertyValueType::Vec2;
        if (typeName == "vec3") return core::PropertyValueType::Vec3;
        return core::PropertyValueType::Float;
    };

    std::istringstream stream(scriptContent);
    std::string line;
    while (std::getline(stream, line)) {
        std::smatch match;
        if (!std::regex_search(line, match, signalRegex)) {
            continue;
        }

        std::string signalName = match[1].str();
        std::vector<core::SignalArgDesc> argDescs;

        std::string argList = match[2].matched ? match[2].str() : std::string();
        if (!argList.empty()) {
            std::stringstream argStream(argList);
            std::string token;
            while (std::getline(argStream, token, ',')) {
                std::smatch argMatch;
                if (std::regex_match(token, argMatch, argRegex)) {
                    core::SignalArgDesc desc;
                    // Group 1 is the optional type, group 2 the argument name.
                    if (argMatch[1].matched) {
                        desc.type = mapType(argMatch[1].str());
                        desc.name = argMatch[2].str();
                    } else {
                        desc.name = argMatch[2].str();
                    }
                    argDescs.push_back(desc);
                }
            }
        }

        AddUserSignal(signalName, argDescs);
    }
}

void ScriptComponent::ParseRpcs(const std::string& scriptContent) {
    // Language-agnostic prefix (-- for Lua, # for Python/Ruby). The optional
    // mode/transfer/call_local tokens may appear in any order before the final
    // function-name token; an optional typed argument list is ignored here (it is
    // documentation only - RPC arguments cross as dynamic values).
    //   --@rpc any_peer unreliable call_local move(float x, float y)
    //   #@rpc authority reliable take_damage
    //   --@rpc shoot
    std::regex rpcRegex(R"((?:--|#)\s*@rpc\s+([^\r\n(]+))");

    std::istringstream stream(scriptContent);
    std::string line;
    while (std::getline(stream, line)) {
        std::smatch match;
        if (!std::regex_search(line, match, rpcRegex)) {
            continue;
        }

        // Tokenize the captured tail (whitespace-separated). The final token is
        // the function name; the rest are option keywords.
        std::vector<std::string> tokens;
        std::stringstream tokenStream(match[1].str());
        std::string token;
        while (tokenStream >> token) {
            tokens.push_back(token);
        }
        if (tokens.empty()) {
            continue;
        }

        // The function name may carry a trailing "(args...)" fragment; strip it.
        std::string functionName = tokens.back();
        const std::size_t paren = functionName.find('(');
        if (paren != std::string::npos) {
            functionName = functionName.substr(0, paren);
        }
        if (functionName.empty()) {
            continue;
        }

        network::RpcConfig config;
        for (std::size_t i = 0; i + 1 < tokens.size(); ++i) {
            const std::string& opt = tokens[i];
            if (opt == "authority") {
                config.mode = network::RpcMode::Authority;
            } else if (opt == "any_peer" || opt == "anypeer") {
                config.mode = network::RpcMode::AnyPeer;
            } else if (opt == "reliable") {
                config.transfer = network::TransferMode::Reliable;
            } else if (opt == "unreliable") {
                config.transfer = network::TransferMode::Unreliable;
            } else if (opt == "call_local" || opt == "calllocal") {
                config.callLocal = true;
            }
        }

        m_RpcConfigs[functionName] = config;
    }
}

const network::RpcConfig* ScriptComponent::GetRpcConfig(const std::string& method) const {
    auto it = m_RpcConfigs.find(method);
    return (it != m_RpcConfigs.end()) ? &it->second : nullptr;
}

void ScriptComponent::ParseInterfaces(const std::string& scriptContent) {
    // Language-agnostic prefix (-- for Lua, # for Python/Ruby). Interface names
    // may be comma- and/or whitespace-separated, and the annotation may repeat:
    //   --@interface Damageable
    //   #@interface Damageable, Destructible
    // (The @interface_define / @interface_method directives that DEFINE an
    // interface are consumed by InterfaceRegistry, not here, and do not match
    // because they have no whitespace right after "interface".)
    std::regex interfaceRegex(R"((?:--|#)\s*@interface\s+([A-Za-z_][\w,\s]*))");

    std::istringstream stream(scriptContent);
    std::string line;
    while (std::getline(stream, line)) {
        std::smatch match;
        if (!std::regex_search(line, match, interfaceRegex)) {
            continue;
        }
        std::stringstream commaStream(match[1].str());
        std::string segment;
        while (std::getline(commaStream, segment, ',')) {
            std::stringstream wsStream(segment);
            std::string name;
            while (wsStream >> name) {
                if (std::find(m_Interfaces.begin(), m_Interfaces.end(), name) == m_Interfaces.end()) {
                    m_Interfaces.push_back(name);
                }
            }
        }
    }

    // Pair with the signal system: ensure each declared interface's required
    // signals exist as user signals on this component, so a handler can connect
    // to (and the script can emit) the contract's signals without redeclaring
    // them via @signal.
    InterfaceRegistry& registry = InterfaceRegistry::GetInstance();
    for (const std::string& iface : m_Interfaces) {
        for (const SignalDesc& sig : registry.GetEffectiveSignals(iface)) {
            if (!HasSignal(sig.name)) {
                AddUserSignal(sig.name, sig.args);
            }
        }
    }
}

std::vector<std::string> ScriptComponent::GetImplementedInterfaces() const {
    // Merge any type-level conformance with the script's @interface declarations.
    std::vector<std::string> result = Component::GetImplementedInterfaces();
    for (const std::string& iface : m_Interfaces) {
        if (std::find(result.begin(), result.end(), iface) == result.end()) {
            result.push_back(iface);
        }
    }
    return result;
}

bool ScriptComponent::HasInterfaceMethod(const std::string& method) const {
    scripting::IScriptEnvironment* env = const_cast<ScriptComponent*>(this)->GetEnvironment();
    return env && env->HasFunction(method);
}

nlohmann::json ScriptComponent::CallMethod(const std::string& method, const nlohmann::json& args) {
    // A node instantiated at runtime (instantiate_prefab / instantiate_scene) has not been
    // through OnAwake yet, so its script is still unloaded. Dropping the call silently left
    // the caller believing it had configured the fresh instance, when in truth the instance
    // kept the prefab's serialized @export defaults -- so load on demand instead.
    //
    // Loading here also fixes the value, rather than racing it: LoadScript pushes the
    // serialized exports into the environment as part of loading, i.e. BEFORE this call
    // runs, and the later OnAwake sees m_ScriptLoaded and neither re-executes the body nor
    // re-pushes those defaults over whatever this call just assigned.
    if (!m_ScriptLoaded) {
        LoadScript();
    }

    scripting::IScriptEnvironment* env = GetEnvironment();
    if (!env || !m_ScriptLoaded || !env->HasFunction(method)) {
        return nlohmann::json();
    }

    // Callers pass an argument array; tolerate a bare single argument and the
    // no-argument case so every binding can forward its varargs verbatim.
    nlohmann::json callArgs = nlohmann::json::array();
    if (args.is_array()) {
        callArgs = args;
    } else if (!args.is_null()) {
        callArgs.push_back(args);
    }

    // CallFunctionResult, not CallMethod: a script's top-level function declares its
    // own parameters, so prepending an implicit `self` would shift every argument.
    return env->CallFunctionResult(method, callArgs);
}

bool ScriptComponent::LoadBaseScript() {
    if (m_ExtendsPath.empty()) {
        return true;  // No base script to load
    }

    std::string baseContent;
    auto& packFS = platform::PackFileSystem::Instance();

    // First try PackFileSystem (for exported games with .pck files)
    if (packFS.isPackMode()) {
        std::string resolvedPath = packFS.resolveAsset(m_ExtendsPath);
        if (packFS.exists(resolvedPath)) {
            baseContent = packFS.readFileAsString(resolvedPath);
            
        }
    }

    // If not loaded from pack, use Asset::ResolveAssetPath
    if (baseContent.empty()) {
        std::string basePhysicalPath;

        // If extends path starts with res:// or user://, resolve it using AssetDatabase/VFS
        if (m_ExtendsPath.find("res://") == 0 || m_ExtendsPath.find("user://") == 0) {
            basePhysicalPath = asset::Asset::ResolveAssetPath(m_ExtendsPath);
            if (basePhysicalPath.empty()) {
                LOG_ERROR(LogCategory::Core, "Failed to resolve base script path: {}", m_ExtendsPath);
                return false;
            }
        } else {
            // Resolve relative path based on current script path
            std::string currentPhysicalPath = asset::Asset::ResolveAssetPath(m_ScriptPath);
            if (currentPhysicalPath.empty()) {
                currentPhysicalPath = m_ScriptPath;
            }

            std::filesystem::path scriptDir = std::filesystem::path(currentPhysicalPath).parent_path();
            std::filesystem::path basePath = scriptDir / m_ExtendsPath;

            if (std::filesystem::exists(basePath)) {
                basePhysicalPath = basePath.string();
            } else if (std::filesystem::exists(m_ExtendsPath)) {
                basePhysicalPath = m_ExtendsPath;
            } else {
                LOG_ERROR(LogCategory::Core, "Base script not found: {}", m_ExtendsPath);
                return false;
            }
        }

        std::ifstream baseFile(basePhysicalPath);
        if (!baseFile.is_open()) {
            LOG_ERROR(LogCategory::Core, "Failed to open base script: {}", basePhysicalPath);
            return false;
        }

        std::stringstream buffer;
        buffer << baseFile.rdbuf();
        baseContent = buffer.str();
        baseFile.close();
    }

    if (baseContent.empty()) {
        LOG_ERROR(LogCategory::Core, "Base script is empty or could not be read: {}", m_ExtendsPath);
        return false;
    }

    // Parse export properties from base script too
    ParseExportProperties(baseContent);
    ParseToolDirective(baseContent);
    ParseSignals(baseContent);
    ParseRpcs(baseContent);
    ParseInterfaces(baseContent);

    auto* env = GetEnvironment();
    if (!env) {
        return false;
    }

    // Execute base script first
    auto result = env->ExecuteString(baseContent);
    if (!result) {
        return false;
    }

    return true;
}

bool ScriptComponent::LoadScript() {
    if (m_ScriptPath.empty()) {
        return false;
    }

    std::string scriptContent;
    auto& packFS = platform::PackFileSystem::Instance();

    // First try PackFileSystem (for exported games with .pck files)
    if (packFS.isPackMode()) {
        std::string resolvedPath = packFS.resolveAsset(m_ScriptPath);
        if (packFS.exists(resolvedPath)) {
            scriptContent = packFS.readFileAsString(resolvedPath);
            
        }
    }

    // If not loaded from pack, use Asset::ResolveAssetPath (handles AssetDatabase, VFS, etc.)
    if (scriptContent.empty()) {
        std::string physicalPath = asset::Asset::ResolveAssetPath(m_ScriptPath);

        if (physicalPath.empty()) {
            LOG_ERROR(LogCategory::Core, "Failed to resolve script path: {}", m_ScriptPath);
            return false;
        }

        std::ifstream file(physicalPath);
        if (!file.is_open()) {
            LOG_ERROR(LogCategory::Core, "Failed to open script file: {} (resolved: {})", m_ScriptPath, physicalPath);
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        scriptContent = buffer.str();
        file.close();

    }

    if (scriptContent.empty()) {
        LOG_ERROR(LogCategory::Core, "Script file is empty or could not be read: {}", m_ScriptPath);
        return false;
    }

    // Parse extends directive first
    ParseExtendsDirective(scriptContent);

    // Re-evaluate tool status from scratch on every (re)load, then parse the
    // --@tool / #@tool directive (base script may also set it in LoadBaseScript).
    m_IsTool = false;
    ParseToolDirective(scriptContent);

    // Parse export properties
    ParseExportProperties(scriptContent);

    // Parse @signal annotations and register them on this component.
    ParseSignals(scriptContent);

    // Parse @rpc annotations into the RPC config table.
    ParseRpcs(scriptContent);

    // Parse @interface annotations and register the interfaces' required signals.
    ParseInterfaces(scriptContent);

    auto* env = GetEnvironment();
    if (!env) {

        return false;
    }

    if (!env->Initialize()) {

        return false;
    }

    if (m_ScriptAPI && m_Owner) {
        m_ScriptAPI->SetOwner(m_Owner);
        m_ScriptAPI->SetEnvironment(env);

        if (auto* luaEnv = dynamic_cast<scripting::LuaEnvironment*>(env)) {
            luaEnv->SetScriptAPI(m_ScriptAPI.get());
        } else if (auto* mrubyEnv = dynamic_cast<scripting::MRubyEnvironment*>(env)) {
            mrubyEnv->SetScriptAPI(m_ScriptAPI.get());
        }
#ifdef LUPINE_HAS_MICROPYTHON
        else if (auto* micropythonEnv = dynamic_cast<scripting::MicroPythonEnvironment*>(env)) {
            micropythonEnv->SetScriptAPI(m_ScriptAPI.get());
        }
#endif
    }

    // Load base script first (if extends directive was found)
    if (!LoadBaseScript()) {
        // Base script loading failed, but we continue with the derived script
    }

    // Apply any pending export property values from deserialization
    // This must happen AFTER ParseExportProperties creates the property definitions
    if (!m_PendingExportProperties.empty()) {
        for (const auto& [key, value] : m_PendingExportProperties.items()) {
            if (m_CustomProperties.HasProperty(key)) {
                ComponentProperty* prop = m_CustomProperties.GetProperty(key);
                if (prop) {
                    prop->SetValueFromJson(value);
                }
            }
        }
        m_PendingExportProperties.clear();
    }

    // Execute the script first - this defines functions and sets default variable values
    auto result = env->ExecuteString(scriptContent);
    if (!result) {
        return false;
    }

    // Now set globals from CustomProperties AFTER script execution
    // This ensures inspector/serialized values override the script's default assignments
    for (const auto& prop : m_ExportProperties) {
        const ComponentProperty* compProp = m_CustomProperties.GetProperty(prop.name);
        if (!compProp) continue;
        PushExportGlobal(env, compProp);
    }

    m_ScriptLoaded = true;

    return true;
}

bool ScriptComponent::ReloadScript() {
    m_ScriptLoaded = false;

    // Clear the export properties from custom property registry
    // (keep only script_path which is a base property)
    for (const auto& prop : m_ExportProperties) {
        m_CustomProperties.RemoveProperty(prop.name);
    }
    m_ExportProperties.clear();

    // Also reset m_PropertiesDefined to allow DefineProperties to run again
    m_PropertiesDefined = false;

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
    // The script is loaded even in the editor (a non-tool/game script's body must
    // still execute so the editor can discover its @export properties); only the
    // gameplay callback below is gated.
    if (!m_ScriptLoaded) {
        LoadScript();
    }
    if (!ScriptCallbacksAllowed()) return;
    CallScriptFunction("on_awake");
}

void ScriptComponent::OnReady() {
    if (!ScriptCallbacksAllowed()) return;
    CallScriptFunction("on_ready");
}

void ScriptComponent::OnDestroy() {
    if (!ScriptCallbacksAllowed()) return;
    CallScriptFunction("on_destroy");
}

void ScriptComponent::OnInput(float deltaTime) {
    if (!ScriptCallbacksAllowed()) return;
    if (m_ScriptAPI) {
        m_ScriptAPI->SetDeltaTime(deltaTime);
    }
    CallScriptFunctionWithDelta("on_input", deltaTime);
}

void ScriptComponent::OnInputEvent(const nlohmann::json& event) {
    if (!m_ScriptLoaded || !ScriptCallbacksAllowed()) {
        return;
    }

    auto* env = GetEnvironment();
    if (!env || !env->HasFunction("on_input_event")) {
        return;
    }

    nlohmann::json args = nlohmann::json::array();
    args.push_back(event);
    env->CallFunctionArgs("on_input_event", args);
}

void ScriptComponent::OnProcess(float deltaTime) {
    if (!ScriptCallbacksAllowed()) return;
    if (m_ScriptAPI) {
        m_ScriptAPI->SetDeltaTime(deltaTime);
    }

    auto* env = GetEnvironment();
    if (env) {
        for (const auto& prop : m_ExportProperties) {
            ComponentProperty* compProp = m_CustomProperties.GetProperty(prop.name);
            if (!compProp) continue;
            PullExportGlobal(env, compProp);
        }
    }

    CallScriptFunctionWithDelta("on_process", deltaTime);

    // Drive the environment's coroutine/await scheduler (Lua); no-op for
    // environments without one.
    if (auto* updateEnv = GetEnvironment()) {
        updateEnv->Update(deltaTime);
    }

    // Deliver completion callbacks for any async archetype loads that finished
    // this frame, while this node's script environment is the active context.
    if (m_ScriptAPI) {
        m_ScriptAPI->DispatchCompletedAsyncLoads();
    }
}

void ScriptComponent::OnPhysicsProcess(float deltaTime) {
    if (!ScriptCallbacksAllowed()) return;
    if (m_ScriptAPI) {
        m_ScriptAPI->SetDeltaTime(deltaTime);
    }
    CallScriptFunctionWithDelta("on_physics_process", deltaTime);
}

void ScriptComponent::OnRender() {
    // Intentionally NOT gated by ScriptCallbacksAllowed: on_render is the
    // editor-time gizmo hook (scripts draw with editor_draw_* here), so it must
    // run for game scripts while a scene is open in the editor.
    CallScriptFunction("on_render");
}

namespace {
bool ParseVec3Json(const nlohmann::json& v, math::Vec3& out) {
    if (v.is_array() && v.size() >= 3) {
        out = math::Vec3(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
        return true;
    }
    if (v.is_array() && v.size() == 2) {
        out = math::Vec3(v[0].get<float>(), v[1].get<float>(), 0.0f);
        return true;
    }
    if (v.is_object()) {
        out = math::Vec3(v.value("x", 0.0f), v.value("y", 0.0f), v.value("z", 0.0f));
        return true;
    }
    return false;
}
} // namespace

bool ParseScriptRenderBounds(const nlohmann::json& value, AABB& outBounds) {
    if (value.is_null()) return false;

    if (value.is_array() && value.size() >= 6) {
        outBounds = AABB(
            math::Vec3(value[0].get<float>(), value[1].get<float>(), value[2].get<float>()),
            math::Vec3(value[3].get<float>(), value[4].get<float>(), value[5].get<float>()));
        return true;
    }

    if (value.is_object()) {
        math::Vec3 a, b;
        if (value.contains("min") && value.contains("max") &&
            ParseVec3Json(value["min"], a) && ParseVec3Json(value["max"], b)) {
            outBounds = AABB(a, b);
            return true;
        }
        if (value.contains("center") && value.contains("size") &&
            ParseVec3Json(value["center"], a) && ParseVec3Json(value["size"], b)) {
            math::Vec3 half(0.5f * b.x, 0.5f * b.y, 0.5f * b.z);
            outBounds = AABB(a - half, a + half);
            return true;
        }
    }
    return false;
}

void ScriptComponent::buildDrawCommands(RenderContext& ctx) {
    if (!m_ScriptLoaded || !m_ScriptAPI) return;
    auto* env = GetEnvironment();
    if (!env) return;

    const char* hook = env->HasFunction("on_draw_override") ? "on_draw_override"
                     : (env->HasFunction("on_draw") ? "on_draw" : nullptr);
    if (!hook) return;

    m_ScriptAPI->SetRenderContext(&ctx);
    CallScriptFunction(hook);
    m_ScriptAPI->SetRenderContext(nullptr);
}

AABB ScriptComponent::getWorldBounds() const {
    auto* env = const_cast<ScriptComponent*>(this)->GetEnvironment();

    // Optional get_render_bounds() callback takes precedence. Its result is cached
    // and only refreshed when the global render epoch advances, so frustum culling
    // never calls into the script VM more than once per change.
    if (env && m_ScriptLoaded && env->HasFunction("get_render_bounds")) {
        uint64_t epoch = Node::GetRenderEpoch();
        if (epoch != m_RenderBoundsEpoch) {
            m_RenderBoundsEpoch = epoch;
            nlohmann::json result = env->CallMethod("get_render_bounds",
                nlohmann::json::object(), nlohmann::json::array());
            AABB parsed;
            m_RenderBoundsValid = ParseScriptRenderBounds(result, parsed);
            if (m_RenderBoundsValid) {
                m_RenderBoundsCache = parsed;
            }
        }
        if (m_RenderBoundsValid) {
            return m_RenderBoundsCache;
        }
    }

    const bool draws = env && (env->HasFunction("on_draw") || env->HasFunction("on_draw_override"));
    if (!draws) {
        // No on_draw and no usable bounds callback: contributes no scene geometry.
        // Report an empty box so culling and the editor selection outline ignore it.
        return AABB(math::Vec3(0.0f, 0.0f, 0.0f), math::Vec3(0.0f, 0.0f, 0.0f));
    }

    Node* owner = GetOwner();
    if (auto* node3D = dynamic_cast<Node3D*>(owner)) {
        math::Vec3 p = node3D->GetGlobalPosition();
        math::Vec3 s = node3D->GetGlobalScale();
        math::Vec3 half(0.5f * std::fabs(s.x), 0.5f * std::fabs(s.y), 0.5f * std::fabs(s.z));
        return AABB(p - half, p + half);
    }
    if (auto* node2D = dynamic_cast<Node2D*>(owner)) {
        math::Vec2 p = node2D->GetGlobalPosition();
        math::Vec2 s = node2D->GetGlobalScale();
        float hx = 32.0f * std::fabs(s.x);
        float hy = 32.0f * std::fabs(s.y);
        return AABB(math::Vec3(p.x - hx, p.y - hy, 0.0f),
                    math::Vec3(p.x + hx, p.y + hy, 0.0f));
    }
    return AABB(math::Vec3(0.0f, 0.0f, 0.0f), math::Vec3(0.0f, 0.0f, 0.0f));
}

SpatialType ScriptComponent::getSpatialType() const {
    Node* owner = GetOwner();
    if (dynamic_cast<Node3D*>(owner)) {
        return SpatialType::World3D;
    }
    if (dynamic_cast<Node2D*>(owner)) {
        return SpatialType::World2D;
    }
    return SpatialType::World3D;
}

bool ScriptComponent::isRenderContentDynamic() const {
    auto* env = const_cast<ScriptComponent*>(this)->GetEnvironment();
    return env && (env->HasFunction("on_draw") || env->HasFunction("on_draw_override"));
}

void ScriptComponent::OnEnterTree() {
    if (!ScriptCallbacksAllowed()) return;
    CallScriptFunction("on_enter_tree");
}

void ScriptComponent::OnExitTree() {
    if (!ScriptCallbacksAllowed()) return;
    CallScriptFunction("on_exit_tree");
}

void ScriptComponent::OnVisibilityChanged(bool visible) {
    if (!ScriptCallbacksAllowed()) return;
    auto* env = GetEnvironment();
    if (!m_ScriptLoaded || !env) return;

    env->SetGlobal("visible", visible);
    CallScriptFunction("on_visibility_changed");
}

void ScriptComponent::OnUnhandledInput(float deltaTime) {
    if (!ScriptCallbacksAllowed()) return;
    if (m_ScriptAPI) {
        m_ScriptAPI->SetDeltaTime(deltaTime);
    }
    CallScriptFunctionWithDelta("on_unhandled_input", deltaTime);
}

void ScriptComponent::OnLateUpdate(float deltaTime) {
    if (!ScriptCallbacksAllowed()) return;
    if (m_ScriptAPI) {
        m_ScriptAPI->SetDeltaTime(deltaTime);
    }
    CallScriptFunctionWithDelta("on_late_update", deltaTime);
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
        LOG_ERROR(LogCategory::Scripting, "Script function '{}' failed: {}", functionName, result.error);
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
        PushExportGlobal(env, compProp);
    }

    auto result = env->CallFunction(functionName);
    if (!result) {
        LOG_ERROR(LogCategory::Scripting, "Script function '{}' failed: {}", functionName, result.error);
        return false;
    }

    return true;
}

bool ScriptComponent::InvokeSignalMethod(const std::string& method, const SignalArgs& args) {
    if (!m_ScriptLoaded || !ScriptCallbacksAllowed()) {
        return false;
    }

    auto* env = GetEnvironment();
    if (!env || !env->HasFunction(method)) {
        return false;
    }

    nlohmann::json argArray = nlohmann::json::array();
    for (const nlohmann::json& arg : args) {
        argArray.push_back(arg);
    }

    return env->CallFunctionArgs(method, argArray);
}

#ifdef LUPINE_HAS_MICROPYTHON

PythonScriptComponent::PythonScriptComponent()
    : ScriptComponent("PythonScriptComponent"),
      m_MicroPythonEnv(std::make_unique<scripting::MicroPythonEnvironment>()) {
}

PythonScriptComponent::PythonScriptComponent(const std::string& name)
    : ScriptComponent(name),
      m_MicroPythonEnv(std::make_unique<scripting::MicroPythonEnvironment>()) {
}

PythonScriptComponent::~PythonScriptComponent() {
}

void PythonScriptComponent::ParseExportProperties(const std::string& scriptContent) {
    ParseExportsWithPrefix(scriptContent, "#");
}

#endif // LUPINE_HAS_MICROPYTHON

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
    ParseExportsWithPrefix(scriptContent, "--");
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
    ParseExportsWithPrefix(scriptContent, "#");
}

#endif

}
}
