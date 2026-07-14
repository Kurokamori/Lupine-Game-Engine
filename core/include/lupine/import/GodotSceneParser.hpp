#pragma once

/**
 * Godot Scene Parser
 *
 * Parses Godot .tscn (text scene) files into an intermediate representation
 * that can be converted to Lupine scenes.
 *
 * Godot .tscn format:
 * - [gd_scene] header with format version, load_steps, uid
 * - [ext_resource] entries for external files (scripts, textures, etc.)
 * - [sub_resource] entries for inline resources
 * - [node] entries defining the scene tree
 */

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <variant>
#include <optional>
#include <nlohmann/json.hpp>

namespace lupine {
namespace import {

/**
 * Represents a parsed Godot value (can be various types)
 */
struct GodotValue {
    enum class Type {
        Null,
        Bool,
        Int,
        Float,
        String,
        Vector2,
        Vector3,
        Color,
        Rect2,
        Transform2D,
        Transform3D,
        Quat,
        Array,
        Dictionary,
        ExtResource,
        SubResource,
        NodePath
    };

    Type type = Type::Null;

    // Value storage
    bool boolValue = false;
    int64_t intValue = 0;
    double floatValue = 0.0;
    std::string stringValue;

    // Vector types
    double x = 0, y = 0, z = 0, w = 1;  // For vectors, quaternions

    // Color (r, g, b, a)
    double r = 1, g = 1, b = 1, a = 1;

    // Resource references
    std::string resourceId;  // For ExtResource/SubResource

    // Complex types
    std::vector<GodotValue> arrayValue;
    std::map<std::string, GodotValue> dictValue;

    // Helpers
    static GodotValue Null() { return GodotValue(); }
    static GodotValue Bool(bool v) { GodotValue gv; gv.type = Type::Bool; gv.boolValue = v; return gv; }
    static GodotValue Int(int64_t v) { GodotValue gv; gv.type = Type::Int; gv.intValue = v; return gv; }
    static GodotValue Float(double v) { GodotValue gv; gv.type = Type::Float; gv.floatValue = v; return gv; }
    static GodotValue String(const std::string& v) { GodotValue gv; gv.type = Type::String; gv.stringValue = v; return gv; }
    static GodotValue Vector2(double x, double y) { GodotValue gv; gv.type = Type::Vector2; gv.x = x; gv.y = y; return gv; }
    static GodotValue Vector3(double x, double y, double z) { GodotValue gv; gv.type = Type::Vector3; gv.x = x; gv.y = y; gv.z = z; return gv; }
    static GodotValue Color(double r, double g, double b, double a) { GodotValue gv; gv.type = Type::Color; gv.r = r; gv.g = g; gv.b = b; gv.a = a; return gv; }

    // Conversion to JSON
    nlohmann::json ToJson() const;
};

/**
 * Represents an external resource reference in Godot
 */
struct GodotExtResource {
    std::string id;           // Resource ID (e.g., "1_abc")
    std::string type;         // Resource type (e.g., "Texture2D", "Script")
    std::string path;         // Resource path (e.g., "res://icon.png")
    std::string uid;          // Optional UID
};

/**
 * Represents an inline sub-resource in Godot
 */
struct GodotSubResource {
    std::string id;           // Resource ID
    std::string type;         // Resource type (e.g., "CircleShape2D")
    std::map<std::string, GodotValue> properties;
};

/**
 * Represents a node in the Godot scene tree
 */
struct GodotNode {
    std::string name;         // Node name
    std::string type;         // Node type (e.g., "Node2D", "Sprite2D")
    std::string parent;       // Parent path ("." for root children, "Player/Area" for nested)
    std::string owner;        // Owner node (for instanced scenes)
    std::string instance;     // Instance path (for instanced scenes)
    int instancePlaceholder = 0;  // Instance placeholder index
    std::map<std::string, GodotValue> properties;
    std::vector<std::string> groups;  // Node groups

    // Resolved children (filled during tree building)
    std::vector<GodotNode*> children;
};

/**
 * Represents a connection (signal) in the Godot scene
 */
struct GodotConnection {
    std::string signal;       // Signal name
    std::string from;         // Source node path
    std::string to;           // Target node path
    std::string method;       // Method to call
    int flags = 0;            // Connection flags
};

/**
 * Parsed Godot scene structure
 */
struct GodotScene {
    // Scene metadata
    int formatVersion = 3;
    int loadSteps = 0;
    std::string uid;

    // Resources
    std::vector<GodotExtResource> extResources;
    std::vector<GodotSubResource> subResources;

    // Scene tree
    std::vector<GodotNode> nodes;
    GodotNode* rootNode = nullptr;

    // Signal connections
    std::vector<GodotConnection> connections;

    // Resource lookup maps
    std::unordered_map<std::string, GodotExtResource*> extResourceMap;
    std::unordered_map<std::string, GodotSubResource*> subResourceMap;
    std::unordered_map<std::string, GodotNode*> nodePathMap;

    // Source file info
    std::string sourcePath;
    std::string sourceDirectory;

    /**
     * Get external resource by ID
     */
    const GodotExtResource* GetExtResource(const std::string& id) const;

    /**
     * Get sub-resource by ID
     */
    const GodotSubResource* GetSubResource(const std::string& id) const;

    /**
     * Get node by path
     */
    const GodotNode* GetNode(const std::string& path) const;

    /**
     * Build node hierarchy (resolve parent references)
     */
    void BuildNodeHierarchy();
};

/**
 * Parser for Godot .tscn files
 */
class GodotSceneParser {
public:
    GodotSceneParser();
    ~GodotSceneParser();

    /**
     * Parse a .tscn file from path
     * @param filepath Path to the .tscn file
     * @return Parsed scene structure, or nullopt on failure
     */
    std::optional<GodotScene> ParseFile(const std::string& filepath);

    /**
     * Parse .tscn content from string
     * @param content Content of the .tscn file
     * @param sourcePath Original file path (for resource resolution)
     * @return Parsed scene structure, or nullopt on failure
     */
    std::optional<GodotScene> ParseString(const std::string& content, const std::string& sourcePath = "");

    /**
     * Get last parse error
     */
    const std::string& GetLastError() const { return m_LastError; }

    /**
     * Get parse warnings
     */
    const std::vector<std::string>& GetWarnings() const { return m_Warnings; }

private:
    std::string m_LastError;
    std::vector<std::string> m_Warnings;

    // Parsing state
    size_t m_CurrentLine;
    std::vector<std::string> m_Lines;

    // Parsing methods
    bool ParseSection(GodotScene& scene, const std::string& line);
    bool ParseGdSceneHeader(GodotScene& scene, const std::string& content);
    bool ParseExtResource(GodotScene& scene, const std::string& content);
    bool ParseSubResource(GodotScene& scene, const std::string& content);
    bool ParseNode(GodotScene& scene, const std::string& content);
    bool ParseConnection(GodotScene& scene, const std::string& content);

    // Property parsing
    std::map<std::string, GodotValue> ParseProperties();
    GodotValue ParseValue(const std::string& valueStr);
    GodotValue ParseVector2(const std::string& str);
    GodotValue ParseVector3(const std::string& str);
    GodotValue ParseColor(const std::string& str);
    GodotValue ParseRect2(const std::string& str);
    GodotValue ParseTransform2D(const std::string& str);
    GodotValue ParseTransform3D(const std::string& str);
    GodotValue ParseQuat(const std::string& str);
    GodotValue ParseArray(const std::string& str);
    GodotValue ParseDictionary(const std::string& str);
    GodotValue ParseExtResourceRef(const std::string& str);
    GodotValue ParseSubResourceRef(const std::string& str);
    GodotValue ParseNodePath(const std::string& str);

    // Utility methods
    std::string Trim(const std::string& str);
    std::string ExtractQuotedString(const std::string& str);
    std::map<std::string, std::string> ParseSectionAttributes(const std::string& content);
    std::vector<std::string> SplitArgs(const std::string& str);
    bool IsSection(const std::string& line);
    std::pair<std::string, std::string> GetSectionTypeAndContent(const std::string& line);
};

} // namespace import
} // namespace lupine
