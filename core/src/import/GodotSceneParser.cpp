#include "lupine/import/GodotSceneParser.hpp"
#include "lupine/logger/Logger.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <cctype>

namespace lupine {
namespace import {

// ============================================================================
// GodotValue Implementation
// ============================================================================

nlohmann::json GodotValue::ToJson() const {
    switch (type) {
        case Type::Null:
            return nullptr;
        case Type::Bool:
            return boolValue;
        case Type::Int:
            return intValue;
        case Type::Float:
            return floatValue;
        case Type::String:
        case Type::NodePath:
            return stringValue;
        case Type::Vector2:
            return nlohmann::json{{"x", x}, {"y", y}};
        case Type::Vector3:
            return nlohmann::json{{"x", x}, {"y", y}, {"z", z}};
        case Type::Color:
            return nlohmann::json{{"r", r}, {"g", g}, {"b", b}, {"a", a}};
        case Type::Quat:
            return nlohmann::json{{"x", x}, {"y", y}, {"z", z}, {"w", w}};
        case Type::Rect2:
            return nlohmann::json{{"x", x}, {"y", y}, {"w", z}, {"h", w}};
        case Type::ExtResource:
        case Type::SubResource:
            return nlohmann::json{{"resourceId", resourceId}};
        case Type::Array: {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& val : arrayValue) {
                arr.push_back(val.ToJson());
            }
            return arr;
        }
        case Type::Dictionary: {
            nlohmann::json obj = nlohmann::json::object();
            for (const auto& [key, val] : dictValue) {
                obj[key] = val.ToJson();
            }
            return obj;
        }
        default:
            return nullptr;
    }
}

// ============================================================================
// GodotScene Implementation
// ============================================================================

const GodotExtResource* GodotScene::GetExtResource(const std::string& id) const {
    auto it = extResourceMap.find(id);
    if (it != extResourceMap.end()) {
        return it->second;
    }
    return nullptr;
}

const GodotSubResource* GodotScene::GetSubResource(const std::string& id) const {
    auto it = subResourceMap.find(id);
    if (it != subResourceMap.end()) {
        return it->second;
    }
    return nullptr;
}

const GodotNode* GodotScene::GetNode(const std::string& path) const {
    auto it = nodePathMap.find(path);
    if (it != nodePathMap.end()) {
        return it->second;
    }
    return nullptr;
}

void GodotScene::BuildNodeHierarchy() {
    if (nodes.empty()) return;

    // Build resource lookup maps
    for (auto& res : extResources) {
        extResourceMap[res.id] = &res;
    }
    for (auto& res : subResources) {
        subResourceMap[res.id] = &res;
    }

    // First, build path to node mapping
    // First node is always root
    if (!nodes.empty()) {
        rootNode = &nodes[0];
        nodePathMap[nodes[0].name] = rootNode;
        nodePathMap["."] = rootNode;  // "." refers to root in children
    }

    // Build full paths for all nodes
    for (size_t i = 1; i < nodes.size(); i++) {
        auto& node = nodes[i];
        std::string fullPath;

        if (node.parent == ".") {
            // Direct child of root
            fullPath = rootNode->name + "/" + node.name;
        } else if (node.parent.empty()) {
            // This shouldn't happen for non-root nodes
            fullPath = node.name;
        } else {
            // Find parent's full path
            std::string parentPath = node.parent;
            // If parent starts with ".", replace with root name
            if (parentPath.front() == '.') {
                if (parentPath == ".") {
                    parentPath = rootNode->name;
                } else {
                    parentPath = rootNode->name + parentPath.substr(1);
                }
            }
            fullPath = parentPath + "/" + node.name;
        }

        nodePathMap[fullPath] = &node;
        nodePathMap[node.name] = &node;  // Also allow lookup by just name
    }

    // Now build parent-child relationships
    for (size_t i = 1; i < nodes.size(); i++) {
        auto& node = nodes[i];
        GodotNode* parent = nullptr;

        if (node.parent == ".") {
            parent = rootNode;
        } else if (!node.parent.empty()) {
            // Try to find parent
            std::string parentPath = node.parent;
            if (parentPath.front() == '.') {
                if (parentPath == ".") {
                    parentPath = rootNode->name;
                } else {
                    parentPath = rootNode->name + parentPath.substr(1);
                }
            }

            auto it = nodePathMap.find(parentPath);
            if (it != nodePathMap.end()) {
                parent = it->second;
            }
        }

        if (parent) {
            parent->children.push_back(&node);
        }
    }
}

// ============================================================================
// GodotSceneParser Implementation
// ============================================================================

GodotSceneParser::GodotSceneParser()
    : m_CurrentLine(0) {
}

GodotSceneParser::~GodotSceneParser() {
}

std::optional<GodotScene> GodotSceneParser::ParseFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        m_LastError = "Failed to open file: " + filepath;
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return ParseString(buffer.str(), filepath);
}

std::optional<GodotScene> GodotSceneParser::ParseString(const std::string& content, const std::string& sourcePath) {
    m_LastError.clear();
    m_Warnings.clear();
    m_Lines.clear();
    m_CurrentLine = 0;

    // Store source info
    GodotScene scene;
    scene.sourcePath = sourcePath;

    // Extract directory from path
    size_t lastSlash = sourcePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        scene.sourceDirectory = sourcePath.substr(0, lastSlash);
    }

    // Split content into lines
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        m_Lines.push_back(line);
    }

    // Parse line by line
    for (m_CurrentLine = 0; m_CurrentLine < m_Lines.size(); m_CurrentLine++) {
        const std::string& currentLine = m_Lines[m_CurrentLine];
        std::string trimmed = Trim(currentLine);

        // Skip empty lines and comments
        if (trimmed.empty() || trimmed[0] == ';') {
            continue;
        }

        // Check if this is a section header
        if (IsSection(trimmed)) {
            if (!ParseSection(scene, trimmed)) {
                return std::nullopt;
            }
        }
    }

    // Build node hierarchy
    scene.BuildNodeHierarchy();

    return scene;
}

bool GodotSceneParser::IsSection(const std::string& line) {
    return !line.empty() && line.front() == '[' && line.back() == ']';
}

std::pair<std::string, std::string> GodotSceneParser::GetSectionTypeAndContent(const std::string& line) {
    // Remove brackets
    std::string content = line.substr(1, line.length() - 2);

    // Find first space to get section type
    size_t spacePos = content.find(' ');
    if (spacePos == std::string::npos) {
        return {content, ""};
    }

    std::string type = content.substr(0, spacePos);
    std::string attrs = content.substr(spacePos + 1);
    return {type, attrs};
}

bool GodotSceneParser::ParseSection(GodotScene& scene, const std::string& line) {
    auto [type, content] = GetSectionTypeAndContent(line);

    if (type == "gd_scene") {
        return ParseGdSceneHeader(scene, content);
    } else if (type == "ext_resource") {
        return ParseExtResource(scene, content);
    } else if (type == "sub_resource") {
        return ParseSubResource(scene, content);
    } else if (type == "node") {
        return ParseNode(scene, content);
    } else if (type == "connection") {
        return ParseConnection(scene, content);
    } else {
        m_Warnings.push_back("Unknown section type: " + type + " at line " + std::to_string(m_CurrentLine + 1));
        return true;  // Continue parsing
    }
}

bool GodotSceneParser::ParseGdSceneHeader(GodotScene& scene, const std::string& content) {
    auto attrs = ParseSectionAttributes(content);

    if (attrs.count("format")) {
        scene.formatVersion = std::stoi(attrs["format"]);
    }
    if (attrs.count("load_steps")) {
        scene.loadSteps = std::stoi(attrs["load_steps"]);
    }
    if (attrs.count("uid")) {
        scene.uid = attrs["uid"];
    }

    return true;
}

bool GodotSceneParser::ParseExtResource(GodotScene& scene, const std::string& content) {
    auto attrs = ParseSectionAttributes(content);

    GodotExtResource resource;
    resource.type = attrs["type"];
    resource.path = attrs["path"];
    resource.id = attrs["id"];
    resource.uid = attrs.count("uid") ? attrs["uid"] : "";

    scene.extResources.push_back(resource);
    return true;
}

bool GodotSceneParser::ParseSubResource(GodotScene& scene, const std::string& content) {
    auto attrs = ParseSectionAttributes(content);

    GodotSubResource resource;
    resource.type = attrs["type"];
    resource.id = attrs["id"];

    // Parse properties following this section
    resource.properties = ParseProperties();

    scene.subResources.push_back(resource);
    return true;
}

bool GodotSceneParser::ParseNode(GodotScene& scene, const std::string& content) {
    auto attrs = ParseSectionAttributes(content);

    GodotNode node;
    node.name = attrs["name"];
    node.type = attrs.count("type") ? attrs["type"] : "";
    node.parent = attrs.count("parent") ? attrs["parent"] : "";
    node.owner = attrs.count("owner") ? attrs["owner"] : "";
    node.instance = attrs.count("instance") ? attrs["instance"] : "";

    // Parse groups
    if (attrs.count("groups")) {
        std::string groupsStr = attrs["groups"];
        // Groups are in PackedStringArray format: ["group1", "group2"]
        // Simple parsing for now
        if (groupsStr.front() == '[' && groupsStr.back() == ']') {
            groupsStr = groupsStr.substr(1, groupsStr.length() - 2);
            std::stringstream ss(groupsStr);
            std::string group;
            while (std::getline(ss, group, ',')) {
                group = Trim(group);
                if (!group.empty() && group.front() == '"' && group.back() == '"') {
                    group = group.substr(1, group.length() - 2);
                }
                if (!group.empty()) {
                    node.groups.push_back(group);
                }
            }
        }
    }

    // Parse properties following this section
    node.properties = ParseProperties();

    scene.nodes.push_back(node);
    return true;
}

bool GodotSceneParser::ParseConnection(GodotScene& scene, const std::string& content) {
    auto attrs = ParseSectionAttributes(content);

    GodotConnection connection;
    connection.signal = attrs["signal"];
    connection.from = attrs["from"];
    connection.to = attrs["to"];
    connection.method = attrs["method"];
    if (attrs.count("flags")) {
        connection.flags = std::stoi(attrs["flags"]);
    }

    scene.connections.push_back(connection);
    return true;
}

std::map<std::string, GodotValue> GodotSceneParser::ParseProperties() {
    std::map<std::string, GodotValue> properties;

    // Look ahead for property lines (lines that aren't sections)
    while (m_CurrentLine + 1 < m_Lines.size()) {
        const std::string& nextLine = m_Lines[m_CurrentLine + 1];
        std::string trimmed = Trim(nextLine);

        // Stop if we hit a new section or empty line
        if (trimmed.empty() || IsSection(trimmed)) {
            break;
        }

        // Skip comments
        if (trimmed[0] == ';') {
            m_CurrentLine++;
            continue;
        }

        // Parse property line: name = value
        size_t equalsPos = trimmed.find('=');
        if (equalsPos != std::string::npos) {
            std::string name = Trim(trimmed.substr(0, equalsPos));
            std::string valueStr = Trim(trimmed.substr(equalsPos + 1));

            // Handle multi-line values (arrays, dictionaries)
            while (!valueStr.empty()) {
                // Count brackets to handle multi-line
                int brackets = 0;
                int parens = 0;
                for (char c : valueStr) {
                    if (c == '[') brackets++;
                    else if (c == ']') brackets--;
                    else if (c == '(') parens++;
                    else if (c == ')') parens--;
                }

                if (brackets > 0 || parens > 0) {
                    // Need more lines
                    m_CurrentLine++;
                    if (m_CurrentLine + 1 >= m_Lines.size()) break;
                    valueStr += "\n" + Trim(m_Lines[m_CurrentLine + 1]);
                } else {
                    break;
                }
            }

            properties[name] = ParseValue(valueStr);
        }

        m_CurrentLine++;
    }

    return properties;
}

GodotValue GodotSceneParser::ParseValue(const std::string& valueStr) {
    std::string trimmed = Trim(valueStr);

    if (trimmed.empty()) {
        return GodotValue::Null();
    }

    // Boolean
    if (trimmed == "true") return GodotValue::Bool(true);
    if (trimmed == "false") return GodotValue::Bool(false);

    // Null
    if (trimmed == "null" || trimmed == "nil") return GodotValue::Null();

    // String (quoted)
    if (trimmed.front() == '"' && trimmed.back() == '"') {
        return GodotValue::String(trimmed.substr(1, trimmed.length() - 2));
    }

    // Vector2
    if (trimmed.find("Vector2(") == 0 || trimmed.find("Vector2i(") == 0) {
        return ParseVector2(trimmed);
    }

    // Vector3
    if (trimmed.find("Vector3(") == 0 || trimmed.find("Vector3i(") == 0) {
        return ParseVector3(trimmed);
    }

    // Color
    if (trimmed.find("Color(") == 0) {
        return ParseColor(trimmed);
    }

    // Rect2
    if (trimmed.find("Rect2(") == 0 || trimmed.find("Rect2i(") == 0) {
        return ParseRect2(trimmed);
    }

    // Transform2D
    if (trimmed.find("Transform2D(") == 0) {
        return ParseTransform2D(trimmed);
    }

    // Transform3D
    if (trimmed.find("Transform3D(") == 0) {
        return ParseTransform3D(trimmed);
    }

    // Quaternion
    if (trimmed.find("Quaternion(") == 0) {
        return ParseQuat(trimmed);
    }

    // ExtResource
    if (trimmed.find("ExtResource(") == 0) {
        return ParseExtResourceRef(trimmed);
    }

    // SubResource
    if (trimmed.find("SubResource(") == 0) {
        return ParseSubResourceRef(trimmed);
    }

    // NodePath
    if (trimmed.find("NodePath(") == 0) {
        return ParseNodePath(trimmed);
    }

    // Array
    if (trimmed.front() == '[') {
        return ParseArray(trimmed);
    }

    // Dictionary
    if (trimmed.front() == '{') {
        return ParseDictionary(trimmed);
    }

    // Number (try int first, then float)
    try {
        // Check if it has a decimal point
        if (trimmed.find('.') != std::string::npos || trimmed.find('e') != std::string::npos || trimmed.find('E') != std::string::npos) {
            return GodotValue::Float(std::stod(trimmed));
        } else {
            return GodotValue::Int(std::stoll(trimmed));
        }
    } catch (...) {
        // Not a number, treat as string
        return GodotValue::String(trimmed);
    }
}

GodotValue GodotSceneParser::ParseVector2(const std::string& str) {
    // Format: Vector2(x, y) or Vector2i(x, y)
    size_t start = str.find('(');
    size_t end = str.rfind(')');
    if (start == std::string::npos || end == std::string::npos) {
        return GodotValue::Vector2(0, 0);
    }

    std::string inner = str.substr(start + 1, end - start - 1);
    auto args = SplitArgs(inner);

    double x = 0, y = 0;
    if (args.size() >= 1) x = std::stod(args[0]);
    if (args.size() >= 2) y = std::stod(args[1]);

    return GodotValue::Vector2(x, y);
}

GodotValue GodotSceneParser::ParseVector3(const std::string& str) {
    size_t start = str.find('(');
    size_t end = str.rfind(')');
    if (start == std::string::npos || end == std::string::npos) {
        return GodotValue::Vector3(0, 0, 0);
    }

    std::string inner = str.substr(start + 1, end - start - 1);
    auto args = SplitArgs(inner);

    double x = 0, y = 0, z = 0;
    if (args.size() >= 1) x = std::stod(args[0]);
    if (args.size() >= 2) y = std::stod(args[1]);
    if (args.size() >= 3) z = std::stod(args[2]);

    return GodotValue::Vector3(x, y, z);
}

GodotValue GodotSceneParser::ParseColor(const std::string& str) {
    size_t start = str.find('(');
    size_t end = str.rfind(')');
    if (start == std::string::npos || end == std::string::npos) {
        return GodotValue::Color(1, 1, 1, 1);
    }

    std::string inner = str.substr(start + 1, end - start - 1);
    auto args = SplitArgs(inner);

    double r = 1, g = 1, b = 1, a = 1;
    if (args.size() >= 1) r = std::stod(args[0]);
    if (args.size() >= 2) g = std::stod(args[1]);
    if (args.size() >= 3) b = std::stod(args[2]);
    if (args.size() >= 4) a = std::stod(args[3]);

    return GodotValue::Color(r, g, b, a);
}

GodotValue GodotSceneParser::ParseRect2(const std::string& str) {
    size_t start = str.find('(');
    size_t end = str.rfind(')');
    if (start == std::string::npos || end == std::string::npos) {
        GodotValue gv;
        gv.type = GodotValue::Type::Rect2;
        return gv;
    }

    std::string inner = str.substr(start + 1, end - start - 1);
    auto args = SplitArgs(inner);

    GodotValue gv;
    gv.type = GodotValue::Type::Rect2;
    if (args.size() >= 1) gv.x = std::stod(args[0]);
    if (args.size() >= 2) gv.y = std::stod(args[1]);
    if (args.size() >= 3) gv.z = std::stod(args[2]);  // width
    if (args.size() >= 4) gv.w = std::stod(args[3]);  // height

    return gv;
}

GodotValue GodotSceneParser::ParseTransform2D(const std::string& str) {
    GodotValue gv;
    gv.type = GodotValue::Type::Transform2D;
    // For now, just store as raw string - complex transform parsing can be added later
    gv.stringValue = str;
    return gv;
}

GodotValue GodotSceneParser::ParseTransform3D(const std::string& str) {
    GodotValue gv;
    gv.type = GodotValue::Type::Transform3D;
    gv.stringValue = str;
    return gv;
}

GodotValue GodotSceneParser::ParseQuat(const std::string& str) {
    size_t start = str.find('(');
    size_t end = str.rfind(')');
    if (start == std::string::npos || end == std::string::npos) {
        GodotValue gv;
        gv.type = GodotValue::Type::Quat;
        gv.w = 1;
        return gv;
    }

    std::string inner = str.substr(start + 1, end - start - 1);
    auto args = SplitArgs(inner);

    GodotValue gv;
    gv.type = GodotValue::Type::Quat;
    if (args.size() >= 1) gv.x = std::stod(args[0]);
    if (args.size() >= 2) gv.y = std::stod(args[1]);
    if (args.size() >= 3) gv.z = std::stod(args[2]);
    if (args.size() >= 4) gv.w = std::stod(args[3]);

    return gv;
}

GodotValue GodotSceneParser::ParseArray(const std::string& str) {
    GodotValue gv;
    gv.type = GodotValue::Type::Array;

    // Simple array parsing - doesn't handle nested structures well
    std::string inner = str.substr(1, str.length() - 2);
    if (inner.empty()) return gv;

    auto elements = SplitArgs(inner);
    for (const auto& elem : elements) {
        gv.arrayValue.push_back(ParseValue(elem));
    }

    return gv;
}

GodotValue GodotSceneParser::ParseDictionary(const std::string& str) {
    GodotValue gv;
    gv.type = GodotValue::Type::Dictionary;

    // Simple dictionary parsing
    std::string inner = str.substr(1, str.length() - 2);
    if (inner.empty()) return gv;

    // Split by commas (not inside strings or nested structures)
    // This is a simplified implementation
    size_t pos = 0;
    while (pos < inner.length()) {
        // Find the colon separating key and value
        size_t colonPos = inner.find(':', pos);
        if (colonPos == std::string::npos) break;

        std::string key = Trim(inner.substr(pos, colonPos - pos));
        if (key.front() == '"' && key.back() == '"') {
            key = key.substr(1, key.length() - 2);
        }

        // Find the end of this entry (next comma at same level)
        size_t valueStart = colonPos + 1;
        size_t valueEnd = inner.find(',', valueStart);
        if (valueEnd == std::string::npos) valueEnd = inner.length();

        std::string valueStr = Trim(inner.substr(valueStart, valueEnd - valueStart));
        gv.dictValue[key] = ParseValue(valueStr);

        pos = valueEnd + 1;
    }

    return gv;
}

GodotValue GodotSceneParser::ParseExtResourceRef(const std::string& str) {
    // Format: ExtResource("1_abc") or ExtResource(1)
    GodotValue gv;
    gv.type = GodotValue::Type::ExtResource;

    size_t start = str.find('(');
    size_t end = str.rfind(')');
    if (start != std::string::npos && end != std::string::npos) {
        std::string inner = Trim(str.substr(start + 1, end - start - 1));
        // Remove quotes if present
        if (inner.front() == '"' && inner.back() == '"') {
            inner = inner.substr(1, inner.length() - 2);
        }
        gv.resourceId = inner;
    }

    return gv;
}

GodotValue GodotSceneParser::ParseSubResourceRef(const std::string& str) {
    // Format: SubResource("CircleShape2D_abc")
    GodotValue gv;
    gv.type = GodotValue::Type::SubResource;

    size_t start = str.find('(');
    size_t end = str.rfind(')');
    if (start != std::string::npos && end != std::string::npos) {
        std::string inner = Trim(str.substr(start + 1, end - start - 1));
        if (inner.front() == '"' && inner.back() == '"') {
            inner = inner.substr(1, inner.length() - 2);
        }
        gv.resourceId = inner;
    }

    return gv;
}

GodotValue GodotSceneParser::ParseNodePath(const std::string& str) {
    // Format: NodePath("../Player")
    GodotValue gv;
    gv.type = GodotValue::Type::NodePath;

    size_t start = str.find('(');
    size_t end = str.rfind(')');
    if (start != std::string::npos && end != std::string::npos) {
        std::string inner = Trim(str.substr(start + 1, end - start - 1));
        if (inner.front() == '"' && inner.back() == '"') {
            inner = inner.substr(1, inner.length() - 2);
        }
        gv.stringValue = inner;
    }

    return gv;
}

std::string GodotSceneParser::Trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string GodotSceneParser::ExtractQuotedString(const std::string& str) {
    if (str.length() >= 2 && str.front() == '"' && str.back() == '"') {
        return str.substr(1, str.length() - 2);
    }
    return str;
}

std::map<std::string, std::string> GodotSceneParser::ParseSectionAttributes(const std::string& content) {
    std::map<std::string, std::string> attrs;

    // Parse key=value pairs
    std::regex attrRegex(R"((\w+)\s*=\s*(\"[^\"]*\"|[^\s\]]+))");
    std::sregex_iterator it(content.begin(), content.end(), attrRegex);
    std::sregex_iterator end;

    while (it != end) {
        std::string key = (*it)[1].str();
        std::string value = (*it)[2].str();

        // Remove quotes from value
        if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.length() - 2);
        }

        attrs[key] = value;
        ++it;
    }

    return attrs;
}

std::vector<std::string> GodotSceneParser::SplitArgs(const std::string& str) {
    std::vector<std::string> args;
    std::string current;
    int parenDepth = 0;
    bool inString = false;

    for (size_t i = 0; i < str.length(); i++) {
        char c = str[i];

        if (c == '"' && (i == 0 || str[i-1] != '\\')) {
            inString = !inString;
        }

        if (!inString) {
            if (c == '(' || c == '[' || c == '{') {
                parenDepth++;
            } else if (c == ')' || c == ']' || c == '}') {
                parenDepth--;
            }

            if (c == ',' && parenDepth == 0) {
                args.push_back(Trim(current));
                current.clear();
                continue;
            }
        }

        current += c;
    }

    if (!current.empty()) {
        args.push_back(Trim(current));
    }

    return args;
}

} // namespace import
} // namespace lupine
