#pragma once

#include "lupine/asset/Asset.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/components/StyleBox.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace lupine {
namespace ui {

/**
 * UI Theme system data model.
 *
 * A ThemeAsset (.uitheme) styles in-game UI control types from one place. It is
 * layered on a design-token model: a colour palette, scalar variables, and font
 * roles. Each per-type entry is either a literal value or a binding to a token,
 * so changing one token restyles everything bound to it.
 *
 * Resolution (token bindings, type inheritance, theme inheritance) lives on
 * ThemeManager; ThemeAsset only owns the parsed data and own-theme token lookups.
 */

/**
 * Colour modifier applied to a token-bound value (e.g. a hover colour derived
 * from a base palette colour). 'amount' is normalised in [0,1] for all ops.
 */
enum class ColorOp {
    None,
    Lighten,
    Darken,
    Alpha,
    Saturate,
    Desaturate
};

const char* ColorOpToString(ColorOp op);
ColorOp ColorOpFromString(const std::string& s);

/**
 * Apply a colour modifier and return the (clamped) result.
 */
math::Color ApplyColorOp(const math::Color& color, ColorOp op, float amount);

/**
 * Parse a colour from JSON. Accepts an [r,g,b,a] / [r,g,b] array or an
 * {"r","g","b","a"} object. Returns false if the value is not a colour.
 */
bool ParseColorJson(const nlohmann::json& json, math::Color& out);
nlohmann::json SerializeColorJson(const math::Color& color);

/**
 * A theme colour entry: either a literal colour, or a binding to a palette slot
 * (optionally transformed by a ColorOp).
 */
struct ThemeColor {
    bool bound = false;            // true -> resolve from palette slot 'paletteKey'
    std::string paletteKey;
    ColorOp op = ColorOp::None;
    float opAmount = 0.0f;
    math::Color literal = math::Color::White();

    static ThemeColor Literal(const math::Color& c);
    static ThemeColor Bind(const std::string& key, ColorOp op = ColorOp::None, float amount = 0.0f);

    nlohmann::json ToJson() const;
    static ThemeColor FromJson(const nlohmann::json& json);
};

/**
 * A theme scalar entry: either a literal float, or a binding to a variable token.
 */
struct ThemeScalar {
    bool bound = false;            // true -> resolve from variable 'variableKey'
    std::string variableKey;
    float literal = 0.0f;

    static ThemeScalar LiteralValue(float v);
    static ThemeScalar Bind(const std::string& key);

    nlohmann::json ToJson() const;
    static ThemeScalar FromJson(const nlohmann::json& json);
};

/**
 * A theme Vec2 entry: a literal (x,y) pair. Used for per-state tween offsets
 * (scale / position) which have no meaningful token binding. Serializes as
 * { "value": [x, y] } (an {"x","y"} object is also accepted on load).
 */
struct ThemeVec2 {
    math::Vec2 literal{0.0f, 0.0f};

    static ThemeVec2 LiteralValue(const math::Vec2& v);

    nlohmann::json ToJson() const;
    static ThemeVec2 FromJson(const nlohmann::json& json);
};

/**
 * A theme bool entry: a literal flag. Used for per-state tween "enabled".
 * Serializes as { "value": true|false }.
 */
struct ThemeBool {
    bool literal = false;

    static ThemeBool LiteralValue(bool v);

    nlohmann::json ToJson() const;
    static ThemeBool FromJson(const nlohmann::json& json);
};

/**
 * A theme font entry: either a font role reference, or a direct font path + size.
 */
struct ThemeFont {
    std::string role;              // non-empty -> resolve from a font role
    std::string fontPath;          // used when 'role' is empty
    float size = 16.0f;            // used when 'role' is empty

    nlohmann::json ToJson() const;
    static ThemeFont FromJson(const nlohmann::json& json);
};

/**
 * A theme image entry: a texture/image resource path (res:// or physical) used to
 * drive a UI control's background or per-state image from the theme. An empty path
 * resolves to "no themed image", so the control keeps its own property value.
 *
 * The entry may optionally carry a stretch/nine-slice configuration so a theme can
 * say HOW its image is fitted (stretch / keep-centered / nine-slice with per-side
 * margins, per-axis stretch/tile, and an optional centre fill) — not just which
 * image. When 'hasStretch' is false the consuming control keeps its own nine-slice
 * properties. Margins are in SOURCE texture pixels.
 *
 * Serializes as { "path": "res://...", "stretch_mode": "nine_slice",
 *   "nine_slice": { "left":N, "top":N, "right":N, "bottom":N,
 *                   "axis_h":"stretch"|"tile", "axis_v":..., "draw_center":bool } }.
 * A bare string (path only) is also accepted on load.
 */
struct ThemeImage {
    std::string path;

    bool hasStretch = false;        // true -> the entry defines stretchMode (+ nine-slice)
    int stretchMode = 0;            // UIImageStretchMode: 0 Stretch, 1 KeepCentered, 2 NineSlice
    float marginLeft = 0.0f;
    float marginTop = 0.0f;
    float marginRight = 0.0f;
    float marginBottom = 0.0f;
    int axisH = 0;                  // UINineSliceAxisMode: 0 Stretch, 1 Tile
    int axisV = 0;
    bool drawCenter = true;

    static ThemeImage LiteralPath(const std::string& p);

    nlohmann::json ToJson() const;
    static ThemeImage FromJson(const nlohmann::json& json);
};

/**
 * A theme stylebox entry. Two mutually exclusive forms:
 *  - Concrete: a StyleBox 'base' of any concrete subtype (StyleBoxFlat /
 *    StyleBoxTexture / StyleBoxLine / StyleBoxEmpty). When the base is a
 *    StyleBoxFlat its colour fields may be overridden by palette bindings in
 *    'colorBindings' (recognised fields: "background", "border", "shadow").
 *  - Derived: 'deriveFrom' names another stylebox entry of the same type; the
 *    resolved box is a clone of that entry with 'deriveOp' applied to its
 *    background and border colours (only meaningful for a flat base).
 */
struct ThemeStyleBox {
    std::shared_ptr<components::StyleBox> base;                 // concrete form (any subtype)
    std::unordered_map<std::string, ThemeColor> colorBindings; // field -> binding

    std::string deriveFrom;                                     // derived form
    ColorOp deriveOp = ColorOp::None;
    float deriveAmount = 0.0f;

    bool IsDerived() const { return !deriveFrom.empty(); }

    nlohmann::json ToJson() const;
    static ThemeStyleBox FromJson(const nlohmann::json& json);
};

/**
 * One control type's themed entries (e.g. "Button"). 'extends' names another
 * type in the same theme whose entries are inherited and may be overridden here.
 */
struct ThemeType {
    std::string name;
    std::string extends;
    std::unordered_map<std::string, ThemeColor>    colors;
    std::unordered_map<std::string, ThemeScalar>   constants;
    std::unordered_map<std::string, ThemeVec2>     vec2s;
    std::unordered_map<std::string, ThemeBool>     bools;
    std::unordered_map<std::string, ThemeFont>     fonts;
    std::unordered_map<std::string, ThemeImage>    images;
    std::unordered_map<std::string, ThemeStyleBox> styleboxes;
};

/**
 * A palette colour slot. 'key' is the stable identifier referenced by bindings;
 * 'name' is a display label only.
 */
struct PaletteSlot {
    std::string key;
    std::string name;
    math::Color color = math::Color::White();
};

/**
 * A font role token: a font path plus a default size.
 */
struct FontRole {
    std::string fontPath;
    float size = 16.0f;
};

/**
 * Parse a palette JSON document into slots. Accepts either a {"slots":[...]}
 * object (the .palette schema) or a bare [...] array. Returns false on a schema
 * error. Existing contents of outSlots are replaced.
 */
bool ParsePaletteJson(const nlohmann::json& json, std::vector<PaletteSlot>& outSlots);

/**
 * Read and parse a .palette file (res:// or physical path). Returns false if the
 * file cannot be read or parsed.
 */
bool ParsePaletteFile(const std::string& filepath, std::vector<PaletteSlot>& outSlots);

/**
 * Serialize palette slots into the .palette document schema.
 */
nlohmann::json SerializePaletteSlots(const std::string& name, const std::vector<PaletteSlot>& slots);

/**
 * The .uitheme asset: parsed palette/variables/font-roles/type-entries plus an
 * optional resolved base theme (set by ThemeManager during load).
 */
class ThemeAsset : public asset::Asset {
public:
    ThemeAsset();
    explicit ThemeAsset(const core::UUID& uuid);
    ~ThemeAsset() override = default;

    asset::AssetType GetType() const override { return asset::AssetType::Theme; }

    // ----- Loading (two-phase so async finalisation can attach base/palette) -----

    // Convenience: read + parse a .uitheme file. Does NOT resolve the base theme
    // chain or an external palette reference; ThemeManager performs those links.
    bool LoadFromFile(const std::string& filepath);

    // Thread-safe file read (handles pack mode + res:// resolution).
    static bool ReadFileContents(const std::string& filepath, std::string& outContents);

    // Thread-safe parse of an in-memory .uitheme document.
    bool ParseFromString(const std::string& filepath, const std::string& contents);

    // ----- Tokens (this theme only; chained lookups live on ThemeManager) -----

    const std::vector<PaletteSlot>& GetPaletteSlots() const { return m_Palette; }
    void SetPaletteSlots(const std::vector<PaletteSlot>& slots);
    bool GetPaletteColor(const std::string& key, math::Color& out) const;
    void SetPaletteColor(const std::string& key, const math::Color& color);

    bool GetVariable(const std::string& key, float& out) const;
    void SetVariable(const std::string& key, float value);
    const std::unordered_map<std::string, float>& GetVariables() const { return m_Variables; }

    bool GetFontRole(const std::string& role, FontRole& out) const;
    const std::unordered_map<std::string, FontRole>& GetFontRoles() const { return m_FontRoles; }

    // ----- Types -----

    const ThemeType* GetType(const std::string& name) const;
    const std::unordered_map<std::string, ThemeType>& GetTypes() const { return m_Types; }

    // ----- Inheritance -----

    const std::string& GetExtends() const { return m_Extends; }      // res:// base theme, or ""
    const std::string& GetPalettePath() const { return m_PalettePath; } // external .palette, or ""

    void SetBase(const asset::AssetRef<ThemeAsset>& base) { m_Base = base; }
    ThemeAsset* GetBase() const { return m_Base.Get(); }

    const std::string& GetThemeName() const { return m_Name; }

    // ----- Theme-level defaults (Godot Theme parity) -----
    // A default font path / size applied to any text control whose type+entry
    // resolves no font, and a base scale that multiplies resolved font sizes and
    // constants. All optional; empty path / <=0 values mean "unset".

    const std::string& GetDefaultFontPath() const { return m_DefaultFontPath; }
    void SetDefaultFontPath(const std::string& path) { m_DefaultFontPath = path; }

    float GetDefaultFontSize() const { return m_DefaultFontSize; }
    void SetDefaultFontSize(float size) { m_DefaultFontSize = size; }

    float GetDefaultBaseScale() const { return m_DefaultBaseScale; }
    void SetDefaultBaseScale(float scale) { m_DefaultBaseScale = scale; }

    bool HasDefaultFont() const { return !m_DefaultFontPath.empty(); }

    // ----- Serialization (for the editor / save) -----
    nlohmann::json ToJson() const;

private:
    std::string m_Name;
    std::string m_Extends;       // res:// path to base theme, or empty
    std::string m_PalettePath;   // res:// path to external palette, or empty (inline/none)

    std::string m_DefaultFontPath;     // theme-wide fallback font, or empty
    float m_DefaultFontSize = 0.0f;    // <=0 -> unset
    float m_DefaultBaseScale = 1.0f;   // multiplies resolved sizes; 1 -> identity

    std::vector<PaletteSlot> m_Palette;
    std::unordered_map<std::string, size_t> m_PaletteIndex;  // key -> slot index
    std::unordered_map<std::string, float> m_Variables;
    std::unordered_map<std::string, FontRole> m_FontRoles;
    std::unordered_map<std::string, ThemeType> m_Types;

    asset::AssetRef<ThemeAsset> m_Base;

    void RebuildPaletteIndex();
};

} // namespace ui
} // namespace lupine
