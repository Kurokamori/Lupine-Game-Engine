#include "lupine/core/ScriptAnnotationParser.hpp"

#include <regex>
#include <sstream>
#include <cctype>
#include <algorithm>

namespace lupine {
namespace core {

namespace {

std::string Trim(const std::string& text) {
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

std::string ToLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::string StripQuotes(const std::string& text) {
    if (text.size() >= 2 &&
        ((text.front() == '"' && text.back() == '"') ||
         (text.front() == '\'' && text.back() == '\''))) {
        return text.substr(1, text.size() - 2);
    }
    return text;
}

// Strip a leading run of comment/doc punctuation (the comment prefix plus any
// additional doc markers such as the third '-' in Lua '---' or '##' in Python).
std::string StripCommentLead(const std::string& line, const std::string& commentPrefix) {
    std::string trimmed = Trim(line);
    if (trimmed.rfind(commentPrefix, 0) == 0) {
        trimmed = trimmed.substr(commentPrefix.size());
    }
    size_t i = 0;
    while (i < trimmed.size() && (trimmed[i] == '-' || trimmed[i] == '#' ||
                                  trimmed[i] == '/' || trimmed[i] == '*' ||
                                  trimmed[i] == ' ' || trimmed[i] == '\t')) {
        ++i;
    }
    return trimmed.substr(i);
}

std::vector<float> ParseFloatList(const std::string& text) {
    std::vector<float> values;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, ',')) {
        try {
            values.push_back(std::stof(Trim(item)));
        } catch (...) {
        }
    }
    return values;
}

// Split a bracket-attribute argument list on top-level commas, honouring quotes
// and backslash escapes, then trim and unquote each argument. Handles e.g.
//   color_ramp", "{\"stops\":5}   ->   ["color_ramp", "{\"stops\":5}"]
std::vector<std::string> SplitArgs(const std::string& raw) {
    std::vector<std::string> args;
    std::string current;
    bool inQuote = false;
    char quoteChar = '\0';
    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (inQuote) {
            if (c == '\\' && i + 1 < raw.size()) {
                char next = raw[i + 1];
                if (next == '"' || next == '\'' || next == '\\') {
                    current.push_back(next);
                    ++i;
                    continue;
                }
            }
            if (c == quoteChar) {
                inQuote = false;
                continue;
            }
            current.push_back(c);
        } else {
            if (c == '"' || c == '\'') {
                inQuote = true;
                quoteChar = c;
            } else if (c == ',') {
                args.push_back(Trim(current));
                current.clear();
            } else {
                current.push_back(c);
            }
        }
    }
    std::string tail = Trim(current);
    if (!tail.empty() || !args.empty()) {
        args.push_back(tail);
    }
    return args;
}

using TagMap = std::map<std::string, std::vector<std::string>>;

// Scan `text` for bracket attributes [Name] / [Name(args)] honouring quotes so a
// JSON config argument may itself contain brackets/commas. Adds each found tag to
// `tags` (key lower-cased; later occurrences overwrite earlier ones).
void CollectBracketTags(const std::string& text, TagMap& tags) {
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] != '[') {
            ++i;
            continue;
        }
        size_t j = i + 1;
        bool inQuote = false;
        char quoteChar = '\0';
        for (; j < text.size(); ++j) {
            char c = text[j];
            if (inQuote) {
                if (c == '\\' && j + 1 < text.size()) {
                    ++j;
                    continue;
                }
                if (c == quoteChar) {
                    inQuote = false;
                }
            } else if (c == '"' || c == '\'') {
                inQuote = true;
                quoteChar = c;
            } else if (c == ']') {
                break;
            }
        }
        if (j >= text.size()) {
            break;
        }
        std::string inner = text.substr(i + 1, j - (i + 1));
        i = j + 1;

        std::string name;
        std::string argsRaw;
        size_t paren = inner.find('(');
        if (paren != std::string::npos && inner.back() == ')') {
            name = Trim(inner.substr(0, paren));
            argsRaw = inner.substr(paren + 1, inner.size() - paren - 2);
        } else {
            name = Trim(inner);
        }
        if (name.empty()) {
            continue;
        }
        tags[ToLower(name)] = SplitArgs(argsRaw);
    }
}

// Collect legacy `@key value` annotations from `text` (value is a quoted string or
// a single whitespace-delimited token), matching the historical archetype syntax.
void CollectLegacyTags(const std::string& text, TagMap& tags) {
    static const std::regex annotationRegex(
        R"(@(\w+)(?:\s+\"([^\"]*)\"|\s+'([^']*)'|\s+([^\s@]+))?)");
    std::sregex_iterator begin(text.begin(), text.end(), annotationRegex);
    std::sregex_iterator end;
    for (std::sregex_iterator it = begin; it != end; ++it) {
        const std::smatch& m = *it;
        std::string key = ToLower(m[1].str());
        std::string value;
        if (m[2].matched) {
            value = m[2].str();
        } else if (m[3].matched) {
            value = m[3].str();
        } else if (m[4].matched) {
            value = m[4].str();
        }
        tags[key] = std::vector<std::string>{ value };
    }
}

bool HasTag(const TagMap& tags, const std::string& key) {
    return tags.find(key) != tags.end();
}

// Join multi-arg tag values with commas; a single value is returned verbatim so a
// legacy `@range "0,100,1"` and a bracket `[Range(0,100,1)]` both yield "0,100,1".
std::string TagStr(const TagMap& tags, const std::string& key) {
    TagMap::const_iterator it = tags.find(key);
    if (it == tags.end() || it->second.empty()) {
        return "";
    }
    if (it->second.size() == 1) {
        return it->second[0];
    }
    std::string joined;
    for (size_t i = 0; i < it->second.size(); ++i) {
        if (i > 0) {
            joined += ",";
        }
        joined += it->second[i];
    }
    return joined;
}

std::string TagArg(const TagMap& tags, const std::string& key, size_t index) {
    TagMap::const_iterator it = tags.find(key);
    if (it == tags.end() || index >= it->second.size()) {
        return "";
    }
    return it->second[index];
}

bool IsBuiltin(const std::string& t) {
    static const std::vector<std::string> kBuiltins = {
        "int", "integer", "float", "number", "str", "string", "bool", "boolean",
        "vec2", "vec3", "vec4", "color", "enum", "file", "dir", "multiline",
        "scenepath", "nodepath", "stringarray", "double", "quat", "quaternion",
        "rect", "rect2", "resource", "intarray", "floatarray", "array",
        "dictionary", "dict"
    };
    return std::find(kBuiltins.begin(), kBuiltins.end(), t) != kBuiltins.end();
}

// Resolve type + default for a built-in type token. Mirrors the historical
// archetype type table so existing scripts and .archetype files load unchanged.
void ApplyBuiltinType(PropertyDescriptor& desc, const std::string& typeName,
                      const std::string& defaultStr, const TagMap& tags) {
    if (typeName == "int" || typeName == "integer") {
        desc.type = PropertyValueType::Int;
        try { desc.defaultValue = std::stoi(defaultStr); } catch (...) { desc.defaultValue = 0; }
    } else if (typeName == "float" || typeName == "number") {
        desc.type = PropertyValueType::Float;
        try { desc.defaultValue = std::stof(defaultStr); } catch (...) { desc.defaultValue = 0.0f; }
    } else if (typeName == "double") {
        desc.type = PropertyValueType::Double;
        try { desc.defaultValue = std::stod(defaultStr); } catch (...) { desc.defaultValue = 0.0; }
    } else if (typeName == "str" || typeName == "string") {
        desc.type = PropertyValueType::String;
        desc.defaultValue = StripQuotes(defaultStr);
    } else if (typeName == "bool" || typeName == "boolean") {
        desc.type = PropertyValueType::Bool;
        std::string d = StripQuotes(defaultStr);
        desc.defaultValue = (d == "true" || d == "True" || d == "1");
    } else if (typeName == "vec2") {
        desc.type = PropertyValueType::Vec2;
        std::vector<float> v = ParseFloatList(StripQuotes(defaultStr));
        desc.defaultValue = math::Vec2(v.size() > 0 ? v[0] : 0.0f, v.size() > 1 ? v[1] : 0.0f);
    } else if (typeName == "vec3") {
        desc.type = PropertyValueType::Vec3;
        std::vector<float> v = ParseFloatList(StripQuotes(defaultStr));
        desc.defaultValue = math::Vec3(v.size() > 0 ? v[0] : 0.0f, v.size() > 1 ? v[1] : 0.0f,
                                       v.size() > 2 ? v[2] : 0.0f);
    } else if (typeName == "vec4") {
        desc.type = PropertyValueType::Vec4;
        std::vector<float> v = ParseFloatList(StripQuotes(defaultStr));
        desc.defaultValue = math::Vec4(v.size() > 0 ? v[0] : 0.0f, v.size() > 1 ? v[1] : 0.0f,
                                       v.size() > 2 ? v[2] : 0.0f, v.size() > 3 ? v[3] : 0.0f);
    } else if (typeName == "color") {
        desc.type = PropertyValueType::Color;
        std::vector<float> v = ParseFloatList(StripQuotes(defaultStr));
        desc.defaultValue = math::Color(v.size() > 0 ? v[0] : 1.0f, v.size() > 1 ? v[1] : 1.0f,
                                        v.size() > 2 ? v[2] : 1.0f, v.size() > 3 ? v[3] : 1.0f);
    } else if (typeName == "quat" || typeName == "quaternion") {
        desc.type = PropertyValueType::Quat;
        std::vector<float> v = ParseFloatList(StripQuotes(defaultStr));
        desc.defaultValue = math::Quat(v.size() > 0 ? v[0] : 1.0f, v.size() > 1 ? v[1] : 0.0f,
                                       v.size() > 2 ? v[2] : 0.0f, v.size() > 3 ? v[3] : 0.0f);
    } else if (typeName == "rect" || typeName == "rect2") {
        desc.type = PropertyValueType::Rect;
        std::vector<float> v = ParseFloatList(StripQuotes(defaultStr));
        desc.defaultValue = math::Rect(v.size() > 0 ? v[0] : 0.0f, v.size() > 1 ? v[1] : 0.0f,
                                       v.size() > 2 ? v[2] : 0.0f, v.size() > 3 ? v[3] : 0.0f);
    } else if (typeName == "enum") {
        desc.type = PropertyValueType::Enum;
        std::string enumList = TagStr(tags, "enum");
        desc.hint = PropertyHint(PropertyHintType::Enum, enumList);
        std::string d = StripQuotes(defaultStr);
        int index = 0;
        try {
            index = std::stoi(d);
        } catch (...) {
            std::vector<std::string> labels = desc.hint.GetEnumValues();
            for (size_t li = 0; li < labels.size(); ++li) {
                if (labels[li] == d) {
                    index = static_cast<int>(li);
                    break;
                }
            }
        }
        desc.defaultValue = index;
    } else if (typeName == "file") {
        desc.type = PropertyValueType::String;
        std::string filter = HasTag(tags, "file") ? TagStr(tags, "file") : "*.*";
        desc.hint = PropertyHint(PropertyHintType::File, filter);
        desc.defaultValue = StripQuotes(defaultStr);
    } else if (typeName == "dir") {
        desc.type = PropertyValueType::String;
        desc.hint = PropertyHint(PropertyHintType::Dir, TagStr(tags, "dir"));
        desc.defaultValue = StripQuotes(defaultStr);
    } else if (typeName == "multiline") {
        desc.type = PropertyValueType::String;
        desc.hint = PropertyHint(PropertyHintType::MultilineText, "");
        desc.defaultValue = StripQuotes(defaultStr);
    } else if (typeName == "scenepath") {
        desc.type = PropertyValueType::ScenePath;
        desc.defaultValue = StripQuotes(defaultStr);
    } else if (typeName == "nodepath") {
        desc.type = PropertyValueType::NodePath;
        desc.defaultValue = StripQuotes(defaultStr);
    } else if (typeName == "stringarray") {
        desc.type = PropertyValueType::StringArray;
        std::vector<std::string> arr;
        std::stringstream ss(StripQuotes(defaultStr));
        std::string item;
        while (std::getline(ss, item, ',')) {
            std::string trimmed = Trim(item);
            if (!trimmed.empty()) {
                arr.push_back(trimmed);
            }
        }
        desc.defaultValue = arr;
    } else if (typeName == "resource") {
        desc.type = PropertyValueType::Resource;
        std::string filter = HasTag(tags, "resource") ? TagStr(tags, "resource")
                           : (HasTag(tags, "file") ? TagStr(tags, "file") : "*.ares");
        desc.hint = PropertyHint(PropertyHintType::File, filter);
        desc.defaultValue = StripQuotes(defaultStr);
    } else if (typeName == "intarray") {
        desc.type = PropertyValueType::IntArray;
        nlohmann::json arr = nlohmann::json::array();
        std::stringstream ss(StripQuotes(defaultStr));
        std::string item;
        while (std::getline(ss, item, ',')) {
            try { arr.push_back(std::stoi(Trim(item))); } catch (...) {}
        }
        desc.defaultValue = PropertyJsonDefault(arr);
    } else if (typeName == "floatarray") {
        desc.type = PropertyValueType::FloatArray;
        nlohmann::json arr = nlohmann::json::array();
        std::stringstream ss(StripQuotes(defaultStr));
        std::string item;
        while (std::getline(ss, item, ',')) {
            try { arr.push_back(std::stof(Trim(item))); } catch (...) {}
        }
        desc.defaultValue = PropertyJsonDefault(arr);
    } else if (typeName == "array") {
        desc.type = PropertyValueType::Array;
        nlohmann::json parsed = nlohmann::json::array();
        std::string body = StripQuotes(defaultStr);
        if (!body.empty()) {
            try {
                nlohmann::json candidate = nlohmann::json::parse(body);
                if (candidate.is_array()) {
                    parsed = candidate;
                }
            } catch (...) {}
        }
        desc.defaultValue = PropertyJsonDefault(parsed);
    } else if (typeName == "dictionary" || typeName == "dict") {
        desc.type = PropertyValueType::Dictionary;
        nlohmann::json parsed = nlohmann::json::object();
        std::string body = StripQuotes(defaultStr);
        if (!body.empty()) {
            try {
                nlohmann::json candidate = nlohmann::json::parse(body);
                if (candidate.is_object()) {
                    parsed = candidate;
                }
            } catch (...) {}
        }
        desc.defaultValue = PropertyJsonDefault(parsed);
    }
}

// Apply general (non-type) metadata tags and any explicit hint override. Called
// after the type has been resolved so a hint tag wins over the type-derived hint.
void ApplyGeneralTags(PropertyDescriptor& desc, const TagMap& tags) {
    if (HasTag(tags, "tooltip")) {
        desc.description = TagStr(tags, "tooltip");
    } else if (HasTag(tags, "desc")) {
        desc.description = TagStr(tags, "desc");
    }
    if (HasTag(tags, "header")) {
        desc.headerText = TagStr(tags, "header");
    }
    if (HasTag(tags, "group")) {
        desc.group = TagStr(tags, "group");
    }
    if (HasTag(tags, "suffix")) {
        desc.suffix = TagStr(tags, "suffix");
    }
    if (HasTag(tags, "custom")) {
        desc.customWidget = TagArg(tags, "custom", 0);
        desc.customWidgetConfig = TagArg(tags, "custom", 1);
    }

    if (HasTag(tags, "hideininspector") || HasTag(tags, "hidden")) {
        desc.usageFlags |= PropertyUsageFlags::Hidden;
    }
    if (HasTag(tags, "readonly")) {
        desc.usageFlags |= PropertyUsageFlags::ReadOnly;
    }
    if (HasTag(tags, "noserialize")) {
        desc.usageFlags |= PropertyUsageFlags::NoSerialize;
    }
    if (HasTag(tags, "required")) {
        desc.usageFlags |= PropertyUsageFlags::Required;
    }
    if (HasTag(tags, "unique")) {
        desc.usageFlags |= PropertyUsageFlags::Unique;
    }
    if (HasTag(tags, "experimental")) {
        desc.usageFlags |= PropertyUsageFlags::Experimental;
    }
    if (HasTag(tags, "advanced")) {
        desc.usageFlags |= PropertyUsageFlags::Advanced;
    }

    // Hint override (single slot; first matching tag wins by precedence).
    if (HasTag(tags, "range") || HasTag(tags, "step")) {
        std::vector<float> comps = ParseFloatList(TagStr(tags, "range"));
        std::string minStr = comps.size() > 0 ? Trim(std::to_string(comps[0])) : "";
        std::string maxStr = comps.size() > 1 ? Trim(std::to_string(comps[1])) : "";
        std::string stepStr = comps.size() > 2 ? Trim(std::to_string(comps[2])) : "";
        if (HasTag(tags, "step")) {
            stepStr = TagStr(tags, "step");
        }
        std::string hintStr = minStr + "," + maxStr;
        if (!stepStr.empty()) {
            hintStr += "," + stepStr;
        }
        desc.hint = PropertyHint(PropertyHintType::Range, hintStr);
    } else if (HasTag(tags, "exprange")) {
        desc.hint = PropertyHint(PropertyHintType::ExpRange, TagStr(tags, "exprange"));
    } else if (HasTag(tags, "enum") && desc.type == PropertyValueType::Enum) {
        desc.hint = PropertyHint(PropertyHintType::Enum, TagStr(tags, "enum"));
    } else if (HasTag(tags, "flags")) {
        desc.hint = PropertyHint(PropertyHintType::Flags, TagStr(tags, "flags"));
    } else if (HasTag(tags, "multiline")) {
        desc.hint = PropertyHint(PropertyHintType::MultilineText, "");
    } else if (HasTag(tags, "colornoalpha")) {
        desc.hint = PropertyHint(PropertyHintType::ColorNoAlpha, "");
    } else if (HasTag(tags, "layers2d")) {
        desc.hint = PropertyHint(PropertyHintType::Layers2D, "");
    } else if (HasTag(tags, "layers3d")) {
        desc.hint = PropertyHint(PropertyHintType::Layers3D, "");
    } else if (HasTag(tags, "nodetype")) {
        desc.hint = PropertyHint(PropertyHintType::NodeType, TagStr(tags, "nodetype"));
    } else if (HasTag(tags, "archetypetype")) {
        desc.hint = PropertyHint(PropertyHintType::ArchetypeType, TagStr(tags, "archetypetype"));
    } else if (HasTag(tags, "scriptclass")) {
        desc.hint = PropertyHint(PropertyHintType::ScriptClass, TagStr(tags, "scriptclass"));
    } else if (HasTag(tags, "expeasing")) {
        desc.hint = PropertyHint(PropertyHintType::ExpEasing, "");
    } else if (HasTag(tags, "file")) {
        desc.hint = PropertyHint(PropertyHintType::File, TagStr(tags, "file"));
    } else if (HasTag(tags, "dir")) {
        desc.hint = PropertyHint(PropertyHintType::Dir, TagStr(tags, "dir"));
    } else if (HasTag(tags, "resource")) {
        desc.hint = PropertyHint(PropertyHintType::ArchetypeType, TagStr(tags, "resource"));
    }
}

// Resolve a single export declaration's type token into the descriptor, choosing
// between built-in types, declared structs, and script-class references.
void ResolveType(PropertyDescriptor& desc, const std::string& originalType,
                 const std::string& defaultStr, const TagMap& tags,
                 const std::map<std::string, std::vector<PropertyDescriptor>>& structs) {
    std::string typeLower = ToLower(originalType);
    if (IsBuiltin(typeLower)) {
        ApplyBuiltinType(desc, typeLower, defaultStr, tags);
        return;
    }

    std::map<std::string, std::vector<PropertyDescriptor>>::const_iterator structIt =
        structs.find(originalType);
    if (structIt != structs.end()) {
        desc.type = PropertyValueType::Dictionary;
        desc.objectTypeName = originalType;
        desc.objectSchema = structIt->second;
        nlohmann::json obj = nlohmann::json::object();
        for (const PropertyDescriptor& field : structIt->second) {
            obj[field.name] = field.GetDefaultAsJson();
        }
        desc.defaultValue = PropertyJsonDefault(obj);
        return;
    }

    // Unknown token -> reference to a script/component class by name.
    desc.type = PropertyValueType::NodePath;
    desc.hint = PropertyHint(PropertyHintType::ScriptClass, originalType);
    desc.defaultValue = StripQuotes(defaultStr);
}

// Split an export line's remainder ("rest") into the default token/string and the
// trailing annotation text. Mirrors the historical archetype extraction.
void SplitDefaultAndTrailing(const std::string& rest, std::string& defaultStr,
                             std::string& trailing) {
    size_t i = 0;
    while (i < rest.size() && std::isspace(static_cast<unsigned char>(rest[i]))) {
        ++i;
    }
    if (i >= rest.size()) {
        return;
    }
    if (rest[i] == '@' || rest[i] == '[') {
        trailing = rest.substr(i);
    } else if (rest[i] == '"' || rest[i] == '\'') {
        char quote = rest[i];
        size_t close = rest.find(quote, i + 1);
        if (close != std::string::npos) {
            defaultStr = rest.substr(i + 1, close - (i + 1));
            trailing = rest.substr(close + 1);
        } else {
            defaultStr = rest.substr(i + 1);
        }
    } else {
        size_t tokenEnd = i;
        while (tokenEnd < rest.size() && !std::isspace(static_cast<unsigned char>(rest[tokenEnd]))) {
            ++tokenEnd;
        }
        defaultStr = rest.substr(i, tokenEnd - i);
        trailing = rest.substr(tokenEnd);
    }
}

// Parse a struct field list body ("hp:int=10, mp:int=5, name:string=\"hero\"").
std::vector<PropertyDescriptor> ParseStructBody(const std::string& body) {
    std::vector<PropertyDescriptor> fields;
    for (const std::string& rawField : SplitArgs(body)) {
        std::string field = Trim(rawField);
        if (field.empty()) {
            continue;
        }
        size_t colon = field.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        std::string fieldName = Trim(field.substr(0, colon));
        std::string typeAndDefault = Trim(field.substr(colon + 1));
        std::string fieldType = typeAndDefault;
        std::string fieldDefault;
        size_t eq = typeAndDefault.find('=');
        if (eq != std::string::npos) {
            fieldType = Trim(typeAndDefault.substr(0, eq));
            fieldDefault = Trim(typeAndDefault.substr(eq + 1));
        }
        if (fieldName.empty() || fieldType.empty()) {
            continue;
        }
        PropertyDescriptor field_desc;
        field_desc.name = fieldName;
        TagMap noTags;
        std::string typeLower = ToLower(fieldType);
        if (IsBuiltin(typeLower)) {
            ApplyBuiltinType(field_desc, typeLower, fieldDefault, noTags);
        } else {
            field_desc.type = PropertyValueType::String;
            field_desc.defaultValue = StripQuotes(fieldDefault);
        }
        fields.push_back(field_desc);
    }
    return fields;
}

bool IsCommentLine(const std::string& line, const std::string& commentPrefix) {
    return Trim(line).rfind(commentPrefix, 0) == 0;
}

// A comment body that begins with one of these directives is structural, not part
// of a property's documentation block.
bool IsReservedDirectiveLine(const std::string& body) {
    static const std::vector<std::string> kReserved = {
        "export", "export_group", "export_category", "struct", "signal", "rpc",
        "interface", "extends", "component_class", "extends_component",
        "archetype_class", "archetype_extends", "archetype_menu", "archetype_icon",
        "archetype_description", "archetype_abstract"
    };
    if (body.empty() || body[0] != '@') {
        return false;
    }
    size_t end = 1;
    while (end < body.size() && (std::isalnum(static_cast<unsigned char>(body[end])) || body[end] == '_')) {
        ++end;
    }
    std::string word = body.substr(1, end - 1);
    return std::find(kReserved.begin(), kReserved.end(), word) != kReserved.end();
}

} // namespace

bool ScriptAnnotationParser::IsBuiltinTypeToken(const std::string& typeNameLower) {
    return IsBuiltin(typeNameLower);
}

ScriptExportParseResult ScriptAnnotationParser::ParseExports(const std::string& content,
                                                             const std::string& commentPrefix) {
    ScriptExportParseResult result;

    const std::regex groupRegex(commentPrefix + R"(\s*@export_group\s+[\"']([^\"']+)[\"'])");
    const std::regex groupEndRegex(commentPrefix + R"(\s*@export_group_end)");
    const std::regex categoryRegex(commentPrefix + R"(\s*@export_category\s+[\"']([^\"']+)[\"'])");
    const std::regex exportRegex(commentPrefix + R"(\s*@export\s+(\w+)\s+([\w]+)\s*(.*))");
    const std::regex structStartRegex(commentPrefix + R"(\s*@struct\s+(\w+)\s*\{(.*))");

    // --- Pass 1: collect @struct definitions (may reference forward) ---
    {
        std::istringstream stream(content);
        std::string line;
        std::smatch match;
        bool inStruct = false;
        std::string structName;
        std::string structBody;
        while (std::getline(stream, line)) {
            if (inStruct) {
                std::string body = StripCommentLead(line, commentPrefix);
                size_t close = body.find('}');
                if (close != std::string::npos) {
                    structBody += " " + body.substr(0, close);
                    result.structs[structName] = ParseStructBody(structBody);
                    inStruct = false;
                } else {
                    structBody += " " + body;
                }
                continue;
            }
            if (std::regex_search(line, match, structStartRegex)) {
                structName = match[1].str();
                std::string remainder = match[2].str();
                size_t close = remainder.find('}');
                if (close != std::string::npos) {
                    result.structs[structName] = ParseStructBody(remainder.substr(0, close));
                } else {
                    inStruct = true;
                    structBody = remainder;
                }
            }
        }
    }

    // --- Pass 2: parse exports with preceding-block annotations ---
    std::istringstream stream(content);
    std::string line;
    std::smatch match;
    std::string currentGroup;
    TagMap blockTags;
    std::string blockProse;
    bool inStruct = false;

    auto clearBlock = [&]() {
        blockTags.clear();
        blockProse.clear();
    };

    while (std::getline(stream, line)) {
        // Skip struct bodies (already handled in pass 1).
        if (inStruct) {
            if (StripCommentLead(line, commentPrefix).find('}') != std::string::npos) {
                inStruct = false;
            }
            clearBlock();
            continue;
        }
        if (std::regex_search(line, match, structStartRegex)) {
            if (match[2].str().find('}') == std::string::npos) {
                inStruct = true;
            }
            clearBlock();
            continue;
        }

        if (std::regex_search(line, match, groupRegex)) {
            currentGroup = match[1].str();
            clearBlock();
            continue;
        }
        if (std::regex_search(line, match, categoryRegex)) {
            currentGroup = match[1].str();
            clearBlock();
            continue;
        }
        if (std::regex_search(line, match, groupEndRegex)) {
            currentGroup.clear();
            clearBlock();
            continue;
        }

        if (std::regex_search(line, match, exportRegex)) {
            std::string varName = match[1].str();
            std::string typeName = match[2].str();
            std::string rest = match[3].str();

            std::string defaultStr;
            std::string trailing;
            SplitDefaultAndTrailing(rest, defaultStr, trailing);

            // Merge tags: block first, trailing overrides (closest to the decl).
            TagMap tags = blockTags;
            CollectBracketTags(trailing, tags);
            CollectLegacyTags(trailing, tags);

            PropertyDescriptor descriptor;
            descriptor.name = varName;
            descriptor.group = currentGroup;

            ResolveType(descriptor, typeName, defaultStr, tags, result.structs);
            ApplyGeneralTags(descriptor, tags);

            // Prose block becomes the description unless a tooltip/desc tag set it.
            if (descriptor.description.empty() && !blockProse.empty()) {
                descriptor.description = Trim(blockProse);
            }

            result.properties.push_back(descriptor);
            clearBlock();
            continue;
        }

        // Accumulate documentation-block comment lines immediately preceding an export.
        if (IsCommentLine(line, commentPrefix)) {
            std::string body = StripCommentLead(line, commentPrefix);
            if (IsReservedDirectiveLine(body)) {
                clearBlock();
                continue;
            }
            CollectBracketTags(body, blockTags);
            // Strip bracket tags from the body to detect prose / legacy @-tags.
            std::string noBrackets;
            {
                bool inQuote = false;
                char quoteChar = '\0';
                int depth = 0;
                for (char c : body) {
                    if (inQuote) {
                        if (c == quoteChar) inQuote = false;
                    } else if (c == '"' || c == '\'') {
                        inQuote = true;
                        quoteChar = c;
                    } else if (c == '[') {
                        ++depth;
                        continue;
                    } else if (c == ']') {
                        if (depth > 0) --depth;
                        continue;
                    }
                    if (depth == 0) {
                        noBrackets.push_back(c);
                    }
                }
            }
            std::string trimmedRemainder = Trim(noBrackets);
            if (!trimmedRemainder.empty()) {
                if (trimmedRemainder[0] == '@') {
                    CollectLegacyTags(trimmedRemainder, blockTags);
                } else {
                    if (!blockProse.empty()) {
                        blockProse += " ";
                    }
                    blockProse += trimmedRemainder;
                }
            }
            continue;
        }

        // A blank line or a code line breaks the preceding-block adjacency.
        clearBlock();
    }

    return result;
}

} // namespace core
} // namespace lupine
