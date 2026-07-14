#pragma once

#include "lupine/ui/Theme.hpp"
#include "lupine/math/Color.hpp"
#include <string>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace lupine {
namespace ui {

/**
 * Singleton that owns the active UI theme and resolves themed values for UI
 * components.
 *
 * On project load it activates the project's default theme (res://default.uitheme
 * by convention). Resolution follows: type variation -> base type ('extends'
 * chain) -> base theme ('extends' chain) -> the caller-supplied fallback (which
 * is the component's own hardcoded default, guaranteeing identical behaviour when
 * no theme is loaded). Token bindings (palette/variable/font-role) are resolved
 * against the root theme first, so a child theme's palette overrides its base.
 *
 * A monotonically increasing Version() bumps whenever the active theme or any of
 * its tokens change; components cache the version they last resolved against and
 * re-pull style when it differs.
 */
class ThemeManager {
public:
    static ThemeManager& GetInstance();

    // ===== Lifecycle =====

    // Activate the project's default theme. Prefers defaultThemeResPath (the
    // project's configured theme), falling back to the res://default.uitheme
    // convention when it is empty.
    void LoadProject(const std::string& projectDir, const std::string& defaultThemeResPath = "");

    // Drop the active theme and the loaded-theme cache.
    void Clear();

    bool IsLoaded() const { return m_Loaded; }
    const std::string& GetProjectDir() const { return m_ProjectDir; }

    // ===== Active theme =====

    // Load (if needed) and activate the theme at resPath. Returns false if it
    // cannot be loaded.
    bool SetActiveTheme(const std::string& resPath);
    void SetActiveTheme(const asset::AssetRef<ThemeAsset>& theme);
    ThemeAsset* GetActiveTheme() const { return m_ActiveTheme.Get(); }

    // Load (and cache) a theme by path, resolving its base chain and external
    // palette. Returns an empty ref on failure. Safe to call for per-node themes.
    asset::AssetRef<ThemeAsset> GetOrLoadTheme(const std::string& resPath);

    // ===== Change signal =====
    uint64_t Version() const { return m_Version; }

    // ===== Resolution against the active theme =====

    math::Color GetColor(const std::string& type, const std::string& variation,
                         const std::string& entry, const math::Color& fallback) const;
    float GetConstant(const std::string& type, const std::string& variation,
                      const std::string& entry, float fallback) const;
    math::Vec2 GetVec2(const std::string& type, const std::string& variation,
                       const std::string& entry, const math::Vec2& fallback) const;
    bool GetBool(const std::string& type, const std::string& variation,
                 const std::string& entry, bool fallback) const;
    bool GetFont(const std::string& type, const std::string& variation,
                 const std::string& entry, std::string& outFontPath, float& outSize) const;
    bool GetImage(const std::string& type, const std::string& variation,
                  const std::string& entry, std::string& outPath) const;
    std::shared_ptr<components::StyleBox> GetStyleBox(
                 const std::string& type, const std::string& variation,
                 const std::string& entry) const;

    // ===== Resolution against an explicit theme (per-node theme overrides) =====

    math::Color ResolveColor(const ThemeAsset* theme, const std::string& type,
                 const std::string& variation, const std::string& entry,
                 const math::Color& fallback) const;
    float ResolveConstant(const ThemeAsset* theme, const std::string& type,
                 const std::string& variation, const std::string& entry,
                 float fallback) const;
    math::Vec2 ResolveVec2(const ThemeAsset* theme, const std::string& type,
                 const std::string& variation, const std::string& entry,
                 const math::Vec2& fallback) const;
    bool ResolveBool(const ThemeAsset* theme, const std::string& type,
                 const std::string& variation, const std::string& entry,
                 bool fallback) const;
    bool ResolveFont(const ThemeAsset* theme, const std::string& type,
                 const std::string& variation, const std::string& entry,
                 std::string& outFontPath, float& outSize) const;
    // Resolve a themed image path (type variation -> base type -> base theme). Returns
    // false when no entry is defined, so the caller keeps its own value.
    bool ResolveImage(const ThemeAsset* theme, const std::string& type,
                 const std::string& variation, const std::string& entry,
                 std::string& outPath) const;
    // As ResolveImage, but also reports the entry's optional stretch / nine-slice
    // configuration (out.hasStretch). Returns false when no entry is defined.
    bool ResolveImageEx(const ThemeAsset* theme, const std::string& type,
                 const std::string& variation, const std::string& entry,
                 ThemeImage& out) const;
    std::shared_ptr<components::StyleBox> ResolveStyleBox(const ThemeAsset* theme,
                 const std::string& type, const std::string& variation,
                 const std::string& entry) const;

    // ===== Theme-level defaults (Godot Theme parity) =====
    // Resolve the theme-wide default font / base scale, walking the base ('extends')
    // chain so a child theme inherits its parent's defaults. The active-theme
    // convenience overloads use GetActiveTheme().

    bool GetDefaultFont(const ThemeAsset* theme, std::string& outFontPath, float& outSize) const;
    bool GetDefaultFont(std::string& outFontPath, float& outSize) const;
    float GetDefaultBaseScale(const ThemeAsset* theme) const;
    float GetDefaultBaseScale() const;

    // ===== Entry presence queries (so callers can keep their own value when the
    // theme does not define an entry — needed for multi-valued properties whose
    // fallback cannot be expressed as a single Resolve* fallback) =====
    bool HasColor(const ThemeAsset* theme, const std::string& type,
                  const std::string& variation, const std::string& entry) const;
    bool HasConstant(const ThemeAsset* theme, const std::string& type,
                     const std::string& variation, const std::string& entry) const;

    // ===== Runtime token edits (mutate the active theme; bump version) =====

    bool SetPaletteColor(const std::string& key, const math::Color& color);
    bool SetVariable(const std::string& key, float value);

    // ===== Token lookups with base-theme fallback (root theme checked first) ====

    bool GetPaletteColorChained(const ThemeAsset* root, const std::string& key, math::Color& out) const;
    bool GetVariableChained(const ThemeAsset* root, const std::string& key, float& out) const;
    bool GetFontRoleChained(const ThemeAsset* root, const std::string& role, FontRole& out) const;

private:
    ThemeManager() = default;
    ~ThemeManager() = default;
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

    void BumpVersion() { ++m_Version; }

    // Lazily create an empty in-memory active theme so runtime palette/variable
    // overrides apply even when no theme asset has been loaded.
    void EnsureActiveTheme();

    // Find a per-type entry, walking the type's 'extends' chain within one theme,
    // then descending into the theme's base ('extends') chain. Returns nullptr if
    // nothing matches.
    const ThemeColor*    FindColor(const ThemeAsset* theme, const std::string& type, const std::string& entry) const;
    const ThemeScalar*   FindScalar(const ThemeAsset* theme, const std::string& type, const std::string& entry) const;
    const ThemeVec2*     FindVec2(const ThemeAsset* theme, const std::string& type, const std::string& entry) const;
    const ThemeBool*     FindBool(const ThemeAsset* theme, const std::string& type, const std::string& entry) const;
    const ThemeFont*     FindFont(const ThemeAsset* theme, const std::string& type, const std::string& entry) const;
    const ThemeImage*    FindImage(const ThemeAsset* theme, const std::string& type, const std::string& entry) const;
    const ThemeStyleBox* FindStyleBox(const ThemeAsset* theme, const std::string& type, const std::string& entry) const;

    // Resolve a stylebox entry to a concrete StyleBox of its declared subtype
    // (handles derive form; palette colour bindings apply only to a flat base).
    std::shared_ptr<components::StyleBox> ResolveStyleBoxEntry(
                 const ThemeAsset* root, const std::string& type, const std::string& variation,
                 const std::string& entry, int depth) const;

    // ===== Resolution cache =====
    // Color/constant/vec2/bool resolution is on the per-frame hot path (every
    // themed control re-pulls many entries each SyncFromProperties, and per-state
    // entries are usually ABSENT — the most expensive case, a full type+theme
    // 'extends' walk that finds nothing). These results are memoized per
    // (theme, type, variation, entry) and dropped wholesale whenever Version()
    // changes. The cached outcome is fallback-independent: 'resolved == false'
    // means the theme does not define the entry, and the caller then applies its
    // own fallback (so the cache also collapses the absent-entry walk to one probe).
    struct ResolveKey {
        const ThemeAsset* theme;
        std::string type;
        std::string variation;
        std::string entry;
        bool operator==(const ResolveKey& other) const {
            return theme == other.theme && type == other.type
                && variation == other.variation && entry == other.entry;
        }
    };
    struct ResolveKeyHash {
        size_t operator()(const ResolveKey& key) const;
    };
    template <typename T>
    struct Cached {
        bool resolved = false;
        T value{};
    };

    // Drop all caches if the active-theme/token version has advanced.
    void EnsureCacheFresh() const;

    // Fallback-independent compute (the Resolve* bodies, minus the fallback).
    Cached<math::Color> ComputeColor(const ThemeAsset* theme, const std::string& type,
                 const std::string& variation, const std::string& entry) const;
    Cached<float> ComputeConstant(const ThemeAsset* theme, const std::string& type,
                 const std::string& variation, const std::string& entry) const;
    Cached<math::Vec2> ComputeVec2(const ThemeAsset* theme, const std::string& type,
                 const std::string& variation, const std::string& entry) const;
    Cached<bool> ComputeBool(const ThemeAsset* theme, const std::string& type,
                 const std::string& variation, const std::string& entry) const;

    asset::AssetRef<ThemeAsset> m_ActiveTheme;
    std::unordered_map<std::string, asset::AssetRef<ThemeAsset>> m_LoadedThemes; // res path -> theme
    std::string m_ProjectDir;
    bool m_Loaded = false;
    uint64_t m_Version = 1;

    mutable uint64_t m_CacheVersion = 0;
    mutable std::unordered_map<ResolveKey, Cached<math::Color>, ResolveKeyHash> m_ColorCache;
    mutable std::unordered_map<ResolveKey, Cached<float>, ResolveKeyHash> m_ConstantCache;
    mutable std::unordered_map<ResolveKey, Cached<math::Vec2>, ResolveKeyHash> m_Vec2Cache;
    mutable std::unordered_map<ResolveKey, Cached<bool>, ResolveKeyHash> m_BoolCache;
};

} // namespace ui
} // namespace lupine
