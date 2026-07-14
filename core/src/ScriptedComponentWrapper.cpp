#include "lupine/core/ScriptedComponentWrapper.hpp"
#include "lupine/core/CustomComponentRegistry.hpp"
#include "lupine/core/Serialization.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/asset/Asset.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/logger/Logger.hpp"
#include <fstream>
#include <sstream>
#include <cmath>

namespace lupine {
namespace core {

ScriptedComponentWrapper::ScriptedComponentWrapper()
    : Component("ScriptedComponentWrapper"),
      m_ScriptAPI(std::make_unique<scripting::ScriptAPI>()) {
}

ScriptedComponentWrapper::ScriptedComponentWrapper(const std::string& customTypeName)
    : Component(customTypeName),
      m_CustomTypeName(customTypeName),
      m_ScriptAPI(std::make_unique<scripting::ScriptAPI>()) {

    // Get the definition from the registry
    const auto* def = CustomComponentRegistry::GetInstance().GetDefinition(customTypeName);
    if (def && def->isValid) {
        m_ScriptPath = def->scriptPath;
        m_ExportProperties = def->exportProperties;
        m_OverrideHooks = def->overrideHooks;
        m_DefinedHooks = def->definedHooks;
        m_IsTool = def->isTool;

        // Initialize base component if specified
        InitializeBaseComponent(*def);

        // Initialize script environment based on language
        InitializeScriptEnvironment(def->language);
    } else {
        LOG_ERROR(LogCategory::Core, "ScriptedComponentWrapper: No definition found for type '{}'",
                  customTypeName);
    }
}

ScriptedComponentWrapper::~ScriptedComponentWrapper() {
    if (m_ScriptEnv) {
        m_ScriptEnv->Shutdown();
    }
}

void ScriptedComponentWrapper::InitializeBaseComponent(const CustomComponentDefinition& def) {
    if (def.baseComponentType.empty()) {
        return;  // No base component
    }

    // Create base component using TypeRegistry
    auto instance = TypeRegistry::GetInstance().CreateInstance(def.baseComponentType);
    if (!instance) {
        LOG_ERROR(LogCategory::Core,
            "ScriptedComponentWrapper: Failed to create base component '{}' for '{}'",
            def.baseComponentType, def.className);
        return;
    }

    m_BaseComponent = std::dynamic_pointer_cast<Component>(instance);
    if (!m_BaseComponent) {
        LOG_ERROR(LogCategory::Core,
            "ScriptedComponentWrapper: Base type '{}' is not a Component for '{}'",
            def.baseComponentType, def.className);
        return;
    }

    // Initialize base component properties
    m_BaseComponent->RegisterProperties();

    LOG_DEBUG(LogCategory::Core, "ScriptedComponentWrapper: Created base component '{}' for '{}'",
              def.baseComponentType, def.className);
}

void ScriptedComponentWrapper::InitializeScriptEnvironment(scripting::ScriptLanguage language) {
    switch (language) {
        case scripting::ScriptLanguage::Lua:
            m_ScriptEnv = std::make_unique<scripting::LuaEnvironment>();
            break;
#ifdef LUPINE_HAS_MRUBY
        case scripting::ScriptLanguage::MRuby:
            m_ScriptEnv = std::make_unique<scripting::MRubyEnvironment>();
            break;
#endif
#ifdef LUPINE_HAS_MICROPYTHON
        case scripting::ScriptLanguage::MicroPython:
        case scripting::ScriptLanguage::Python:
            m_ScriptEnv = std::make_unique<scripting::MicroPythonEnvironment>();
            break;
#endif
        default:
            LOG_ERROR(LogCategory::Core,
                "ScriptedComponentWrapper: Unsupported script language for '{}'", m_CustomTypeName);
            break;
    }
}

const CustomComponentDefinition* ScriptedComponentWrapper::GetDefinition() const {
    return CustomComponentRegistry::GetInstance().GetDefinition(m_CustomTypeName);
}

void ScriptedComponentWrapper::RegisterProperties() {
    Component::RegisterProperties();

    // Register script_path property for serialization
    PropertyDescriptor scriptPathDesc("script_path", PropertyValueType::String);
    scriptPathDesc.defaultValue = std::string("");
    scriptPathDesc.hint = PropertyHint(PropertyHintType::File, "*.lua,*.py,*.rb");
    scriptPathDesc.description = "Path to the custom component script file";

    RegisterPropertyWithMetadata<std::string>("script_path", PropertyType::String,
        [this]() { return m_ScriptPath; },
        [this](const std::string&) { /* Read-only for custom components */ },
        scriptPathDesc);
}

void ScriptedComponentWrapper::DefineProperties() {
    // Merge properties from base component and script
    MergeProperties();
}

void ScriptedComponentWrapper::MergeProperties() {
    // First, add all base component properties
    if (m_BaseComponent) {
        // Ensure base component properties are defined
        if (!m_BaseComponent->GetPropertyRegistry().GetProperties().empty() == false) {
            m_BaseComponent->DefineProperties();
        }

        // Copy property descriptors from base component
        auto baseDescriptors = m_BaseComponent->GetPropertyRegistry().GetPropertyDescriptors();
        for (const auto& desc : baseDescriptors) {
            // Skip if we already have this property (script overrides base)
            if (m_CustomProperties.HasProperty(desc.name)) {
                continue;
            }
            DefineProperty(desc);
        }
    }

    // Then add script export properties (these can override base properties)
    for (const auto& exportProp : m_ExportProperties) {
        DefineProperty(exportProp.descriptor);
    }
}

nlohmann::json ScriptedComponentWrapper::Serialize() const {
    nlohmann::json json = Component::Serialize();
    json["custom_type"] = m_CustomTypeName;
    json["script_path"] = m_ScriptPath;

    // Serialize base component properties separately
    if (m_BaseComponent) {
        json["base_properties"] = m_BaseComponent->Serialize();
    }

    // Serialize export properties
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

void ScriptedComponentWrapper::Deserialize(const nlohmann::json& json) {
    Component::Deserialize(json);

    if (json.contains("custom_type")) {
        m_CustomTypeName = json["custom_type"].get<std::string>();
    }

    if (json.contains("script_path")) {
        m_ScriptPath = json["script_path"].get<std::string>();
    }

    // Store base properties for later application
    if (json.contains("base_properties")) {
        m_PendingBaseProperties = json["base_properties"];
    }

    // Store export properties for later application
    if (json.contains("export_properties")) {
        m_PendingExportProperties = json["export_properties"];
    }
}

bool ScriptedComponentWrapper::LoadScript() {
    if (m_ScriptPath.empty()) {
        LOG_ERROR(LogCategory::Core, "ScriptedComponentWrapper: No script path for '{}'", m_CustomTypeName);
        return false;
    }

    if (!m_ScriptEnv) {
        LOG_ERROR(LogCategory::Core, "ScriptedComponentWrapper: No script environment for '{}'",
                  m_CustomTypeName);
        return false;
    }

    // Initialize the script environment
    if (!m_ScriptEnv->Initialize()) {
        LOG_ERROR(LogCategory::Core, "ScriptedComponentWrapper: Failed to initialize script environment for '{}'",
                  m_CustomTypeName);
        return false;
    }

    // Make every known custom component class resolvable by name inside the VM before
    // this script runs. Built-in bases are registered once when the VM starts, but a
    // custom base only exists after the project has been scanned - so without this a
    // script that inherits from another custom component (SimBoulder from SimObject)
    // fails on its very first line: the base name is simply not defined in the VM.
    m_ScriptEnv->RegisterCustomComponentTypes();

    // Set the ScriptAPI owner to our owner node
    m_ScriptAPI->SetOwner(m_Owner);

    // Register ScriptAPI with the environment
    if (auto* luaEnv = dynamic_cast<scripting::LuaEnvironment*>(m_ScriptEnv.get())) {
        luaEnv->SetScriptAPI(m_ScriptAPI.get());
    }
#ifdef LUPINE_HAS_MRUBY
    else if (auto* mrubyEnv = dynamic_cast<scripting::MRubyEnvironment*>(m_ScriptEnv.get())) {
        mrubyEnv->SetScriptAPI(m_ScriptAPI.get());
    }
#endif
#ifdef LUPINE_HAS_MICROPYTHON
    else if (auto* mpyEnv = dynamic_cast<scripting::MicroPythonEnvironment*>(m_ScriptEnv.get())) {
        mpyEnv->SetScriptAPI(m_ScriptAPI.get());
    }
#endif

    // Read script content
    std::string scriptContent;
    auto& packFS = platform::PackFileSystem::Instance();

    // Try PackFileSystem first (for exported games)
    if (packFS.isPackMode()) {
        std::string resolvedPath = packFS.resolveAsset(m_ScriptPath);
        if (packFS.exists(resolvedPath)) {
            scriptContent = packFS.readFileAsString(resolvedPath);
        }
    }

    // If not from pack, read from file system
    if (scriptContent.empty()) {
        std::string physicalPath = asset::Asset::ResolveAssetPath(m_ScriptPath);
        if (physicalPath.empty()) {
            LOG_ERROR(LogCategory::Core, "ScriptedComponentWrapper: Failed to resolve script path '{}'",
                      m_ScriptPath);
            return false;
        }

        std::ifstream file(physicalPath);
        if (!file.is_open()) {
            LOG_ERROR(LogCategory::Core, "ScriptedComponentWrapper: Failed to open script file '{}'",
                      physicalPath);
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        scriptContent = buffer.str();
        file.close();
    }

    // Execute the script
    auto result = m_ScriptEnv->ExecuteString(scriptContent);
    if (!result) {
        LOG_ERROR(LogCategory::Core, "ScriptedComponentWrapper: Failed to execute script '{}': {}",
                  m_ScriptPath, result.error);
        return false;
    }

    m_ScriptLoaded = true;

    // Apply pending property values
    ApplyPendingPropertyValues();

    // Sync current property values to script
    SyncExportPropertiesToScript();

    LOG_DEBUG(LogCategory::Core, "ScriptedComponentWrapper: Loaded script '{}' for '{}'",
              m_ScriptPath, m_CustomTypeName);

    return true;
}

bool ScriptedComponentWrapper::ReloadScript() {
    // Store current property values
    nlohmann::json savedExportProps;
    for (const auto& prop : m_ExportProperties) {
        if (m_CustomProperties.HasProperty(prop.name)) {
            const ComponentProperty* compProp = m_CustomProperties.GetProperty(prop.name);
            if (compProp) {
                savedExportProps[prop.name] = compProp->GetValueAsJson();
            }
        }
    }

    // Shutdown existing environment
    if (m_ScriptEnv) {
        m_ScriptEnv->Shutdown();
    }

    m_ScriptLoaded = false;

    // Re-get definition (it may have changed)
    const auto* def = CustomComponentRegistry::GetInstance().GetDefinition(m_CustomTypeName);
    if (!def || !def->isValid) {
        LOG_ERROR(LogCategory::Core, "ScriptedComponentWrapper: Definition no longer valid for '{}'",
                  m_CustomTypeName);
        return false;
    }

    // Update from new definition
    m_ExportProperties = def->exportProperties;
    m_OverrideHooks = def->overrideHooks;
    m_DefinedHooks = def->definedHooks;
    m_IsTool = def->isTool;

    // Re-initialize script environment
    InitializeScriptEnvironment(def->language);

    // Re-merge properties
    MergeProperties();

    // Restore saved property values
    m_PendingExportProperties = savedExportProps;

    // Load script
    return LoadScript();
}

void ScriptedComponentWrapper::ApplyPendingPropertyValues() {
    // Apply base component properties
    if (m_BaseComponent && !m_PendingBaseProperties.empty()) {
        m_BaseComponent->Deserialize(m_PendingBaseProperties);
        m_PendingBaseProperties.clear();
    }

    // Apply export property values
    if (!m_PendingExportProperties.empty()) {
        for (const auto& [name, value] : m_PendingExportProperties.items()) {
            ComponentProperty* prop = m_CustomProperties.GetProperty(name);
            if (prop) {
                prop->SetValueFromJson(value);
            }
        }
        m_PendingExportProperties.clear();
    }
}

void ScriptedComponentWrapper::SyncExportPropertiesToScript() {
    if (!m_ScriptEnv || !m_ScriptLoaded) return;

    for (const auto& prop : m_ExportProperties) {
        const ComponentProperty* compProp = m_CustomProperties.GetProperty(prop.name);
        if (!compProp) continue;

        switch (prop.descriptor.type) {
            case PropertyValueType::Int:
            case PropertyValueType::Enum:
                m_ScriptEnv->SetGlobal(prop.name, compProp->GetValue<int>());
                break;
            case PropertyValueType::Float:
                m_ScriptEnv->SetGlobal(prop.name, compProp->GetValue<float>());
                break;
            case PropertyValueType::String:
                m_ScriptEnv->SetGlobal(prop.name, compProp->GetValue<std::string>());
                break;
            case PropertyValueType::Bool:
                m_ScriptEnv->SetGlobal(prop.name, compProp->GetValue<bool>());
                break;
            default:
                break;
        }
    }
}

bool ScriptedComponentWrapper::ScriptCallbacksAllowed() const {
    if (m_IsTool) {
        return true;
    }
    const Node* owner = GetOwner();
    const Scene* scene = owner ? owner->GetScene() : nullptr;
    return !(scene && scene->IsInEditor());
}

bool ScriptedComponentWrapper::HasOverride(const std::string& hookName) const {
    return m_OverrideHooks.find(hookName + "_override") != m_OverrideHooks.end();
}

bool ScriptedComponentWrapper::HasHook(const std::string& hookName) const {
    return m_DefinedHooks.find(hookName) != m_DefinedHooks.end();
}

bool ScriptedComponentWrapper::CallScriptFunction(const std::string& name) {
    if (!m_ScriptLoaded || !m_ScriptEnv) return false;
    if (!m_ScriptEnv->HasFunction(name)) return true;  // Not an error if function doesn't exist

    // Sync export properties before calling
    SyncExportPropertiesToScript();

    auto result = m_ScriptEnv->CallFunction(name);
    if (!result) {
        LOG_ERROR(LogCategory::Scripting, "ScriptedComponentWrapper: Script function '{}' failed: {}",
                  name, result.error);
        return false;
    }

    return true;
}

bool ScriptedComponentWrapper::CallScriptFunctionWithDelta(const std::string& name, float deltaTime) {
    if (!m_ScriptLoaded || !m_ScriptEnv) return false;
    if (!m_ScriptEnv->HasFunction(name)) return true;

    // Set delta_time global
    m_ScriptEnv->SetGlobal("delta_time", deltaTime);

    // Sync export properties before calling
    SyncExportPropertiesToScript();

    auto result = m_ScriptEnv->CallFunction(name);
    if (!result) {
        LOG_ERROR(LogCategory::Scripting, "ScriptedComponentWrapper: Script function '{}' failed: {}",
                  name, result.error);
        return false;
    }

    return true;
}

// Lifecycle hooks implementation with dual model

void ScriptedComponentWrapper::OnAwake() {
    // Load script on awake
    if (!m_ScriptLoaded) {
        LoadScript();
    }

    // Update ScriptAPI owner
    m_ScriptAPI->SetOwner(m_Owner);

    // Set owner for base component too
    if (m_BaseComponent) {
        m_BaseComponent->SetOwner(m_Owner);
    }

    // In the editor a game component still runs its base built-in lifecycle (so
    // the built-in initializes/renders as before) but skips its own script hooks.
    if (!ScriptCallbacksAllowed()) {
        if (m_BaseComponent) m_BaseComponent->OnAwake();
        return;
    }

    if (HasOverride("on_awake")) {
        CallScriptFunction("on_awake_override");
    } else {
        if (m_BaseComponent) {
            m_BaseComponent->OnAwake();
        }
        CallScriptFunction("on_awake");
    }
}

void ScriptedComponentWrapper::OnReady() {
    if (!ScriptCallbacksAllowed()) {
        if (m_BaseComponent) m_BaseComponent->OnReady();
        return;
    }
    if (HasOverride("on_ready")) {
        CallScriptFunction("on_ready_override");
    } else {
        if (m_BaseComponent) {
            m_BaseComponent->OnReady();
        }
        CallScriptFunction("on_ready");
    }
}

void ScriptedComponentWrapper::OnDestroy() {
    // The wrapped base component is destroyed through DispatchDestroy() so that it, too,
    // tears down exactly once - the wrapper's own OnDestroy is already guarded by the
    // DispatchDestroy() call that reached it.
    if (!ScriptCallbacksAllowed()) {
        if (m_BaseComponent) m_BaseComponent->DispatchDestroy();
        return;
    }
    if (HasOverride("on_destroy")) {
        CallScriptFunction("on_destroy_override");
    } else {
        CallScriptFunction("on_destroy");
        if (m_BaseComponent) {
            m_BaseComponent->DispatchDestroy();
        }
    }
}

void ScriptedComponentWrapper::OnUpdate(float deltaTime) {
    if (!ScriptCallbacksAllowed()) {
        if (m_BaseComponent) m_BaseComponent->OnUpdate(deltaTime);
        return;
    }
    if (HasOverride("on_update")) {
        CallScriptFunctionWithDelta("on_update_override", deltaTime);
    } else {
        if (m_BaseComponent) {
            m_BaseComponent->OnUpdate(deltaTime);
        }
        CallScriptFunctionWithDelta("on_update", deltaTime);
    }
}

void ScriptedComponentWrapper::OnInput(float deltaTime) {
    if (!ScriptCallbacksAllowed()) {
        if (m_BaseComponent) m_BaseComponent->OnInput(deltaTime);
        return;
    }
    if (HasOverride("on_input")) {
        CallScriptFunctionWithDelta("on_input_override", deltaTime);
    } else {
        if (m_BaseComponent) {
            m_BaseComponent->OnInput(deltaTime);
        }
        CallScriptFunctionWithDelta("on_input", deltaTime);
    }
}

void ScriptedComponentWrapper::OnInputEvent(const nlohmann::json& event) {
    if (!ScriptCallbacksAllowed()) {
        if (m_BaseComponent) m_BaseComponent->OnInputEvent(event);
        return;
    }
    auto dispatch = [&](const std::string& name) {
        if (!m_ScriptLoaded || !m_ScriptEnv) return;
        if (!m_ScriptEnv->HasFunction(name)) return;
        SyncExportPropertiesToScript();
        nlohmann::json args = nlohmann::json::array();
        args.push_back(event);
        m_ScriptEnv->CallFunctionArgs(name, args);
    };

    if (HasOverride("on_input_event")) {
        dispatch("on_input_event_override");
    } else {
        if (m_BaseComponent) {
            m_BaseComponent->OnInputEvent(event);
        }
        dispatch("on_input_event");
    }
}

void ScriptedComponentWrapper::OnUnhandledInput(float deltaTime) {
    if (!ScriptCallbacksAllowed()) {
        if (m_BaseComponent) m_BaseComponent->OnUnhandledInput(deltaTime);
        return;
    }
    if (HasOverride("on_unhandled_input")) {
        CallScriptFunctionWithDelta("on_unhandled_input_override", deltaTime);
    } else {
        if (m_BaseComponent) {
            m_BaseComponent->OnUnhandledInput(deltaTime);
        }
        CallScriptFunctionWithDelta("on_unhandled_input", deltaTime);
    }
}

void ScriptedComponentWrapper::OnProcess(float deltaTime) {
    if (!ScriptCallbacksAllowed()) {
        if (m_BaseComponent) m_BaseComponent->OnProcess(deltaTime);
        return;
    }
    if (HasOverride("on_process")) {
        CallScriptFunctionWithDelta("on_process_override", deltaTime);
    } else {
        if (m_BaseComponent) {
            m_BaseComponent->OnProcess(deltaTime);
        }
        CallScriptFunctionWithDelta("on_process", deltaTime);
    }
}

void ScriptedComponentWrapper::OnPhysicsProcess(float deltaTime) {
    if (!ScriptCallbacksAllowed()) {
        if (m_BaseComponent) m_BaseComponent->OnPhysicsProcess(deltaTime);
        return;
    }
    if (HasOverride("on_physics_process")) {
        CallScriptFunctionWithDelta("on_physics_process_override", deltaTime);
    } else {
        if (m_BaseComponent) {
            m_BaseComponent->OnPhysicsProcess(deltaTime);
        }
        CallScriptFunctionWithDelta("on_physics_process", deltaTime);
    }
}

void ScriptedComponentWrapper::OnRender() {
    if (HasOverride("on_render")) {
        CallScriptFunction("on_render_override");
    } else {
        if (m_BaseComponent) {
            m_BaseComponent->OnRender();
        }
        CallScriptFunction("on_render");
    }
}

void ScriptedComponentWrapper::OnLateUpdate(float deltaTime) {
    if (!ScriptCallbacksAllowed()) {
        if (m_BaseComponent) m_BaseComponent->OnLateUpdate(deltaTime);
        return;
    }
    if (HasOverride("on_late_update")) {
        CallScriptFunctionWithDelta("on_late_update_override", deltaTime);
    } else {
        if (m_BaseComponent) {
            m_BaseComponent->OnLateUpdate(deltaTime);
        }
        CallScriptFunctionWithDelta("on_late_update", deltaTime);
    }
}

void ScriptedComponentWrapper::OnEnterTree() {
    if (!ScriptCallbacksAllowed()) {
        if (m_BaseComponent) m_BaseComponent->OnEnterTree();
        return;
    }
    if (HasOverride("on_enter_tree")) {
        CallScriptFunction("on_enter_tree_override");
    } else {
        if (m_BaseComponent) {
            m_BaseComponent->OnEnterTree();
        }
        CallScriptFunction("on_enter_tree");
    }
}

void ScriptedComponentWrapper::OnExitTree() {
    if (!ScriptCallbacksAllowed()) {
        if (m_BaseComponent) m_BaseComponent->OnExitTree();
        return;
    }
    if (HasOverride("on_exit_tree")) {
        CallScriptFunction("on_exit_tree_override");
    } else {
        CallScriptFunction("on_exit_tree");
        if (m_BaseComponent) {
            m_BaseComponent->OnExitTree();
        }
    }
}

void ScriptedComponentWrapper::OnVisibilityChanged(bool visible) {
    if (!ScriptCallbacksAllowed()) {
        if (m_BaseComponent) m_BaseComponent->OnVisibilityChanged(visible);
        return;
    }
    if (m_ScriptEnv && m_ScriptLoaded) {
        m_ScriptEnv->SetGlobal("visible", visible);
    }

    if (HasOverride("on_visibility_changed")) {
        CallScriptFunction("on_visibility_changed_override");
    } else {
        if (m_BaseComponent) {
            m_BaseComponent->OnVisibilityChanged(visible);
        }
        CallScriptFunction("on_visibility_changed");
    }
}

void ScriptedComponentWrapper::OnPropertyChanged(const std::string& propertyName,
                                                  const nlohmann::json& newValue) {
    // Forward to base component if it has this property
    if (m_BaseComponent) {
        auto& baseRegistry = m_BaseComponent->GetPropertyRegistry();
        if (baseRegistry.HasProperty(propertyName)) {
            ComponentProperty* baseProp = baseRegistry.GetProperty(propertyName);
            if (baseProp) {
                baseProp->SetValueFromJson(newValue);
                m_BaseComponent->OnPropertyChanged(propertyName, newValue);
            }
        }
    }

    // Update script environment if it's an export property
    if (m_ScriptEnv && m_ScriptLoaded) {
        for (const auto& prop : m_ExportProperties) {
            if (prop.name == propertyName) {
                switch (prop.descriptor.type) {
                    case PropertyValueType::Int:
                    case PropertyValueType::Enum:
                        if (newValue.is_number_integer()) {
                            m_ScriptEnv->SetGlobal(propertyName, newValue.get<int>());
                        } else if (newValue.is_number()) {
                            m_ScriptEnv->SetGlobal(propertyName, static_cast<int>(newValue.get<double>()));
                        }
                        break;
                    case PropertyValueType::Float:
                        if (newValue.is_number()) {
                            m_ScriptEnv->SetGlobal(propertyName, newValue.get<float>());
                        }
                        break;
                    case PropertyValueType::String:
                        if (newValue.is_string()) {
                            m_ScriptEnv->SetGlobal(propertyName, newValue.get<std::string>());
                        }
                        break;
                    case PropertyValueType::Bool:
                        if (newValue.is_boolean()) {
                            m_ScriptEnv->SetGlobal(propertyName, newValue.get<bool>());
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

bool ScriptedComponentWrapper::OnAssetFileChanged(const std::string& changedPath,
                                                   const std::string& resolvedChangedPath) {
    bool affected = false;

    // Check if our script file changed
    if (changedPath == m_ScriptPath || resolvedChangedPath == m_ScriptPath) {
        ReloadScript();
        affected = true;
    }

    // Forward to base component
    if (m_BaseComponent) {
        if (m_BaseComponent->OnAssetFileChanged(changedPath, resolvedChangedPath)) {
            affected = true;
        }
    }

    return affected;
}

void ScriptedComponentWrapper::OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) {
    // A nested wrapper forwards again, so this walks the whole inheritance chain down to the
    // built-in physics component that actually owns the body.
    if (m_BaseComponent) {
        m_BaseComponent->OnPhysicsWorldRebuild(phase);
    }
}

bool ScriptedComponentWrapper::OnGizmoScale(float scaleDelta, int axis, bool is3D) {
    // Forward to base component
    if (m_BaseComponent) {
        return m_BaseComponent->OnGizmoScale(scaleDelta, axis, is3D);
    }
    return false;
}

std::vector<std::string> ScriptedComponentWrapper::GetTypeChain() const {
    std::vector<std::string> chain;
    chain.push_back(m_CustomTypeName);
    if (m_BaseComponent) {
        std::vector<std::string> baseChain = m_BaseComponent->GetTypeChain();
        chain.insert(chain.end(), baseChain.begin(), baseChain.end());
    }
    return chain;
}

IRenderableComponent* ScriptedComponentWrapper::GetRenderableBase() const {
    return dynamic_cast<IRenderableComponent*>(m_BaseComponent.get());
}

void ScriptedComponentWrapper::EnsureBaseOwner() const {
    if (m_BaseComponent && m_BaseComponent->GetOwner() != GetOwner()) {
        m_BaseComponent->SetOwner(GetOwner());
    }
}

void ScriptedComponentWrapper::buildDrawCommands(RenderContext& ctx) {
    EnsureBaseOwner();

    const bool drawOverride = HasOverride("on_draw");

    // Extend mode: draw the base component first - for a nested wrapper this
    // recurses down the inheritance chain to the built-in renderable - then let
    // the script draw on top. Override mode: the script replaces base drawing.
    if (!drawOverride) {
        if (IRenderableComponent* base = GetRenderableBase()) {
            base->buildDrawCommands(ctx);
        }
    }

    if (m_ScriptLoaded && m_ScriptEnv && m_ScriptAPI) {
        const char* hook = drawOverride ? "on_draw_override" : "on_draw";
        if (m_ScriptEnv->HasFunction(hook)) {
            m_ScriptAPI->SetRenderContext(&ctx);
            CallScriptFunction(hook);
            m_ScriptAPI->SetRenderContext(nullptr);
        }
    }
}

AABB ScriptedComponentWrapper::getWorldBounds() const {
    EnsureBaseOwner();

    // A get_render_bounds() callback in the script overrides everything (cached by
    // render epoch so culling does not re-enter the VM each frame).
    if (m_ScriptEnv && m_ScriptLoaded && m_ScriptEnv->HasFunction("get_render_bounds")) {
        uint64_t epoch = Node::GetRenderEpoch();
        if (epoch != m_RenderBoundsEpoch) {
            m_RenderBoundsEpoch = epoch;
            nlohmann::json result = m_ScriptEnv->CallMethod("get_render_bounds",
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

    if (IRenderableComponent* base = GetRenderableBase()) {
        return base->getWorldBounds();
    }

    // No renderable base (a pure on_draw custom component): derive a bounds box
    // from the owner node's global transform so frustum culling and the editor
    // selection outline have something to work with. Scaling the node scales
    // this box, which is what makes a scriptable debug rectangle resizable.
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

RenderLayer ScriptedComponentWrapper::getRenderLayer() const {
    if (IRenderableComponent* base = GetRenderableBase()) {
        return base->getRenderLayer();
    }
    return RenderLayer::Opaque;
}

SpatialType ScriptedComponentWrapper::getSpatialType() const {
    if (IRenderableComponent* base = GetRenderableBase()) {
        return base->getSpatialType();
    }
    Node* owner = GetOwner();
    if (dynamic_cast<Node3D*>(owner)) {
        return SpatialType::World3D;
    }
    if (dynamic_cast<Node2D*>(owner)) {
        return SpatialType::World2D;
    }
    return SpatialType::World3D;
}

math::OBB ScriptedComponentWrapper::getOrientedBounds() const {
    EnsureBaseOwner();
    if (IRenderableComponent* base = GetRenderableBase()) {
        return base->getOrientedBounds();
    }
    return math::OBB::FromAABB(getWorldBounds());
}

bool ScriptedComponentWrapper::isRenderContentDynamic() const {
    // A script that draws each frame (on_draw / on_draw_override) is dynamic, so
    // the renderer's static draw-list cache must not be used. Otherwise defer to
    // the base component (e.g. a StaticMesh3D base can still report itself static
    // and remain cacheable).
    if (m_ScriptEnv && m_ScriptLoaded &&
        (m_DefinedHooks.count("on_draw") || m_OverrideHooks.count("on_draw_override"))) {
        return true;
    }
    if (IRenderableComponent* base = GetRenderableBase()) {
        return base->isRenderContentDynamic();
    }
    return true;
}

bool ScriptedComponentWrapper::IntersectRay(const math::Ray& ray, float& outDistance) const {
    EnsureBaseOwner();
    if (IRenderableComponent* base = GetRenderableBase()) {
        return base->IntersectRay(ray, outDistance);
    }
    AABB bounds = getWorldBounds();
    return ray.IntersectAABB(bounds, outDistance);
}

// Factory function
std::shared_ptr<ISerializable> CreateScriptedComponentWrapper(const std::string& customTypeName) {
    return std::make_shared<ScriptedComponentWrapper>(customTypeName);
}

} // namespace core
} // namespace lupine
