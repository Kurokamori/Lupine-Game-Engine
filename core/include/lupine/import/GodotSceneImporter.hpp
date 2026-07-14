#pragma once

/**
 * Godot Scene Importer
 *
 * Converts parsed Godot scenes into Lupine scenes.
 * Handles node type mapping, component conversion, and resource resolution.
 */

#include "GodotSceneParser.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Component.hpp"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <memory>

namespace lupine {
namespace import {

/**
 * Represents an asset that needs to be imported along with the scene
 */
struct ImportAsset {
    enum class Type {
        Texture,
        Audio,
        Model,
        Font,
        Script,
        Scene,
        Resource,
        Unknown
    };

    enum class Status {
        Resolved,       // Asset found and can be imported
        Missing,        // Asset not found
        NeedsMapping,   // Asset exists but needs user to map it
        Script          // Script file - needs special handling
    };

    std::string godotPath;          // Original Godot path (res://...)
    std::string resolvedPath;       // Resolved physical path
    std::string lupinePath;         // Target path in Lupine project
    std::string resourceId;         // Godot resource ID
    Type type = Type::Unknown;
    Status status = Status::Missing;

    // For scripts
    std::string scriptLanguage;     // "GDScript", "C#", etc.
    bool createBlankScript = false;
    std::string blankScriptLanguage = "Lua";  // Default blank script language

    // For import decision
    bool shouldImport = true;
    bool isSelected = true;         // User selection in import dialog
};

/**
 * Represents a node that couldn't be directly mapped
 */
struct UnmappedNode {
    std::string godotType;          // Original Godot type
    std::string nodeName;           // Node name in scene
    std::string nodePath;           // Full path in hierarchy
    std::string suggestedLupineType; // Suggested Lupine equivalent
    std::string notes;              // Any notes about the conversion
    bool hasPartialSupport = false; // True if some properties can be converted
};

/**
 * Result of an import operation
 */
struct ImportResult {
    bool success = false;
    std::shared_ptr<core::Scene> scene;
    std::vector<ImportAsset> assets;
    std::vector<UnmappedNode> unmappedNodes;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;

    // Referenced scenes (instanced scenes that should also be imported)
    std::vector<std::string> referencedScenes;

    // Summary statistics
    int totalNodes = 0;
    int convertedNodes = 0;
    int partialNodes = 0;
    int skippedNodes = 0;
    int totalAssets = 0;
    int resolvedAssets = 0;
    int missingAssets = 0;
};

/**
 * Configuration for the import process
 */
struct ImportConfig {
    std::string targetDirectory;        // Where to place imported files
    bool copyAssets = true;             // Copy asset files to project
    bool createBlankScripts = true;     // Create blank scripts for GDScript
    std::string defaultScriptLanguage = "Lua";
    bool preserveNodeNames = true;      // Keep original node names
    bool convertGroups = true;          // Convert Godot groups to tags/layers
    bool importConnections = false;     // Try to import signal connections

    // Asset path resolution
    std::string godotProjectRoot;       // Root of the Godot project
    std::string lupineProjectRoot;      // Root of the Lupine project
    std::vector<std::string> assetSearchPaths;  // Additional paths to search
};

/**
 * Node type mapping definition
 */
struct NodeMapping {
    std::string godotType;
    std::string lupineNodeType;         // "Node", "Node2D", "Node3D"
    std::vector<std::string> components; // Components to add
    std::function<void(const GodotNode&, std::shared_ptr<core::Node>)> customConverter;
    std::string notes;
};

/**
 * Converts Godot scenes to Lupine scenes
 */
class GodotSceneImporter {
public:
    GodotSceneImporter();
    ~GodotSceneImporter();

    /**
     * Set import configuration
     */
    void SetConfig(const ImportConfig& config) { m_Config = config; }
    const ImportConfig& GetConfig() const { return m_Config; }

    /**
     * Import a Godot scene file
     * @param filepath Path to the .tscn file
     * @return Import result with scene and asset information
     */
    ImportResult Import(const std::string& filepath);

    /**
     * Import from already parsed scene
     * @param scene Parsed Godot scene
     * @return Import result
     */
    ImportResult Import(const GodotScene& scene);

    /**
     * Analyze a scene without importing (preview)
     * @param filepath Path to the .tscn file
     * @return Import result with analysis only (scene not created)
     */
    ImportResult Analyze(const std::string& filepath);

    /**
     * Resolve asset paths and check availability
     * @param assets List of assets to resolve
     */
    void ResolveAssets(std::vector<ImportAsset>& assets);

    /**
     * Copy assets to project directory
     * @param assets Assets to copy
     * @return Number of assets successfully copied
     */
    int CopyAssets(const std::vector<ImportAsset>& assets);

    /**
     * Create blank script files
     * @param assets Script assets to create blanks for
     * @param language Target scripting language
     * @return Number of scripts created
     */
    int CreateBlankScripts(const std::vector<ImportAsset>& assets, const std::string& language);

    /**
     * Get available node mappings
     */
    const std::vector<NodeMapping>& GetNodeMappings() const { return m_NodeMappings; }

    /**
     * Get mapping for a Godot node type
     */
    const NodeMapping* GetMapping(const std::string& godotType) const;

    /**
     * Check if a Godot type has a mapping
     */
    bool HasMapping(const std::string& godotType) const;

    /**
     * Get list of all supported Godot node types
     */
    std::vector<std::string> GetSupportedGodotTypes() const;

    /**
     * Get list of Godot types without mappings
     */
    std::vector<std::string> GetUnsupportedGodotTypes(const GodotScene& scene) const;

private:
    ImportConfig m_Config;
    std::vector<NodeMapping> m_NodeMappings;
    std::unordered_map<std::string, size_t> m_MappingIndex;

    // Initialize built-in mappings
    void InitializeMappings();

    // Conversion methods
    std::shared_ptr<core::Node> ConvertNode(const GodotNode& godotNode, const GodotScene& scene, ImportResult& result);
    void ConvertNodeProperties(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    std::shared_ptr<core::Component> CreateComponent(const std::string& typeName);
    void ConvertComponentProperties(const GodotNode& godotNode, std::shared_ptr<core::Component> component, const GodotScene& scene);

    // Specific node converters
    void ConvertSprite2D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertSprite3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertAnimatedSprite2D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertLabel(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertCamera2D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertCamera3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertAudioStreamPlayer(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertLight2D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertDirectionalLight3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertOmniLight3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertSpotLight3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertMeshInstance3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertRigidBody2D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertStaticBody2D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertCharacterBody2D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertArea2D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertCollisionShape2D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertRigidBody3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertStaticBody3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertCharacterBody3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertArea3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertCollisionShape3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertTimer(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertTileMap(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertColorRect(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertTextureRect(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertButton(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertContainer(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertPanel(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);

    // UI Control converters
    void ConvertControl(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertProgressBar(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertLineEdit(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);
    void ConvertTextEdit(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene);

    // Asset handling
    ImportAsset CreateAssetEntry(const GodotExtResource& resource, const GodotScene& scene);
    std::string ResolveGodotPath(const std::string& godotPath, const GodotScene& scene);
    std::string ConvertToLupinePath(const std::string& godotPath);
    ImportAsset::Type DetermineAssetType(const std::string& godotType, const std::string& path);

    // Value conversion helpers
    double GetFloatProperty(const GodotNode& node, const std::string& name, double defaultVal = 0.0);
    int64_t GetIntProperty(const GodotNode& node, const std::string& name, int64_t defaultVal = 0);
    bool GetBoolProperty(const GodotNode& node, const std::string& name, bool defaultVal = false);
    std::string GetStringProperty(const GodotNode& node, const std::string& name, const std::string& defaultVal = "");
    std::pair<double, double> GetVector2Property(const GodotNode& node, const std::string& name, double defX = 0, double defY = 0);
    std::tuple<double, double, double> GetVector3Property(const GodotNode& node, const std::string& name, double defX = 0, double defY = 0, double defZ = 0);
    std::tuple<double, double, double, double> GetColorProperty(const GodotNode& node, const std::string& name, double defR = 1, double defG = 1, double defB = 1, double defA = 1);
    const GodotValue* GetProperty(const GodotNode& node, const std::string& name);

    // Helper for getting Control size from size, custom_minimum_size, or offset deltas
    std::pair<float, float> GetControlSize(const GodotNode& node);
};

/**
 * Utility class for generating blank script templates
 */
class BlankScriptGenerator {
public:
    /**
     * Generate a blank Lua script
     */
    static std::string GenerateLuaScript(const std::string& className, const std::vector<std::string>& methods = {});

    /**
     * Generate a blank Python script
     */
    static std::string GeneratePythonScript(const std::string& className, const std::vector<std::string>& methods = {});

    /**
     * Generate a blank mruby script
     */
    static std::string GenerateMrubyScript(const std::string& className, const std::vector<std::string>& methods = {});

    /**
     * Get file extension for a script language
     */
    static std::string GetScriptExtension(const std::string& language);

    /**
     * Get available scripting languages
     */
    static std::vector<std::string> GetAvailableLanguages();
};

} // namespace import
} // namespace lupine
