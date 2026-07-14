#include "lupine/core/CustomComponentRegistry.hpp"
#include "lupine/core/ScriptedComponentWrapper.hpp"
#include "lupine/core/Serialization.hpp"
#include "lupine/asset/Asset.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/PackFile.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <filesystem>
#include <set>
#include <vector>

namespace lupine {
namespace core {

CustomComponentRegistry& CustomComponentRegistry::GetInstance() {
    static CustomComponentRegistry instance;
    return instance;
}

void CustomComponentRegistry::ScanProject(const std::string& projectPath) {
    m_ProjectPath = projectPath;
    Clear();

    // Scan the project directory recursively for script files. The pre-scan pass
    // collects every custom class name first so the full parse below can resolve
    // bases that are themselves custom components defined in later-scanned files
    // (multi-level inheritance chains).
    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode()) {
        ScanPack();
    } else if (std::filesystem::exists(projectPath)) {
        PreScanClassNames(projectPath);
        ScanDirectory(projectPath);
    }

    // Break any inheritance cycles before instances can be created.
    ValidateInheritanceChains();

    // Register discovered components with TypeRegistry
    RegisterWithTypeRegistry();

    NotifyDefinitionsChanged();

    LOG_INFO(LogCategory::Core, "CustomComponentRegistry: Scanned project, found {} custom components",
             m_Definitions.size());
}

bool CustomComponentRegistry::IsScriptFile(const std::string& filePath) {
    std::string extension = std::filesystem::path(filePath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    return extension == ".lua" || extension == ".rb" || extension == ".py";
}

std::string CustomComponentRegistry::ReadScriptSource(const std::string& filePath) const {
    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode()) {
        const std::string resolved = packFS.resolveAsset(filePath);
        if (packFS.exists(resolved)) {
            return packFS.readFileAsString(resolved);
        }
    }

    std::ifstream file(filePath);
    if (!file.is_open()) {
        return std::string();
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void CustomComponentRegistry::RegisterScriptFile(const std::string& filePath) {
    CustomComponentDefinition def = ParseScriptFile(filePath);
    if (!def.isValid) {
        return;
    }

    if (m_NameToIndex.find(def.className) != m_NameToIndex.end()) {
        LOG_WARN(LogCategory::Core,
            "CustomComponentRegistry: Duplicate class name '{}' found in '{}', already defined in '{}'",
            def.className, def.scriptPath,
            m_Definitions[m_NameToIndex[def.className]].scriptPath);
        return;
    }

    const size_t index = m_Definitions.size();
    m_Definitions.push_back(def);
    m_NameToIndex[def.className] = index;
    m_PathToIndex[def.scriptPath] = index;

    LOG_DEBUG(LogCategory::Core, "CustomComponentRegistry: Found custom component '{}' in '{}'",
              def.className, def.scriptPath);
}

void CustomComponentRegistry::ScanPack() {
    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();

    // ParseScriptFile/ToResourcePath operate on res:// paths; prefix the pack-relative
    // entry so the recorded scriptPath stays canonical and matches the scene's.
    const std::vector<std::string> files = packFS.listFilesRecursive("");

    for (const std::string& file : files) {
        if (IsScriptFile(file)) {
            PreScanClassName("res://" + file);
        }
    }

    for (const std::string& file : files) {
        if (IsScriptFile(file)) {
            RegisterScriptFile("res://" + file);
        }
    }
}

void CustomComponentRegistry::ScanDirectory(const std::string& directory) {
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (!entry.is_regular_file()) continue;

            if (!IsScriptFile(entry.path().string())) {
                continue;
            }

            RegisterScriptFile(entry.path().string());
        }
    } catch (const std::filesystem::filesystem_error& e) {
        LOG_ERROR(LogCategory::Core, "CustomComponentRegistry: Error scanning directory '{}': {}",
                  directory, e.what());
    }
}

bool CustomComponentRegistry::RescanFile(const std::string& scriptPath) {
    auto def = ParseScriptFile(scriptPath);

    // Check if this file was previously registered
    auto pathIt = m_PathToIndex.find(scriptPath);
    if (pathIt != m_PathToIndex.end()) {
        size_t oldIndex = pathIt->second;
        std::string oldClassName = m_Definitions[oldIndex].className;

        if (def.isValid) {
            // Update existing definition
            if (oldClassName != def.className) {
                // Class name changed, update name index
                m_NameToIndex.erase(oldClassName);
                m_NameToIndex[def.className] = oldIndex;
            }
            m_Definitions[oldIndex] = def;

            // A renamed class is a type the TypeRegistry has never seen, so it has no
            // factory and CreateInstance would fail for it. Re-running registration
            // (which skips already-registered names) adds it. Cycles introduced by the
            // edit are broken here too, before any instance can recurse through them.
            ValidateInheritanceChains();
            RegisterWithTypeRegistry();

            LOG_INFO(LogCategory::Core, "CustomComponentRegistry: Updated custom component '{}' from '{}'",
                     def.className, scriptPath);
        } else {
            // File no longer declares a custom component, remove it
            RemoveDefinition(scriptPath);
        }

        NotifyDefinitionsChanged();
        return true;
    }

    // New file that wasn't registered before
    if (def.isValid) {
        // Check for duplicate class names
        if (m_NameToIndex.find(def.className) != m_NameToIndex.end()) {
            LOG_WARN(LogCategory::Core,
                "CustomComponentRegistry: Cannot add '{}' - class name already exists",
                def.className);
            return false;
        }

        size_t index = m_Definitions.size();
        m_Definitions.push_back(def);
        m_NameToIndex[def.className] = index;
        m_PathToIndex[def.scriptPath] = index;

        // Break any cycle the new file introduces before it can be instantiated.
        ValidateInheritanceChains();

        // Register with TypeRegistry
        RegisterWithTypeRegistry();

        NotifyDefinitionsChanged();

        LOG_INFO(LogCategory::Core, "CustomComponentRegistry: Added new custom component '{}' from '{}'",
                 def.className, scriptPath);
        return true;
    }

    return false;
}

bool CustomComponentRegistry::RemoveDefinition(const std::string& scriptPath) {
    auto pathIt = m_PathToIndex.find(scriptPath);
    if (pathIt == m_PathToIndex.end()) {
        return false;
    }

    size_t indexToRemove = pathIt->second;
    std::string className = m_Definitions[indexToRemove].className;

    // Remove from name index
    m_NameToIndex.erase(className);
    m_PathToIndex.erase(scriptPath);

    // Remove from definitions (swap with last and pop)
    if (indexToRemove < m_Definitions.size() - 1) {
        // Swap with last element
        std::swap(m_Definitions[indexToRemove], m_Definitions.back());

        // Update indices for the swapped element
        const auto& swapped = m_Definitions[indexToRemove];
        m_NameToIndex[swapped.className] = indexToRemove;
        m_PathToIndex[swapped.scriptPath] = indexToRemove;
    }
    m_Definitions.pop_back();

    // Note: We don't unregister from TypeRegistry as it doesn't support unregistration
    // Existing instances will continue to work, but new instances cannot be created

    NotifyDefinitionsChanged();

    LOG_INFO(LogCategory::Core, "CustomComponentRegistry: Removed custom component '{}' from '{}'",
             className, scriptPath);
    return true;
}

const CustomComponentDefinition* CustomComponentRegistry::GetDefinition(const std::string& className) const {
    auto it = m_NameToIndex.find(className);
    if (it != m_NameToIndex.end()) {
        return &m_Definitions[it->second];
    }
    return nullptr;
}

const CustomComponentDefinition* CustomComponentRegistry::GetDefinitionByPath(const std::string& scriptPath) const {
    auto it = m_PathToIndex.find(scriptPath);
    if (it != m_PathToIndex.end()) {
        return &m_Definitions[it->second];
    }
    return nullptr;
}

bool CustomComponentRegistry::IsCustomComponent(const std::string& typeName) const {
    return m_NameToIndex.find(typeName) != m_NameToIndex.end();
}

void CustomComponentRegistry::RegisterWithTypeRegistry() {
    // Note: TypeRegistry doesn't support unregistration, so we only register new types
    for (const auto& def : m_Definitions) {
        if (!def.isValid) continue;

        // Check if already registered
        if (TypeRegistry::GetInstance().IsTypeRegistered(def.className)) {
            continue;
        }

        // Capture class name by value for the lambda
        std::string className = def.className;

        TypeRegistry::GetInstance().RegisterType(className,
            [className]() -> std::shared_ptr<ISerializable> {
                return std::make_shared<ScriptedComponentWrapper>(className);
            }
        );

        LOG_DEBUG(LogCategory::Core, "CustomComponentRegistry: Registered type '{}' with TypeRegistry",
                  className);
    }
}

void CustomComponentRegistry::UnregisterFromTypeRegistry() {
    // TypeRegistry doesn't support unregistration
    // This is a no-op for now
}

void CustomComponentRegistry::Clear() {
    m_Definitions.clear();
    m_NameToIndex.clear();
    m_PathToIndex.clear();
    m_ExtraInheritableTypes.clear();
}

std::vector<std::string> CustomComponentRegistry::GetInheritableComponentTypes() {
    // Return all component types that scripts can inherit from
    std::vector<std::string> types = {
        // 2D Rendering
        "Sprite2D",
        "AnimatedSprite2D",
        "ColorRect",
        "Image2D",
        "NineSlicePanel",
        "Shape2D",
        "Line2D",
        "Curve2D",
        "Light2D",
        "GifPlayer",
        "VideoPlayer",

        // 3D Rendering
        "Sprite3D",
        "AnimatedSprite3D",
        "StaticMesh3D",
        "SkeletalMesh3D",
        "PrimitiveMesh3D",
        "Label3D",
        "Panel3D",
        "Button3D",
        "ProgressBar3D",
        "Curve3D",
        "Path3D",
        "PathFollow3D",

        // UI
        "Button",
        "TextureButton",
        "ToggleButton",
        "Checkbox",
        "RadioButton",
        "Label",
        "ProgressBar",
        "Panel",
        "Container",
        "HorizontalContainer",
        "VerticalContainer",
        "GridContainer",
        "PaddingContainer",
        "CenterContainer",
        "DockContainer",
        "Stack",
        "Wrap",
        "SplitContainer",
        "AspectRatioContainer",

        // Audio
        "AudioPlayer",
        "AudioListener",

        // Physics 2D
        "RigidBody2D",
        "StaticBody2D",
        "KinematicBody2D",
        "AreaTrigger2D",
        "CollisionBody2D",
        "CharacterController2D",
        // Also accept Component suffix variants
        "RigidBody2DComponent",
        "StaticBody2DComponent",
        "KinematicBody2DComponent",
        "AreaTrigger2DComponent",
        "CollisionBody2DComponent",
        "CharacterController2DComponent",

        // Physics 3D
        "RigidBody3D",
        "StaticBody3D",
        "KinematicBody3D",
        "AreaTrigger3D",
        "CharacterController3D",
        // Also accept Component suffix variants
        "RigidBody3DComponent",
        "StaticBody3DComponent",
        "KinematicBody3DComponent",
        "AreaTrigger3DComponent",
        "CharacterController3DComponent",

        // Lighting
        "DirectionalLight3D",
        "OmniLight3D",
        "SpotLight3D",

        // Utility
        "Timer",
        "WorldEnvironment",
        "Camera2D",
        "Camera3D",
        "TileMap2D",
        "ParticleEmitter2D",
        "ParticleEmitter3D",
        "YSort",
        "SubViewport",
        "CameraEffectColorGrade",
        "CameraEffectTonemap",
        "CameraEffectVignette",
        "CameraEffectFilmGrain",
        "CameraEffectColorInvert",
        "CameraEffectPosterize",
        "CameraEffectHueShift",
        "CameraEffectBlur",
        "CameraEffectGlow",
        "CameraEffectOutline",
        "CameraEffectPixelate",
        "CameraEffectSharpen",
        "CameraEffectChromaticAberration"
    };

    // Append discovered custom component class names so a custom component can
    // extend another custom component. m_ExtraInheritableTypes is filled by the
    // pre-scan pass (covering files not yet fully parsed); m_Definitions covers
    // anything already parsed. Both are consulted through the singleton because
    // this accessor is static.
    CustomComponentRegistry& reg = GetInstance();
    auto appendUnique = [&types](const std::string& name) {
        if (!name.empty() &&
            std::find(types.begin(), types.end(), name) == types.end()) {
            types.push_back(name);
        }
    };
    for (const std::string& name : reg.m_ExtraInheritableTypes) {
        appendUnique(name);
    }
    for (const CustomComponentDefinition& def : reg.m_Definitions) {
        if (def.isValid) {
            appendUnique(def.className);
        }
    }

    return types;
}

std::string CustomComponentRegistry::ExtractClassNameOnly(const std::string& content,
                                                          const std::string& extension) const {
    std::smatch match;

    // Directive style works for every language: @component_class "Name".
    std::regex directiveRegex(R"((?:--|#)@component_class\s+[\"'](\w+)[\"'])");
    if (std::regex_search(content, match, directiveRegex)) {
        return match[1].str();
    }

    // Native class syntax - mirror the class-name capture groups used by the
    // language-specific parsers so the pre-scan name matches the parsed name.
    if (extension == ".lua") {
        std::regex localExtend(R"(local\s+(\w+)\s*=\s*(\w+):extend\s*\(\s*\))");
        if (std::regex_search(content, match, localExtend)) return match[1].str();
        std::regex extend(R"((\w+)\s*=\s*(\w+):extend\s*\(\s*\))");
        if (std::regex_search(content, match, extend)) return match[1].str();
        std::regex classCall(R"((\w+)\s*=\s*class\s*\(\s*[\"'](\w+)[\"']\s*,\s*(\w+)\s*\))");
        if (std::regex_search(content, match, classCall)) return match[1].str();
    } else if (extension == ".rb") {
        std::regex classDecl(R"(class\s+(\w+)\s*<\s*(\w+))");
        if (std::regex_search(content, match, classDecl)) return match[1].str();
    } else if (extension == ".py") {
        std::regex classDecl(R"(class\s+(\w+)\s*\(\s*(\w+)\s*\)\s*:)");
        if (std::regex_search(content, match, classDecl)) return match[1].str();
    }

    return std::string();
}

void CustomComponentRegistry::PreScanClassName(const std::string& filePath) {
    const std::string content = ReadScriptSource(filePath);
    if (content.empty()) {
        return;
    }

    std::string extension = std::filesystem::path(filePath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    const std::string name = ExtractClassNameOnly(content, extension);
    if (!name.empty() &&
        std::find(m_ExtraInheritableTypes.begin(), m_ExtraInheritableTypes.end(), name) ==
            m_ExtraInheritableTypes.end()) {
        m_ExtraInheritableTypes.push_back(name);
    }
}

void CustomComponentRegistry::PreScanClassNames(const std::string& directory) {
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (!entry.is_regular_file()) continue;

            if (!IsScriptFile(entry.path().string())) {
                continue;
            }

            PreScanClassName(entry.path().string());
        }
    } catch (const std::filesystem::filesystem_error& e) {
        LOG_ERROR(LogCategory::Core,
                  "CustomComponentRegistry: Error pre-scanning directory '{}': {}",
                  directory, e.what());
    }
}

void CustomComponentRegistry::ValidateInheritanceChains() {
    for (auto& def : m_Definitions) {
        if (!def.isValid || def.baseComponentType.empty()) continue;

        // Walk the base chain through the custom definitions. A name seen twice
        // means a cycle (self-extension is the length-1 case); break the link so
        // instantiation cannot recurse forever.
        std::set<std::string> visited;
        visited.insert(def.className);

        std::string current = def.baseComponentType;
        while (!current.empty()) {
            if (visited.count(current) > 0) {
                LOG_ERROR(LogCategory::Core,
                    "CustomComponentRegistry: Inheritance cycle detected for '{}' (revisits '{}'); "
                    "clearing its base link",
                    def.className, current);
                def.baseComponentType.clear();
                break;
            }
            visited.insert(current);

            auto it = m_NameToIndex.find(current);
            if (it == m_NameToIndex.end()) {
                break;  // reached a built-in base type (the root of the chain)
            }
            current = m_Definitions[it->second].baseComponentType;
        }
    }
}

CustomComponentDefinition CustomComponentRegistry::ParseScriptFile(const std::string& filePath) {
    CustomComponentDefinition def;
    def.scriptPath = ToResourcePath(filePath);

    // Read file content
    std::string content = ReadScriptSource(filePath);
    if (content.empty()) {
        def.parseError = "Could not open file: " + filePath;
        return def;
    }

    // Determine language from extension
    std::string extension = std::filesystem::path(filePath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    def.language = GetLanguageFromExtension(extension);

    // Get file modification time
    try {
        def.lastModified = std::filesystem::last_write_time(filePath);
    } catch (...) {
        // Ignore errors
    }

    // Parse based on language
    switch (def.language) {
        case scripting::ScriptLanguage::Lua:
            return ParseLuaScript(content, def.scriptPath);
        case scripting::ScriptLanguage::MRuby:
            return ParseMRubyScript(content, def.scriptPath);
        case scripting::ScriptLanguage::MicroPython:
        case scripting::ScriptLanguage::Python:
            return ParseMicroPythonScript(content, def.scriptPath);
        default:
            def.parseError = "Unknown script language";
            return def;
    }
}

CustomComponentDefinition CustomComponentRegistry::ParseLuaScript(const std::string& content,
                                                                   const std::string& path) {
    CustomComponentDefinition def;
    def.scriptPath = path;
    def.language = scripting::ScriptLanguage::Lua;

    // Parse directives first (--@component_class, --@extends_component)
    ParseDirectives(content, "--", def);

    // If no directive found, try native class syntax
    if (def.className.empty()) {
        ParseLuaNativeClass(content, def);
    }

    // If still no class found, this is not a custom component
    if (def.className.empty()) {
        return def;
    }

    // Parse export properties
    ParseExportProperties(content, "--", def);

    // Detect defined hooks
    DetectDefinedHooks(content, scripting::ScriptLanguage::Lua, def);

    def.isValid = true;
    return def;
}

CustomComponentDefinition CustomComponentRegistry::ParseMRubyScript(const std::string& content,
                                                                     const std::string& path) {
    CustomComponentDefinition def;
    def.scriptPath = path;
    def.language = scripting::ScriptLanguage::MRuby;

    // Parse directives first (#@component_class, #@extends_component)
    ParseDirectives(content, "#", def);

    // If no directive found, try native class syntax
    if (def.className.empty()) {
        ParseMRubyNativeClass(content, def);
    }

    // If still no class found, this is not a custom component
    if (def.className.empty()) {
        return def;
    }

    // Parse export properties
    ParseExportProperties(content, "#", def);

    // Detect defined hooks
    DetectDefinedHooks(content, scripting::ScriptLanguage::MRuby, def);

    def.isValid = true;
    return def;
}

CustomComponentDefinition CustomComponentRegistry::ParseMicroPythonScript(const std::string& content,
                                                                           const std::string& path) {
    CustomComponentDefinition def;
    def.scriptPath = path;
    def.language = scripting::ScriptLanguage::MicroPython;

    // Parse directives first (#@component_class, #@extends_component)
    ParseDirectives(content, "#", def);

    // If no directive found, try native class syntax
    if (def.className.empty()) {
        ParsePythonNativeClass(content, def);
    }

    // If still no class found, this is not a custom component
    if (def.className.empty()) {
        return def;
    }

    // Parse export properties
    ParseExportProperties(content, "#", def);

    // Detect defined hooks
    DetectDefinedHooks(content, scripting::ScriptLanguage::MicroPython, def);

    def.isValid = true;
    return def;
}

void CustomComponentRegistry::ParseDirectives(const std::string& content, const std::string& commentPrefix,
                                              CustomComponentDefinition& def) {
    // Pattern: --@component_class "ClassName" or #@component_class "ClassName"
    std::string componentClassPattern = commentPrefix + R"(@component_class\s+[\"'](\w+)[\"'])";
    std::regex componentClassRegex(componentClassPattern);

    // Pattern: --@extends_component "BaseType" or #@extends_component "BaseType"
    std::string extendsPattern = commentPrefix + R"(@extends_component\s+[\"'](\w+)[\"'])";
    std::regex extendsRegex(extendsPattern);

    // Pattern: --@component_category "Category" or #@component_category "Category"
    std::string categoryPattern = commentPrefix + R"(@component_category\s+[\"']([^\"']+)[\"'])";
    std::regex categoryRegex(categoryPattern);

    // Pattern: --@tool or #@tool. Marks the component as an editor-running tool.
    std::regex toolRegex(commentPrefix + R"(@tool\b)");

    std::smatch match;

    // Find @tool directive
    if (std::regex_search(content, match, toolRegex)) {
        def.isTool = true;
    }

    // Find component_class directive
    if (std::regex_search(content, match, componentClassRegex)) {
        def.className = match[1].str();
    }

    // Find extends_component directive
    if (std::regex_search(content, match, extendsRegex)) {
        def.baseComponentType = match[1].str();

        // Validate that the base type is inheritable
        auto inheritableTypes = GetInheritableComponentTypes();
        bool found = std::find(inheritableTypes.begin(), inheritableTypes.end(),
                              def.baseComponentType) != inheritableTypes.end();
        if (!found) {
            LOG_WARN(LogCategory::Core,
                "CustomComponentRegistry: '{}' in '{}' extends unknown base type '{}'",
                def.className, def.scriptPath, def.baseComponentType);
        }
    }

    // Find component_category directive
    if (std::regex_search(content, match, categoryRegex)) {
        def.subcategory = match[1].str();
    } else if (!def.baseComponentType.empty()) {
        // Auto-determine category based on base type
        if (def.baseComponentType.find("2D") != std::string::npos ||
            def.baseComponentType == "Shape2D" || def.baseComponentType == "Line2D") {
            def.subcategory = "Custom/2D";
        } else if (def.baseComponentType.find("3D") != std::string::npos) {
            def.subcategory = "Custom/3D";
        } else if (def.baseComponentType == "Button" || def.baseComponentType == "Label" ||
                   def.baseComponentType == "Panel" || def.baseComponentType.find("Container") != std::string::npos) {
            def.subcategory = "Custom/UI";
        } else if (def.baseComponentType.find("Body") != std::string::npos ||
                   def.baseComponentType.find("Trigger") != std::string::npos) {
            def.subcategory = "Custom/Physics";
        } else {
            def.subcategory = "Custom";
        }
    }
}

void CustomComponentRegistry::ParseLuaNativeClass(const std::string& content, CustomComponentDefinition& def) {
    // Pattern 1: MyClass = BaseClass:extend()
    std::regex extendPattern(R"((\w+)\s*=\s*(\w+):extend\s*\(\s*\))");

    // Pattern 2: local MyClass = BaseClass:extend()
    std::regex localExtendPattern(R"(local\s+(\w+)\s*=\s*(\w+):extend\s*\(\s*\))");

    // Pattern 3: MyClass = class("MyClass", BaseClass)
    std::regex classPattern(R"((\w+)\s*=\s*class\s*\(\s*[\"'](\w+)[\"']\s*,\s*(\w+)\s*\))");

    std::smatch match;

    // Try pattern 1
    if (std::regex_search(content, match, extendPattern)) {
        std::string potentialBase = match[2].str();
        auto inheritableTypes = GetInheritableComponentTypes();
        if (std::find(inheritableTypes.begin(), inheritableTypes.end(), potentialBase) != inheritableTypes.end()) {
            def.className = match[1].str();
            def.baseComponentType = potentialBase;
            return;
        }
    }

    // Try pattern 2
    if (std::regex_search(content, match, localExtendPattern)) {
        std::string potentialBase = match[2].str();
        auto inheritableTypes = GetInheritableComponentTypes();
        if (std::find(inheritableTypes.begin(), inheritableTypes.end(), potentialBase) != inheritableTypes.end()) {
            def.className = match[1].str();
            def.baseComponentType = potentialBase;
            return;
        }
    }

    // Try pattern 3
    if (std::regex_search(content, match, classPattern)) {
        std::string potentialBase = match[3].str();
        auto inheritableTypes = GetInheritableComponentTypes();
        if (std::find(inheritableTypes.begin(), inheritableTypes.end(), potentialBase) != inheritableTypes.end()) {
            def.className = match[1].str();  // Variable name
            def.baseComponentType = potentialBase;
            return;
        }
    }
}

void CustomComponentRegistry::ParseMRubyNativeClass(const std::string& content, CustomComponentDefinition& def) {
    // Pattern: class MyClass < BaseClass
    std::regex classPattern(R"(class\s+(\w+)\s*<\s*(\w+))");

    std::smatch match;
    if (std::regex_search(content, match, classPattern)) {
        std::string potentialBase = match[2].str();
        auto inheritableTypes = GetInheritableComponentTypes();
        if (std::find(inheritableTypes.begin(), inheritableTypes.end(), potentialBase) != inheritableTypes.end()) {
            def.className = match[1].str();
            def.baseComponentType = potentialBase;
        }
    }
}

void CustomComponentRegistry::ParsePythonNativeClass(const std::string& content, CustomComponentDefinition& def) {
    // Pattern: class MyClass(BaseClass):
    std::regex classPattern(R"(class\s+(\w+)\s*\(\s*(\w+)\s*\)\s*:)");

    std::smatch match;
    if (std::regex_search(content, match, classPattern)) {
        std::string potentialBase = match[2].str();
        auto inheritableTypes = GetInheritableComponentTypes();
        if (std::find(inheritableTypes.begin(), inheritableTypes.end(), potentialBase) != inheritableTypes.end()) {
            def.className = match[1].str();
            def.baseComponentType = potentialBase;
        }
    }
}

void CustomComponentRegistry::ParseExportProperties(const std::string& content, const std::string& commentPrefix,
                                                    CustomComponentDefinition& def) {
    std::string currentGroup;

    // Parse line by line
    std::istringstream stream(content);
    std::string line;

    // Patterns for export directives
    std::string groupPattern = commentPrefix + R"(\s*@export_group\s+[\"']([^\"']+)[\"'])";
    std::string groupEndPattern = commentPrefix + R"(\s*@export_group_end)";
    std::string exportPattern = commentPrefix + R"(\s*@export\s+(\w+)\s+(\w+)\s+(.+))";

    std::regex exportGroupRegex(groupPattern);
    std::regex exportGroupEndRegex(groupEndPattern);
    std::regex exportRegex(exportPattern);
    std::smatch match;

    while (std::getline(stream, line)) {
        // Check for export_group directive
        if (std::regex_search(line, match, exportGroupRegex)) {
            currentGroup = match[1].str();
            continue;
        }

        // Check for export_group_end directive
        if (std::regex_search(line, match, exportGroupEndRegex)) {
            currentGroup.clear();
            continue;
        }

        // Check for export directive
        if (std::regex_search(line, match, exportRegex)) {
            std::string varName = match[1].str();
            std::string typeName = match[2].str();
            std::string defaultStr = match[3].str();

            // Trim whitespace
            defaultStr.erase(0, defaultStr.find_first_not_of(" \t"));
            defaultStr.erase(defaultStr.find_last_not_of(" \t\n\r") + 1);

            PropertyDescriptor descriptor;
            descriptor.name = varName;
            descriptor.group = currentGroup;

            // Parse type and default value
            if (typeName == "int" || typeName == "integer") {
                descriptor.type = PropertyValueType::Int;
                try {
                    descriptor.defaultValue = std::stoi(defaultStr);
                } catch (...) {
                    descriptor.defaultValue = 0;
                }
            }
            else if (typeName == "float" || typeName == "number") {
                descriptor.type = PropertyValueType::Float;
                try {
                    descriptor.defaultValue = std::stof(defaultStr);
                } catch (...) {
                    descriptor.defaultValue = 0.0f;
                }
            }
            else if (typeName == "str" || typeName == "string") {
                descriptor.type = PropertyValueType::String;
                // Strip quotes if present
                if (defaultStr.size() >= 2 &&
                    ((defaultStr.front() == '"' && defaultStr.back() == '"') ||
                     (defaultStr.front() == '\'' && defaultStr.back() == '\''))) {
                    defaultStr = defaultStr.substr(1, defaultStr.size() - 2);
                }
                descriptor.defaultValue = defaultStr;
            }
            else if (typeName == "bool" || typeName == "boolean") {
                descriptor.type = PropertyValueType::Bool;
                descriptor.defaultValue = (defaultStr == "true" || defaultStr == "True");
            }
            else if (typeName == "enum") {
                // Syntax: --@export name enum "A,B,C" <defaultIndex>
                // Renders as an inspector dropdown; the script reads an int index.
                descriptor.type = PropertyValueType::Enum;
                std::string options;
                int defaultIndex = 0;
                size_t quoteStart = defaultStr.find_first_of("\"'");
                if (quoteStart != std::string::npos) {
                    char quote = defaultStr[quoteStart];
                    size_t quoteEnd = defaultStr.find(quote, quoteStart + 1);
                    if (quoteEnd != std::string::npos) {
                        options = defaultStr.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                        std::string rest = defaultStr.substr(quoteEnd + 1);
                        try {
                            defaultIndex = std::stoi(rest);
                        } catch (...) {
                            defaultIndex = 0;
                        }
                    }
                }
                descriptor.hint = PropertyHint(PropertyHintType::Enum, options);
                descriptor.defaultValue = defaultIndex;
            }
            else {
                // Unknown type, skip
                continue;
            }

            CustomComponentDefinition::ExportProperty exportProp;
            exportProp.name = varName;
            exportProp.group = currentGroup;
            exportProp.descriptor = descriptor;
            def.exportProperties.push_back(exportProp);
        }
    }
}

void CustomComponentRegistry::DetectDefinedHooks(const std::string& content, scripting::ScriptLanguage language,
                                                  CustomComponentDefinition& def) {
    // List of standard lifecycle hooks
    std::vector<std::string> hooks = {
        "on_awake", "on_ready", "on_destroy",
        "on_process", "on_physics_process",
        "on_input", "on_input_event", "on_unhandled_input",
        "on_render", "on_draw", "on_late_update",
        "on_enter_tree", "on_exit_tree",
        "on_visibility_changed"
    };

    // Build patterns based on language
    std::string funcPattern;
    switch (language) {
        case scripting::ScriptLanguage::Lua:
            funcPattern = R"(function\s+)";
            break;
        case scripting::ScriptLanguage::MRuby:
            funcPattern = R"(def\s+)";
            break;
        case scripting::ScriptLanguage::MicroPython:
        case scripting::ScriptLanguage::Python:
            funcPattern = R"(def\s+)";
            break;
        default:
            return;
    }

    for (const auto& hook : hooks) {
        // Check for regular hook
        std::regex hookRegex(funcPattern + hook + R"(\s*[\(\)])");
        if (std::regex_search(content, hookRegex)) {
            def.definedHooks.insert(hook);
        }

        // Check for override hook
        std::string overrideHook = hook + "_override";
        std::regex overrideRegex(funcPattern + overrideHook + R"(\s*[\(\)])");
        if (std::regex_search(content, overrideRegex)) {
            def.overrideHooks.insert(overrideHook);
        }
    }
}

scripting::ScriptLanguage CustomComponentRegistry::GetLanguageFromExtension(const std::string& extension) {
    if (extension == ".lua") {
        return scripting::ScriptLanguage::Lua;
    } else if (extension == ".rb") {
        return scripting::ScriptLanguage::MRuby;
    } else if (extension == ".py") {
        return scripting::ScriptLanguage::MicroPython;
    }
    return scripting::ScriptLanguage::Lua;  // Default
}

std::string CustomComponentRegistry::ToResourcePath(const std::string& absolutePath) const {
    // Already canonical - the pack scan feeds res:// keys straight through.
    if (absolutePath.rfind("res://", 0) == 0) {
        return absolutePath;
    }
    if (m_ProjectPath.empty()) {
        return absolutePath;
    }

    // Convert to res:// path if within project
    std::filesystem::path absPath(absolutePath);
    std::filesystem::path projPath(m_ProjectPath);

    try {
        auto relPath = std::filesystem::relative(absPath, projPath);
        std::string result = "res://" + relPath.generic_string();
        return result;
    } catch (...) {
        return absolutePath;
    }
}

void CustomComponentRegistry::NotifyDefinitionsChanged() {
    if (m_OnDefinitionsChanged) {
        m_OnDefinitionsChanged();
    }
}

} // namespace core
} // namespace lupine
