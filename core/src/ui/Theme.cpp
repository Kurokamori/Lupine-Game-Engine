#include "lupine/ui/Theme.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>
#include <cctype>

namespace lupine {
namespace ui {

namespace {

int HexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool ParseHexColor(const std::string& s, math::Color& out) {
    if (s.empty() || s[0] != '#') {
        return false;
    }
    std::string h = s.substr(1);
    if (h.size() != 6 && h.size() != 8) {
        return false;
    }
    int values[4] = {0, 0, 0, 255};
    size_t channels = h.size() / 2;
    for (size_t i = 0; i < channels; ++i) {
        int hi = HexNibble(h[i * 2]);
        int lo = HexNibble(h[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        values[i] = hi * 16 + lo;
    }
    out = math::Color(values[0] / 255.0f, values[1] / 255.0f,
                      values[2] / 255.0f, values[3] / 255.0f);
    return true;
}

// Split a binding string of the form "<prefix>:<key>". Returns the key, or the
// whole string if no recognised prefix is present.
std::string StripBindPrefix(const std::string& bind, const std::string& prefix) {
    if (bind.size() > prefix.size() && bind.compare(0, prefix.size(), prefix) == 0) {
        return bind.substr(prefix.size());
    }
    return bind;
}

} // namespace

// ============================================================== ColorOp helpers

const char* ColorOpToString(ColorOp op) {
    switch (op) {
        case ColorOp::None:       return "none";
        case ColorOp::Lighten:    return "lighten";
        case ColorOp::Darken:     return "darken";
        case ColorOp::Alpha:      return "alpha";
        case ColorOp::Saturate:   return "saturate";
        case ColorOp::Desaturate: return "desaturate";
        default:                  return "none";
    }
}

ColorOp ColorOpFromString(const std::string& s) {
    if (s == "lighten")    return ColorOp::Lighten;
    if (s == "darken")     return ColorOp::Darken;
    if (s == "alpha")      return ColorOp::Alpha;
    if (s == "saturate")   return ColorOp::Saturate;
    if (s == "desaturate") return ColorOp::Desaturate;
    return ColorOp::None;
}

math::Color ApplyColorOp(const math::Color& color, ColorOp op, float amount) {
    math::Color c = color;
    switch (op) {
        case ColorOp::None:
            return c;
        case ColorOp::Lighten:
            c.r = c.r + (1.0f - c.r) * amount;
            c.g = c.g + (1.0f - c.g) * amount;
            c.b = c.b + (1.0f - c.b) * amount;
            break;
        case ColorOp::Darken:
            c.r = c.r * (1.0f - amount);
            c.g = c.g * (1.0f - amount);
            c.b = c.b * (1.0f - amount);
            break;
        case ColorOp::Alpha:
            c.a = amount;
            break;
        case ColorOp::Desaturate: {
            float lum = 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
            c.r = c.r + (lum - c.r) * amount;
            c.g = c.g + (lum - c.g) * amount;
            c.b = c.b + (lum - c.b) * amount;
            break;
        }
        case ColorOp::Saturate: {
            float lum = 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
            c.r = lum + (c.r - lum) * (1.0f + amount);
            c.g = lum + (c.g - lum) * (1.0f + amount);
            c.b = lum + (c.b - lum) * (1.0f + amount);
            break;
        }
    }
    return c.Clamped();
}

// ================================================================ Colour JSON

bool ParseColorJson(const nlohmann::json& json, math::Color& out) {
    if (json.is_array()) {
        if (json.size() == 4) {
            out = math::Color(json[0].get<float>(), json[1].get<float>(),
                              json[2].get<float>(), json[3].get<float>());
            return true;
        }
        if (json.size() == 3) {
            out = math::Color(json[0].get<float>(), json[1].get<float>(),
                              json[2].get<float>(), 1.0f);
            return true;
        }
        return false;
    }
    if (json.is_object() && json.contains("r") && json.contains("g") && json.contains("b")) {
        float a = json.contains("a") ? json["a"].get<float>() : 1.0f;
        out = math::Color(json["r"].get<float>(), json["g"].get<float>(),
                          json["b"].get<float>(), a);
        return true;
    }
    if (json.is_string()) {
        return ParseHexColor(json.get<std::string>(), out);
    }
    return false;
}

nlohmann::json SerializeColorJson(const math::Color& color) {
    return nlohmann::json::array({ color.r, color.g, color.b, color.a });
}

// ================================================================= ThemeColor

ThemeColor ThemeColor::Literal(const math::Color& c) {
    ThemeColor tc;
    tc.bound = false;
    tc.literal = c;
    return tc;
}

ThemeColor ThemeColor::Bind(const std::string& key, ColorOp op, float amount) {
    ThemeColor tc;
    tc.bound = true;
    tc.paletteKey = key;
    tc.op = op;
    tc.opAmount = amount;
    return tc;
}

nlohmann::json ThemeColor::ToJson() const {
    nlohmann::json json = nlohmann::json::object();
    if (bound) {
        json["bind"] = "palette:" + paletteKey;
        if (op != ColorOp::None) {
            json["op"] = ColorOpToString(op);
            json["amount"] = opAmount;
        }
    } else {
        json["value"] = SerializeColorJson(literal);
    }
    return json;
}

ThemeColor ThemeColor::FromJson(const nlohmann::json& json) {
    ThemeColor tc;
    if (json.is_object()) {
        if (json.contains("bind") && json["bind"].is_string()) {
            tc.bound = true;
            tc.paletteKey = StripBindPrefix(json["bind"].get<std::string>(), "palette:");
            if (json.contains("op") && json["op"].is_string()) {
                tc.op = ColorOpFromString(json["op"].get<std::string>());
            }
            if (json.contains("amount") && json["amount"].is_number()) {
                tc.opAmount = json["amount"].get<float>();
            }
            return tc;
        }
        if (json.contains("value")) {
            tc.bound = false;
            ParseColorJson(json["value"], tc.literal);
            return tc;
        }
        // Bare colour object {r,g,b,a}.
        tc.bound = false;
        ParseColorJson(json, tc.literal);
        return tc;
    }
    if (json.is_string()) {
        std::string s = json.get<std::string>();
        if (s.compare(0, 8, "palette:") == 0) {
            tc.bound = true;
            tc.paletteKey = s.substr(8);
            return tc;
        }
        tc.bound = false;
        ParseHexColor(s, tc.literal);
        return tc;
    }
    if (json.is_array()) {
        tc.bound = false;
        ParseColorJson(json, tc.literal);
        return tc;
    }
    return tc;
}

// ================================================================ ThemeScalar

ThemeScalar ThemeScalar::LiteralValue(float v) {
    ThemeScalar ts;
    ts.bound = false;
    ts.literal = v;
    return ts;
}

ThemeScalar ThemeScalar::Bind(const std::string& key) {
    ThemeScalar ts;
    ts.bound = true;
    ts.variableKey = key;
    return ts;
}

nlohmann::json ThemeScalar::ToJson() const {
    nlohmann::json json = nlohmann::json::object();
    if (bound) {
        json["bind"] = "var:" + variableKey;
    } else {
        json["value"] = literal;
    }
    return json;
}

ThemeScalar ThemeScalar::FromJson(const nlohmann::json& json) {
    ThemeScalar ts;
    if (json.is_object()) {
        if (json.contains("bind") && json["bind"].is_string()) {
            ts.bound = true;
            ts.variableKey = StripBindPrefix(json["bind"].get<std::string>(), "var:");
            return ts;
        }
        if (json.contains("value") && json["value"].is_number()) {
            ts.bound = false;
            ts.literal = json["value"].get<float>();
            return ts;
        }
        return ts;
    }
    if (json.is_number()) {
        ts.bound = false;
        ts.literal = json.get<float>();
        return ts;
    }
    if (json.is_string()) {
        std::string s = json.get<std::string>();
        if (s.compare(0, 4, "var:") == 0) {
            ts.bound = true;
            ts.variableKey = s.substr(4);
        }
        return ts;
    }
    return ts;
}

// ================================================================== ThemeVec2

ThemeVec2 ThemeVec2::LiteralValue(const math::Vec2& v) {
    ThemeVec2 tv;
    tv.literal = v;
    return tv;
}

nlohmann::json ThemeVec2::ToJson() const {
    nlohmann::json json = nlohmann::json::object();
    json["value"] = nlohmann::json::array({ literal.x, literal.y });
    return json;
}

ThemeVec2 ThemeVec2::FromJson(const nlohmann::json& json) {
    ThemeVec2 tv;
    auto parse = [&tv](const nlohmann::json& v) {
        if (v.is_array() && v.size() >= 2 && v[0].is_number() && v[1].is_number()) {
            tv.literal.x = v[0].get<float>();
            tv.literal.y = v[1].get<float>();
        } else if (v.is_object() && v.contains("x") && v.contains("y") &&
                   v["x"].is_number() && v["y"].is_number()) {
            tv.literal.x = v["x"].get<float>();
            tv.literal.y = v["y"].get<float>();
        }
    };
    if (json.is_object()) {
        if (json.contains("value")) {
            parse(json["value"]);
        }
    } else {
        parse(json);
    }
    return tv;
}

// ================================================================== ThemeBool

ThemeBool ThemeBool::LiteralValue(bool v) {
    ThemeBool tb;
    tb.literal = v;
    return tb;
}

nlohmann::json ThemeBool::ToJson() const {
    nlohmann::json json = nlohmann::json::object();
    json["value"] = literal;
    return json;
}

ThemeBool ThemeBool::FromJson(const nlohmann::json& json) {
    ThemeBool tb;
    if (json.is_object()) {
        if (json.contains("value") && json["value"].is_boolean()) {
            tb.literal = json["value"].get<bool>();
        }
    } else if (json.is_boolean()) {
        tb.literal = json.get<bool>();
    }
    return tb;
}

// ================================================================== ThemeFont

nlohmann::json ThemeFont::ToJson() const {
    nlohmann::json json = nlohmann::json::object();
    if (!role.empty()) {
        json["role"] = role;
    } else {
        json["font"] = fontPath;
        json["size"] = size;
    }
    return json;
}

ThemeFont ThemeFont::FromJson(const nlohmann::json& json) {
    ThemeFont tf;
    if (json.is_object()) {
        if (json.contains("role") && json["role"].is_string()) {
            tf.role = json["role"].get<std::string>();
            return tf;
        }
        if (json.contains("font") && json["font"].is_string()) {
            tf.fontPath = json["font"].get<std::string>();
        }
        if (json.contains("size") && json["size"].is_number()) {
            tf.size = json["size"].get<float>();
        }
        return tf;
    }
    if (json.is_string()) {
        tf.fontPath = json.get<std::string>();
    }
    return tf;
}

// =================================================================== ThemeImage

ThemeImage ThemeImage::LiteralPath(const std::string& p) {
    ThemeImage ti;
    ti.path = p;
    return ti;
}

static const char* StretchModeToString(int mode) {
    switch (mode) {
        case 1: return "keep_centered";
        case 2: return "nine_slice";
        default: return "stretch";
    }
}

static int StretchModeFromString(const std::string& s) {
    if (s == "keep_centered") return 1;
    if (s == "nine_slice") return 2;
    return 0;
}

static const char* ImageAxisToString(int mode) {
    return mode == 1 ? "tile" : "stretch";
}

static int ImageAxisFromString(const std::string& s) {
    return s == "tile" ? 1 : 0;
}

nlohmann::json ThemeImage::ToJson() const {
    nlohmann::json json = nlohmann::json::object();
    json["path"] = path;
    if (hasStretch) {
        json["stretch_mode"] = StretchModeToString(stretchMode);
        if (stretchMode == 2) {
            nlohmann::json ns = nlohmann::json::object();
            ns["left"] = marginLeft;
            ns["top"] = marginTop;
            ns["right"] = marginRight;
            ns["bottom"] = marginBottom;
            ns["axis_h"] = ImageAxisToString(axisH);
            ns["axis_v"] = ImageAxisToString(axisV);
            ns["draw_center"] = drawCenter;
            json["nine_slice"] = ns;
        }
    }
    return json;
}

ThemeImage ThemeImage::FromJson(const nlohmann::json& json) {
    ThemeImage ti;
    if (json.is_object()) {
        if (json.contains("path") && json["path"].is_string()) {
            ti.path = json["path"].get<std::string>();
        }
        if (json.contains("stretch_mode") && json["stretch_mode"].is_string()) {
            ti.hasStretch = true;
            ti.stretchMode = StretchModeFromString(json["stretch_mode"].get<std::string>());
        }
        if (json.contains("nine_slice") && json["nine_slice"].is_object()) {
            const nlohmann::json& ns = json["nine_slice"];
            if (ns.contains("left") && ns["left"].is_number())   ti.marginLeft   = ns["left"].get<float>();
            if (ns.contains("top") && ns["top"].is_number())      ti.marginTop    = ns["top"].get<float>();
            if (ns.contains("right") && ns["right"].is_number())  ti.marginRight  = ns["right"].get<float>();
            if (ns.contains("bottom") && ns["bottom"].is_number())ti.marginBottom = ns["bottom"].get<float>();
            if (ns.contains("axis_h") && ns["axis_h"].is_string())ti.axisH = ImageAxisFromString(ns["axis_h"].get<std::string>());
            if (ns.contains("axis_v") && ns["axis_v"].is_string())ti.axisV = ImageAxisFromString(ns["axis_v"].get<std::string>());
            if (ns.contains("draw_center") && ns["draw_center"].is_boolean()) ti.drawCenter = ns["draw_center"].get<bool>();
        }
        return ti;
    }
    if (json.is_string()) {
        ti.path = json.get<std::string>();
    }
    return ti;
}

// =============================================================== ThemeStyleBox

nlohmann::json ThemeStyleBox::ToJson() const {
    nlohmann::json json = nlohmann::json::object();
    if (IsDerived()) {
        json["derive_from"] = deriveFrom;
        json["op"] = ColorOpToString(deriveOp);
        json["amount"] = deriveAmount;
        return json;
    }
    if (base) {
        json["stylebox"] = base->Serialize();
    }
    if (!colorBindings.empty()) {
        nlohmann::json bindings = nlohmann::json::object();
        for (const std::pair<const std::string, ThemeColor>& kv : colorBindings) {
            bindings[kv.first] = kv.second.ToJson();
        }
        json["bindings"] = bindings;
    }
    return json;
}

ThemeStyleBox ThemeStyleBox::FromJson(const nlohmann::json& json) {
    ThemeStyleBox sb;
    if (!json.is_object()) {
        return sb;
    }
    if (json.contains("derive_from") && json["derive_from"].is_string()) {
        sb.deriveFrom = json["derive_from"].get<std::string>();
        if (json.contains("op") && json["op"].is_string()) {
            sb.deriveOp = ColorOpFromString(json["op"].get<std::string>());
        }
        if (json.contains("amount") && json["amount"].is_number()) {
            sb.deriveAmount = json["amount"].get<float>();
        }
        return sb;
    }
    if (json.contains("stylebox") && json["stylebox"].is_object()) {
        // The embedded document carries a "type" field (StyleBox::Serialize), so a
        // themed stylebox can be any concrete subtype. Older themes that omit the
        // type fall back to a flat box for backward compatibility.
        sb.base = components::StyleBox::CreateFromJson(json["stylebox"]);
        if (!sb.base) {
            sb.base = std::make_shared<components::StyleBoxFlat>();
            sb.base->Deserialize(json["stylebox"]);
        }
    } else {
        sb.base = std::make_shared<components::StyleBoxFlat>();
    }
    if (json.contains("bindings") && json["bindings"].is_object()) {
        for (nlohmann::json::const_iterator it = json["bindings"].begin();
             it != json["bindings"].end(); ++it) {
            sb.colorBindings[it.key()] = ThemeColor::FromJson(it.value());
        }
    }
    return sb;
}

// =================================================================== Palette

bool ParsePaletteJson(const nlohmann::json& json, std::vector<PaletteSlot>& outSlots) {
    const nlohmann::json* slotsArray = nullptr;
    if (json.is_array()) {
        slotsArray = &json;
    } else if (json.is_object() && json.contains("slots") && json["slots"].is_array()) {
        slotsArray = &json["slots"];
    } else {
        return false;
    }

    outSlots.clear();
    for (const nlohmann::json& slotJson : *slotsArray) {
        if (!slotJson.is_object() || !slotJson.contains("key") || !slotJson["key"].is_string()) {
            continue;
        }
        PaletteSlot slot;
        slot.key = slotJson["key"].get<std::string>();
        slot.name = slotJson.contains("name") && slotJson["name"].is_string()
                        ? slotJson["name"].get<std::string>()
                        : slot.key;
        if (slotJson.contains("color")) {
            ParseColorJson(slotJson["color"], slot.color);
        }
        outSlots.push_back(slot);
    }
    return true;
}

bool ParsePaletteFile(const std::string& filepath, std::vector<PaletteSlot>& outSlots) {
    std::string contents;
    if (!ThemeAsset::ReadFileContents(filepath, contents)) {
        return false;
    }
    try {
        nlohmann::json json = nlohmann::json::parse(contents);
        return ParsePaletteJson(json, outSlots);
    } catch (const std::exception& e) {
        LOG_ERROR(LogCategory::UI, "Theme: failed to parse palette '{}': {}", filepath, e.what());
        return false;
    }
}

nlohmann::json SerializePaletteSlots(const std::string& name, const std::vector<PaletteSlot>& slots) {
    nlohmann::json json = nlohmann::json::object();
    json["lupine_palette"] = 1;
    json["name"] = name;
    nlohmann::json slotsArray = nlohmann::json::array();
    for (const PaletteSlot& slot : slots) {
        nlohmann::json slotJson = nlohmann::json::object();
        slotJson["key"] = slot.key;
        slotJson["name"] = slot.name;
        slotJson["color"] = SerializeColorJson(slot.color);
        slotsArray.push_back(slotJson);
    }
    json["slots"] = slotsArray;
    return json;
}

// ================================================================ ThemeAsset

ThemeAsset::ThemeAsset()
    : asset::Asset() {
}

ThemeAsset::ThemeAsset(const core::UUID& uuid)
    : asset::Asset(uuid) {
}

bool ThemeAsset::LoadFromFile(const std::string& filepath) {
    std::string contents;
    if (!ReadFileContents(filepath, contents)) {
        return false;
    }
    if (!ParseFromString(filepath, contents)) {
        return false;
    }
    SetLoaded(true);
    return true;
}

bool ThemeAsset::ReadFileContents(const std::string& filepath, std::string& outContents) {
    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode() && packFS.exists(filepath)) {
        outContents = packFS.readFileAsString(filepath);
        return !outContents.empty();
    }

    std::string physical = filepath;
    if (!platform::FileSystem::Exists(physical)) {
        physical = Asset::ResolveAssetPath(filepath);
    }
    platform::FileResult<std::string> result = platform::FileSystem::ReadFile(physical);
    if (!result.success) {
        return false;
    }
    outContents = std::move(result.data);
    return true;
}

bool ThemeAsset::ParseFromString(const std::string& filepath, const std::string& contents) {
    SetPath(filepath);

    try {
        nlohmann::json json = nlohmann::json::parse(contents);

        m_Name.clear();
        m_Extends.clear();
        m_PalettePath.clear();
        m_Palette.clear();
        m_Variables.clear();
        m_FontRoles.clear();
        m_Types.clear();
        m_DefaultFontPath.clear();
        m_DefaultFontSize = 0.0f;
        m_DefaultBaseScale = 1.0f;

        if (json.contains("name") && json["name"].is_string()) {
            m_Name = json["name"].get<std::string>();
        }
        if (json.contains("extends") && json["extends"].is_string()) {
            m_Extends = json["extends"].get<std::string>();
        }

        if (json.contains("default_font") && json["default_font"].is_string()) {
            m_DefaultFontPath = json["default_font"].get<std::string>();
        }
        if (json.contains("default_font_size") && json["default_font_size"].is_number()) {
            m_DefaultFontSize = json["default_font_size"].get<float>();
        }
        if (json.contains("default_base_scale") && json["default_base_scale"].is_number()) {
            m_DefaultBaseScale = json["default_base_scale"].get<float>();
        }

        // Palette: a string is an external .palette reference; an object/array is
        // an inline palette.
        if (json.contains("palette")) {
            const nlohmann::json& pal = json["palette"];
            if (pal.is_string()) {
                m_PalettePath = pal.get<std::string>();
            } else if (pal.is_object() || pal.is_array()) {
                ParsePaletteJson(pal, m_Palette);
            }
        }

        if (json.contains("variables") && json["variables"].is_object()) {
            for (nlohmann::json::iterator it = json["variables"].begin();
                 it != json["variables"].end(); ++it) {
                if (it.value().is_number()) {
                    m_Variables[it.key()] = it.value().get<float>();
                }
            }
        }

        if (json.contains("font_roles") && json["font_roles"].is_object()) {
            for (nlohmann::json::iterator it = json["font_roles"].begin();
                 it != json["font_roles"].end(); ++it) {
                const nlohmann::json& roleJson = it.value();
                if (!roleJson.is_object()) {
                    continue;
                }
                FontRole role;
                if (roleJson.contains("font") && roleJson["font"].is_string()) {
                    role.fontPath = roleJson["font"].get<std::string>();
                }
                if (roleJson.contains("size") && roleJson["size"].is_number()) {
                    role.size = roleJson["size"].get<float>();
                }
                m_FontRoles[it.key()] = role;
            }
        }

        if (json.contains("types") && json["types"].is_object()) {
            for (nlohmann::json::iterator it = json["types"].begin();
                 it != json["types"].end(); ++it) {
                const nlohmann::json& typeJson = it.value();
                if (!typeJson.is_object()) {
                    continue;
                }
                ThemeType type;
                type.name = it.key();
                if (typeJson.contains("extends") && typeJson["extends"].is_string()) {
                    type.extends = typeJson["extends"].get<std::string>();
                }
                if (typeJson.contains("colors") && typeJson["colors"].is_object()) {
                    for (nlohmann::json::const_iterator c = typeJson["colors"].begin();
                         c != typeJson["colors"].end(); ++c) {
                        type.colors[c.key()] = ThemeColor::FromJson(c.value());
                    }
                }
                if (typeJson.contains("constants") && typeJson["constants"].is_object()) {
                    for (nlohmann::json::const_iterator c = typeJson["constants"].begin();
                         c != typeJson["constants"].end(); ++c) {
                        type.constants[c.key()] = ThemeScalar::FromJson(c.value());
                    }
                }
                if (typeJson.contains("vec2s") && typeJson["vec2s"].is_object()) {
                    for (nlohmann::json::const_iterator c = typeJson["vec2s"].begin();
                         c != typeJson["vec2s"].end(); ++c) {
                        type.vec2s[c.key()] = ThemeVec2::FromJson(c.value());
                    }
                }
                if (typeJson.contains("bools") && typeJson["bools"].is_object()) {
                    for (nlohmann::json::const_iterator c = typeJson["bools"].begin();
                         c != typeJson["bools"].end(); ++c) {
                        type.bools[c.key()] = ThemeBool::FromJson(c.value());
                    }
                }
                if (typeJson.contains("fonts") && typeJson["fonts"].is_object()) {
                    for (nlohmann::json::const_iterator c = typeJson["fonts"].begin();
                         c != typeJson["fonts"].end(); ++c) {
                        type.fonts[c.key()] = ThemeFont::FromJson(c.value());
                    }
                }
                if (typeJson.contains("images") && typeJson["images"].is_object()) {
                    for (nlohmann::json::const_iterator c = typeJson["images"].begin();
                         c != typeJson["images"].end(); ++c) {
                        type.images[c.key()] = ThemeImage::FromJson(c.value());
                    }
                }
                if (typeJson.contains("styleboxes") && typeJson["styleboxes"].is_object()) {
                    for (nlohmann::json::const_iterator c = typeJson["styleboxes"].begin();
                         c != typeJson["styleboxes"].end(); ++c) {
                        type.styleboxes[c.key()] = ThemeStyleBox::FromJson(c.value());
                    }
                }
                m_Types[type.name] = type;
            }
        }

        RebuildPaletteIndex();
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(LogCategory::UI, "ThemeAsset: failed to parse '{}': {}", filepath, e.what());
        return false;
    }
}

void ThemeAsset::RebuildPaletteIndex() {
    m_PaletteIndex.clear();
    for (size_t i = 0; i < m_Palette.size(); ++i) {
        m_PaletteIndex[m_Palette[i].key] = i;
    }
}

void ThemeAsset::SetPaletteSlots(const std::vector<PaletteSlot>& slots) {
    m_Palette = slots;
    RebuildPaletteIndex();
}

bool ThemeAsset::GetPaletteColor(const std::string& key, math::Color& out) const {
    std::unordered_map<std::string, size_t>::const_iterator it = m_PaletteIndex.find(key);
    if (it == m_PaletteIndex.end()) {
        return false;
    }
    out = m_Palette[it->second].color;
    return true;
}

void ThemeAsset::SetPaletteColor(const std::string& key, const math::Color& color) {
    std::unordered_map<std::string, size_t>::const_iterator it = m_PaletteIndex.find(key);
    if (it != m_PaletteIndex.end()) {
        m_Palette[it->second].color = color;
        return;
    }
    PaletteSlot slot;
    slot.key = key;
    slot.name = key;
    slot.color = color;
    m_PaletteIndex[key] = m_Palette.size();
    m_Palette.push_back(slot);
}

bool ThemeAsset::GetVariable(const std::string& key, float& out) const {
    std::unordered_map<std::string, float>::const_iterator it = m_Variables.find(key);
    if (it == m_Variables.end()) {
        return false;
    }
    out = it->second;
    return true;
}

void ThemeAsset::SetVariable(const std::string& key, float value) {
    m_Variables[key] = value;
}

bool ThemeAsset::GetFontRole(const std::string& role, FontRole& out) const {
    std::unordered_map<std::string, FontRole>::const_iterator it = m_FontRoles.find(role);
    if (it == m_FontRoles.end()) {
        return false;
    }
    out = it->second;
    return true;
}

const ThemeType* ThemeAsset::GetType(const std::string& name) const {
    std::unordered_map<std::string, ThemeType>::const_iterator it = m_Types.find(name);
    if (it == m_Types.end()) {
        return nullptr;
    }
    return &it->second;
}

nlohmann::json ThemeAsset::ToJson() const {
    nlohmann::json json = nlohmann::json::object();
    json["lupine_theme"] = 1;
    json["name"] = m_Name;
    json["extends"] = m_Extends;

    if (!m_DefaultFontPath.empty()) {
        json["default_font"] = m_DefaultFontPath;
    }
    if (m_DefaultFontSize > 0.0f) {
        json["default_font_size"] = m_DefaultFontSize;
    }
    if (m_DefaultBaseScale != 1.0f) {
        json["default_base_scale"] = m_DefaultBaseScale;
    }

    if (!m_PalettePath.empty()) {
        json["palette"] = m_PalettePath;
    } else if (!m_Palette.empty()) {
        json["palette"] = SerializePaletteSlots(m_Name, m_Palette);
    }

    if (!m_Variables.empty()) {
        nlohmann::json vars = nlohmann::json::object();
        for (const std::pair<const std::string, float>& kv : m_Variables) {
            vars[kv.first] = kv.second;
        }
        json["variables"] = vars;
    }

    if (!m_FontRoles.empty()) {
        nlohmann::json roles = nlohmann::json::object();
        for (const std::pair<const std::string, FontRole>& kv : m_FontRoles) {
            nlohmann::json role = nlohmann::json::object();
            role["font"] = kv.second.fontPath;
            role["size"] = kv.second.size;
            roles[kv.first] = role;
        }
        json["font_roles"] = roles;
    }

    if (!m_Types.empty()) {
        nlohmann::json types = nlohmann::json::object();
        for (const std::pair<const std::string, ThemeType>& kv : m_Types) {
            const ThemeType& type = kv.second;
            nlohmann::json typeJson = nlohmann::json::object();
            if (!type.extends.empty()) {
                typeJson["extends"] = type.extends;
            }
            if (!type.colors.empty()) {
                nlohmann::json colors = nlohmann::json::object();
                for (const std::pair<const std::string, ThemeColor>& c : type.colors) {
                    colors[c.first] = c.second.ToJson();
                }
                typeJson["colors"] = colors;
            }
            if (!type.constants.empty()) {
                nlohmann::json constants = nlohmann::json::object();
                for (const std::pair<const std::string, ThemeScalar>& c : type.constants) {
                    constants[c.first] = c.second.ToJson();
                }
                typeJson["constants"] = constants;
            }
            if (!type.vec2s.empty()) {
                nlohmann::json vec2s = nlohmann::json::object();
                for (const std::pair<const std::string, ThemeVec2>& c : type.vec2s) {
                    vec2s[c.first] = c.second.ToJson();
                }
                typeJson["vec2s"] = vec2s;
            }
            if (!type.bools.empty()) {
                nlohmann::json bools = nlohmann::json::object();
                for (const std::pair<const std::string, ThemeBool>& c : type.bools) {
                    bools[c.first] = c.second.ToJson();
                }
                typeJson["bools"] = bools;
            }
            if (!type.fonts.empty()) {
                nlohmann::json fonts = nlohmann::json::object();
                for (const std::pair<const std::string, ThemeFont>& c : type.fonts) {
                    fonts[c.first] = c.second.ToJson();
                }
                typeJson["fonts"] = fonts;
            }
            if (!type.images.empty()) {
                nlohmann::json images = nlohmann::json::object();
                for (const std::pair<const std::string, ThemeImage>& c : type.images) {
                    images[c.first] = c.second.ToJson();
                }
                typeJson["images"] = images;
            }
            if (!type.styleboxes.empty()) {
                nlohmann::json styleboxes = nlohmann::json::object();
                for (const std::pair<const std::string, ThemeStyleBox>& c : type.styleboxes) {
                    styleboxes[c.first] = c.second.ToJson();
                }
                typeJson["styleboxes"] = styleboxes;
            }
            types[kv.first] = typeJson;
        }
        json["types"] = types;
    }

    return json;
}

} // namespace ui
} // namespace lupine
