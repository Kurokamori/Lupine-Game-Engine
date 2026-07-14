#include "lupine/ui/ThemeManager.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/logger/Logger.hpp"
#include <functional>

namespace lupine {
namespace ui {

ThemeManager& ThemeManager::GetInstance() {
    static ThemeManager instance;
    return instance;
}

// ===================================================================== Lifecycle

void ThemeManager::LoadProject(const std::string& projectDir, const std::string& defaultThemeResPath) {
    Clear();
    m_ProjectDir = projectDir;

    // Prefer the project's configured default theme. If it is unset or fails to load,
    // fall back to the res://default.uitheme convention at the project root. The
    // fallback goes through GetOrLoadTheme rather than probing a physical path,
    // because in pack mode there is no file on disk for FileSystem::Exists to see
    // (and projectDir is empty there, so the join would produce "/default.uitheme").
    asset::AssetRef<ThemeAsset> theme;
    if (!defaultThemeResPath.empty()) {
        theme = GetOrLoadTheme(defaultThemeResPath);
        if (!theme) {
            LOG_WARN(LogCategory::UI,
                     "ThemeManager: configured default theme '{}' failed to load; "
                     "falling back to res://default.uitheme", defaultThemeResPath);
        }
    }
    if (!theme) {
        theme = GetOrLoadTheme("res://default.uitheme");
    }
    if (theme) {
        m_ActiveTheme = theme;
    }

    m_Loaded = true;
    BumpVersion();

    if (m_ActiveTheme) {
        LOG_INFO(LogCategory::UI,
                 "ThemeManager: active theme '{}' ({} type(s), {} palette slot(s))",
                 m_ActiveTheme->GetThemeName(), m_ActiveTheme->GetTypes().size(),
                 m_ActiveTheme->GetPaletteSlots().size());
    } else {
        LOG_INFO(LogCategory::UI,
                 "ThemeManager: no default theme (res://default.uitheme); UI uses built-in defaults");
    }
}

void ThemeManager::Clear() {
    m_ActiveTheme.Reset();
    m_LoadedThemes.clear();
    m_ProjectDir.clear();
    m_Loaded = false;
    BumpVersion();
}

// =================================================================== Active theme

bool ThemeManager::SetActiveTheme(const std::string& resPath) {
    asset::AssetRef<ThemeAsset> theme = GetOrLoadTheme(resPath);
    if (!theme) {
        return false;
    }
    m_ActiveTheme = theme;
    BumpVersion();
    return true;
}

void ThemeManager::SetActiveTheme(const asset::AssetRef<ThemeAsset>& theme) {
    m_ActiveTheme = theme;
    BumpVersion();
}

asset::AssetRef<ThemeAsset> ThemeManager::GetOrLoadTheme(const std::string& resPath) {
    std::unordered_map<std::string, asset::AssetRef<ThemeAsset>>::iterator it =
        m_LoadedThemes.find(resPath);
    if (it != m_LoadedThemes.end()) {
        return it->second;
    }

    asset::AssetRef<ThemeAsset> theme(new ThemeAsset());
    if (!theme->LoadFromFile(resPath)) {
        LOG_WARN(LogCategory::UI, "ThemeManager: failed to load theme '{}'", resPath);
        return asset::AssetRef<ThemeAsset>();
    }

    // Cache before resolving the base chain so an 'extends' cycle returns this
    // (partially linked) theme instead of recursing forever.
    m_LoadedThemes[resPath] = theme;

    if (!theme->GetPalettePath().empty()) {
        std::vector<PaletteSlot> slots;
        if (ParsePaletteFile(theme->GetPalettePath(), slots)) {
            theme->SetPaletteSlots(slots);
        } else {
            LOG_WARN(LogCategory::UI,
                     "ThemeManager: theme '{}' references palette '{}' which could not be loaded",
                     resPath, theme->GetPalettePath());
        }
    }

    if (!theme->GetExtends().empty()) {
        asset::AssetRef<ThemeAsset> base = GetOrLoadTheme(theme->GetExtends());
        if (base.Get() == theme.Get()) {
            LOG_WARN(LogCategory::UI,
                     "ThemeManager: theme '{}' extends itself; ignoring base", resPath);
        } else {
            theme->SetBase(base);
        }
    }

    return theme;
}

// ============================================================ Entry lookups

const ThemeColor* ThemeManager::FindColor(const ThemeAsset* theme, const std::string& type,
                                          const std::string& entry) const {
    const ThemeAsset* t = theme;
    int baseGuard = 0;
    while (t && baseGuard++ < 64) {
        std::string typeName = type;
        int guard = 0;
        while (!typeName.empty() && guard++ < 32) {
            const ThemeType* tt = t->GetType(typeName);
            if (!tt) {
                break;
            }
            std::unordered_map<std::string, ThemeColor>::const_iterator it = tt->colors.find(entry);
            if (it != tt->colors.end()) {
                return &it->second;
            }
            typeName = tt->extends;
        }
        t = t->GetBase();
    }
    return nullptr;
}

const ThemeScalar* ThemeManager::FindScalar(const ThemeAsset* theme, const std::string& type,
                                            const std::string& entry) const {
    const ThemeAsset* t = theme;
    int baseGuard = 0;
    while (t && baseGuard++ < 64) {
        std::string typeName = type;
        int guard = 0;
        while (!typeName.empty() && guard++ < 32) {
            const ThemeType* tt = t->GetType(typeName);
            if (!tt) {
                break;
            }
            std::unordered_map<std::string, ThemeScalar>::const_iterator it = tt->constants.find(entry);
            if (it != tt->constants.end()) {
                return &it->second;
            }
            typeName = tt->extends;
        }
        t = t->GetBase();
    }
    return nullptr;
}

const ThemeVec2* ThemeManager::FindVec2(const ThemeAsset* theme, const std::string& type,
                                        const std::string& entry) const {
    const ThemeAsset* t = theme;
    int baseGuard = 0;
    while (t && baseGuard++ < 64) {
        std::string typeName = type;
        int guard = 0;
        while (!typeName.empty() && guard++ < 32) {
            const ThemeType* tt = t->GetType(typeName);
            if (!tt) {
                break;
            }
            std::unordered_map<std::string, ThemeVec2>::const_iterator it = tt->vec2s.find(entry);
            if (it != tt->vec2s.end()) {
                return &it->second;
            }
            typeName = tt->extends;
        }
        t = t->GetBase();
    }
    return nullptr;
}

const ThemeBool* ThemeManager::FindBool(const ThemeAsset* theme, const std::string& type,
                                        const std::string& entry) const {
    const ThemeAsset* t = theme;
    int baseGuard = 0;
    while (t && baseGuard++ < 64) {
        std::string typeName = type;
        int guard = 0;
        while (!typeName.empty() && guard++ < 32) {
            const ThemeType* tt = t->GetType(typeName);
            if (!tt) {
                break;
            }
            std::unordered_map<std::string, ThemeBool>::const_iterator it = tt->bools.find(entry);
            if (it != tt->bools.end()) {
                return &it->second;
            }
            typeName = tt->extends;
        }
        t = t->GetBase();
    }
    return nullptr;
}

const ThemeFont* ThemeManager::FindFont(const ThemeAsset* theme, const std::string& type,
                                        const std::string& entry) const {
    const ThemeAsset* t = theme;
    int baseGuard = 0;
    while (t && baseGuard++ < 64) {
        std::string typeName = type;
        int guard = 0;
        while (!typeName.empty() && guard++ < 32) {
            const ThemeType* tt = t->GetType(typeName);
            if (!tt) {
                break;
            }
            std::unordered_map<std::string, ThemeFont>::const_iterator it = tt->fonts.find(entry);
            if (it != tt->fonts.end()) {
                return &it->second;
            }
            typeName = tt->extends;
        }
        t = t->GetBase();
    }
    return nullptr;
}

const ThemeImage* ThemeManager::FindImage(const ThemeAsset* theme, const std::string& type,
                                          const std::string& entry) const {
    const ThemeAsset* t = theme;
    int baseGuard = 0;
    while (t && baseGuard++ < 64) {
        std::string typeName = type;
        int guard = 0;
        while (!typeName.empty() && guard++ < 32) {
            const ThemeType* tt = t->GetType(typeName);
            if (!tt) {
                break;
            }
            std::unordered_map<std::string, ThemeImage>::const_iterator it = tt->images.find(entry);
            if (it != tt->images.end()) {
                return &it->second;
            }
            typeName = tt->extends;
        }
        t = t->GetBase();
    }
    return nullptr;
}

const ThemeStyleBox* ThemeManager::FindStyleBox(const ThemeAsset* theme, const std::string& type,
                                                const std::string& entry) const {
    const ThemeAsset* t = theme;
    int baseGuard = 0;
    while (t && baseGuard++ < 64) {
        std::string typeName = type;
        int guard = 0;
        while (!typeName.empty() && guard++ < 32) {
            const ThemeType* tt = t->GetType(typeName);
            if (!tt) {
                break;
            }
            std::unordered_map<std::string, ThemeStyleBox>::const_iterator it = tt->styleboxes.find(entry);
            if (it != tt->styleboxes.end()) {
                return &it->second;
            }
            typeName = tt->extends;
        }
        t = t->GetBase();
    }
    return nullptr;
}

// =========================================================== Token chained lookups

bool ThemeManager::GetPaletteColorChained(const ThemeAsset* root, const std::string& key,
                                          math::Color& out) const {
    const ThemeAsset* t = root;
    int baseGuard = 0;
    while (t && baseGuard++ < 64) {
        if (t->GetPaletteColor(key, out)) {
            return true;
        }
        t = t->GetBase();
    }
    return false;
}

bool ThemeManager::GetVariableChained(const ThemeAsset* root, const std::string& key,
                                      float& out) const {
    const ThemeAsset* t = root;
    int baseGuard = 0;
    while (t && baseGuard++ < 64) {
        if (t->GetVariable(key, out)) {
            return true;
        }
        t = t->GetBase();
    }
    return false;
}

bool ThemeManager::GetFontRoleChained(const ThemeAsset* root, const std::string& role,
                                      FontRole& out) const {
    const ThemeAsset* t = root;
    int baseGuard = 0;
    while (t && baseGuard++ < 64) {
        if (t->GetFontRole(role, out)) {
            return true;
        }
        t = t->GetBase();
    }
    return false;
}

// ================================================= Resolution (explicit theme)

// ===================================================================== Cache

size_t ThemeManager::ResolveKeyHash::operator()(const ResolveKey& key) const {
    size_t h = std::hash<const void*>()(static_cast<const void*>(key.theme));
    auto mix = [&h](size_t v) {
        h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    };
    mix(std::hash<std::string>()(key.type));
    mix(std::hash<std::string>()(key.variation));
    mix(std::hash<std::string>()(key.entry));
    return h;
}

void ThemeManager::EnsureCacheFresh() const {
    if (m_CacheVersion != m_Version) {
        m_ColorCache.clear();
        m_ConstantCache.clear();
        m_Vec2Cache.clear();
        m_BoolCache.clear();
        m_CacheVersion = m_Version;
    }
}

ThemeManager::Cached<math::Color> ThemeManager::ComputeColor(const ThemeAsset* theme,
        const std::string& type, const std::string& variation, const std::string& entry) const {
    const ThemeColor* tc = nullptr;
    if (!variation.empty()) {
        tc = FindColor(theme, variation, entry);
    }
    if (!tc) {
        tc = FindColor(theme, type, entry);
    }
    if (!tc) {
        return {false, math::Color()};
    }
    if (!tc->bound) {
        return {true, tc->literal};
    }
    math::Color c;
    if (GetPaletteColorChained(theme, tc->paletteKey, c)) {
        return {true, ApplyColorOp(c, tc->op, tc->opAmount)};
    }
    return {false, math::Color()};
}

ThemeManager::Cached<float> ThemeManager::ComputeConstant(const ThemeAsset* theme,
        const std::string& type, const std::string& variation, const std::string& entry) const {
    const ThemeScalar* ts = nullptr;
    if (!variation.empty()) {
        ts = FindScalar(theme, variation, entry);
    }
    if (!ts) {
        ts = FindScalar(theme, type, entry);
    }
    if (!ts) {
        return {false, 0.0f};
    }
    if (!ts->bound) {
        return {true, ts->literal};
    }
    float v = 0.0f;
    if (GetVariableChained(theme, ts->variableKey, v)) {
        return {true, v};
    }
    return {false, 0.0f};
}

ThemeManager::Cached<math::Vec2> ThemeManager::ComputeVec2(const ThemeAsset* theme,
        const std::string& type, const std::string& variation, const std::string& entry) const {
    const ThemeVec2* tv = nullptr;
    if (!variation.empty()) {
        tv = FindVec2(theme, variation, entry);
    }
    if (!tv) {
        tv = FindVec2(theme, type, entry);
    }
    if (!tv) {
        return {false, math::Vec2(0.0f, 0.0f)};
    }
    return {true, tv->literal};
}

ThemeManager::Cached<bool> ThemeManager::ComputeBool(const ThemeAsset* theme,
        const std::string& type, const std::string& variation, const std::string& entry) const {
    const ThemeBool* tb = nullptr;
    if (!variation.empty()) {
        tb = FindBool(theme, variation, entry);
    }
    if (!tb) {
        tb = FindBool(theme, type, entry);
    }
    if (!tb) {
        return {false, false};
    }
    return {true, tb->literal};
}

math::Color ThemeManager::ResolveColor(const ThemeAsset* theme, const std::string& type,
                                       const std::string& variation, const std::string& entry,
                                       const math::Color& fallback) const {
    if (!theme) {
        return fallback;
    }
    EnsureCacheFresh();
    ResolveKey key{theme, type, variation, entry};
    std::unordered_map<ResolveKey, Cached<math::Color>, ResolveKeyHash>::const_iterator it =
        m_ColorCache.find(key);
    if (it == m_ColorCache.end()) {
        it = m_ColorCache.emplace(std::move(key),
                                  ComputeColor(theme, type, variation, entry)).first;
    }
    return it->second.resolved ? it->second.value : fallback;
}

float ThemeManager::ResolveConstant(const ThemeAsset* theme, const std::string& type,
                                    const std::string& variation, const std::string& entry,
                                    float fallback) const {
    if (!theme) {
        return fallback;
    }
    EnsureCacheFresh();
    ResolveKey key{theme, type, variation, entry};
    std::unordered_map<ResolveKey, Cached<float>, ResolveKeyHash>::const_iterator it =
        m_ConstantCache.find(key);
    if (it == m_ConstantCache.end()) {
        it = m_ConstantCache.emplace(std::move(key),
                                     ComputeConstant(theme, type, variation, entry)).first;
    }
    return it->second.resolved ? it->second.value : fallback;
}

math::Vec2 ThemeManager::ResolveVec2(const ThemeAsset* theme, const std::string& type,
                                     const std::string& variation, const std::string& entry,
                                     const math::Vec2& fallback) const {
    if (!theme) {
        return fallback;
    }
    EnsureCacheFresh();
    ResolveKey key{theme, type, variation, entry};
    std::unordered_map<ResolveKey, Cached<math::Vec2>, ResolveKeyHash>::const_iterator it =
        m_Vec2Cache.find(key);
    if (it == m_Vec2Cache.end()) {
        it = m_Vec2Cache.emplace(std::move(key),
                                 ComputeVec2(theme, type, variation, entry)).first;
    }
    return it->second.resolved ? it->second.value : fallback;
}

bool ThemeManager::ResolveBool(const ThemeAsset* theme, const std::string& type,
                               const std::string& variation, const std::string& entry,
                               bool fallback) const {
    if (!theme) {
        return fallback;
    }
    EnsureCacheFresh();
    ResolveKey key{theme, type, variation, entry};
    std::unordered_map<ResolveKey, Cached<bool>, ResolveKeyHash>::const_iterator it =
        m_BoolCache.find(key);
    if (it == m_BoolCache.end()) {
        it = m_BoolCache.emplace(std::move(key),
                                 ComputeBool(theme, type, variation, entry)).first;
    }
    return it->second.resolved ? it->second.value : fallback;
}

bool ThemeManager::ResolveFont(const ThemeAsset* theme, const std::string& type,
                               const std::string& variation, const std::string& entry,
                               std::string& outFontPath, float& outSize) const {
    if (!theme) {
        return false;
    }
    const ThemeFont* tf = nullptr;
    if (!variation.empty()) {
        tf = FindFont(theme, variation, entry);
    }
    if (!tf) {
        tf = FindFont(theme, type, entry);
    }
    if (!tf) {
        return false;
    }
    if (!tf->role.empty()) {
        FontRole role;
        if (GetFontRoleChained(theme, tf->role, role)) {
            outFontPath = role.fontPath;
            outSize = role.size;
            return true;
        }
        return false;
    }
    outFontPath = tf->fontPath;
    outSize = tf->size;
    return true;
}

bool ThemeManager::ResolveImage(const ThemeAsset* theme, const std::string& type,
                                const std::string& variation, const std::string& entry,
                                std::string& outPath) const {
    ThemeImage out;
    if (!ResolveImageEx(theme, type, variation, entry, out)) {
        return false;
    }
    outPath = out.path;
    return true;
}

bool ThemeManager::ResolveImageEx(const ThemeAsset* theme, const std::string& type,
                                  const std::string& variation, const std::string& entry,
                                  ThemeImage& out) const {
    if (!theme) {
        return false;
    }
    const ThemeImage* ti = nullptr;
    if (!variation.empty()) {
        ti = FindImage(theme, variation, entry);
    }
    if (!ti) {
        ti = FindImage(theme, type, entry);
    }
    if (!ti) {
        return false;
    }
    out = *ti;
    return true;
}

std::shared_ptr<components::StyleBox> ThemeManager::ResolveStyleBoxEntry(
        const ThemeAsset* root, const std::string& type, const std::string& variation,
        const std::string& entry, int depth) const {
    if (!root || depth > 16) {
        return nullptr;
    }
    const ThemeStyleBox* sb = nullptr;
    if (!variation.empty()) {
        sb = FindStyleBox(root, variation, entry);
    }
    if (!sb) {
        sb = FindStyleBox(root, type, entry);
    }
    if (!sb) {
        return nullptr;
    }

    if (sb->IsDerived()) {
        std::shared_ptr<components::StyleBox> baseBox =
            ResolveStyleBoxEntry(root, type, variation, sb->deriveFrom, depth + 1);
        if (!baseBox) {
            return nullptr;
        }
        std::shared_ptr<components::StyleBox> clone = baseBox->Clone();
        // The derive colour op only applies to a flat base (texture/line/empty
        // boxes have no background/border colours to transform).
        if (auto flat = std::dynamic_pointer_cast<components::StyleBoxFlat>(clone)) {
            flat->SetBackgroundColor(ApplyColorOp(flat->GetBackgroundColor(), sb->deriveOp, sb->deriveAmount));
            flat->SetBorderColorAll(ApplyColorOp(flat->GetBorderColorLeft(), sb->deriveOp, sb->deriveAmount));
        }
        return clone;
    }

    std::shared_ptr<components::StyleBox> box;
    if (sb->base) {
        box = sb->base->Clone();
    } else {
        box = std::make_shared<components::StyleBoxFlat>();
    }

    // Palette colour bindings only affect a flat base's colour fields.
    std::shared_ptr<components::StyleBoxFlat> flat =
        std::dynamic_pointer_cast<components::StyleBoxFlat>(box);
    if (flat) {
        for (const std::pair<const std::string, ThemeColor>& kv : sb->colorBindings) {
            math::Color c;
            bool ok = false;
            if (kv.second.bound) {
                if (GetPaletteColorChained(root, kv.second.paletteKey, c)) {
                    c = ApplyColorOp(c, kv.second.op, kv.second.opAmount);
                    ok = true;
                }
            } else {
                c = kv.second.literal;
                ok = true;
            }
            if (!ok) {
                continue;
            }
            // Recognise both the short field aliases (background/border/shadow) and
            // the concrete StyleBoxFlat property names the theme editor writes
            // (backgroundColor/borderColor{Left,Right,Top,Bottom}/shadowColor), so a
            // palette binding authored per-side actually applies.
            if (kv.first == "background" || kv.first == "backgroundColor") {
                flat->SetBackgroundColor(c);
            } else if (kv.first == "border" || kv.first == "borderColor") {
                flat->SetBorderColorAll(c);
            } else if (kv.first == "borderColorLeft") {
                flat->SetBorderColorLeft(c);
            } else if (kv.first == "borderColorRight") {
                flat->SetBorderColorRight(c);
            } else if (kv.first == "borderColorTop") {
                flat->SetBorderColorTop(c);
            } else if (kv.first == "borderColorBottom") {
                flat->SetBorderColorBottom(c);
            } else if (kv.first == "shadow" || kv.first == "shadowColor") {
                flat->SetShadowColor(c);
            }
        }
    }
    return box;
}

std::shared_ptr<components::StyleBox> ThemeManager::ResolveStyleBox(
        const ThemeAsset* theme, const std::string& type, const std::string& variation,
        const std::string& entry) const {
    return ResolveStyleBoxEntry(theme, type, variation, entry, 0);
}

// ============================================================== Theme defaults

bool ThemeManager::GetDefaultFont(const ThemeAsset* theme, std::string& outFontPath,
                                  float& outSize) const {
    int depth = 0;
    for (const ThemeAsset* t = theme; t && depth < 32; t = t->GetBase(), ++depth) {
        if (t->HasDefaultFont()) {
            outFontPath = t->GetDefaultFontPath();
            float sz = t->GetDefaultFontSize();
            outSize = sz > 0.0f ? sz : 16.0f;
            return true;
        }
    }
    return false;
}

bool ThemeManager::GetDefaultFont(std::string& outFontPath, float& outSize) const {
    return GetDefaultFont(m_ActiveTheme.Get(), outFontPath, outSize);
}

float ThemeManager::GetDefaultBaseScale(const ThemeAsset* theme) const {
    int depth = 0;
    for (const ThemeAsset* t = theme; t && depth < 32; t = t->GetBase(), ++depth) {
        float s = t->GetDefaultBaseScale();
        if (s > 0.0f && s != 1.0f) {
            return s;
        }
    }
    return 1.0f;
}

float ThemeManager::GetDefaultBaseScale() const {
    return GetDefaultBaseScale(m_ActiveTheme.Get());
}

bool ThemeManager::HasColor(const ThemeAsset* theme, const std::string& type,
                            const std::string& variation, const std::string& entry) const {
    if (!theme) {
        return false;
    }
    if (!variation.empty() && FindColor(theme, variation, entry)) {
        return true;
    }
    return FindColor(theme, type, entry) != nullptr;
}

bool ThemeManager::HasConstant(const ThemeAsset* theme, const std::string& type,
                               const std::string& variation, const std::string& entry) const {
    if (!theme) {
        return false;
    }
    if (!variation.empty() && FindScalar(theme, variation, entry)) {
        return true;
    }
    return FindScalar(theme, type, entry) != nullptr;
}

// ================================================= Resolution (active theme)

math::Color ThemeManager::GetColor(const std::string& type, const std::string& variation,
                                   const std::string& entry, const math::Color& fallback) const {
    return ResolveColor(m_ActiveTheme.Get(), type, variation, entry, fallback);
}

float ThemeManager::GetConstant(const std::string& type, const std::string& variation,
                                const std::string& entry, float fallback) const {
    return ResolveConstant(m_ActiveTheme.Get(), type, variation, entry, fallback);
}

math::Vec2 ThemeManager::GetVec2(const std::string& type, const std::string& variation,
                                 const std::string& entry, const math::Vec2& fallback) const {
    return ResolveVec2(m_ActiveTheme.Get(), type, variation, entry, fallback);
}

bool ThemeManager::GetBool(const std::string& type, const std::string& variation,
                           const std::string& entry, bool fallback) const {
    return ResolveBool(m_ActiveTheme.Get(), type, variation, entry, fallback);
}

bool ThemeManager::GetFont(const std::string& type, const std::string& variation,
                           const std::string& entry, std::string& outFontPath, float& outSize) const {
    return ResolveFont(m_ActiveTheme.Get(), type, variation, entry, outFontPath, outSize);
}

bool ThemeManager::GetImage(const std::string& type, const std::string& variation,
                            const std::string& entry, std::string& outPath) const {
    return ResolveImage(m_ActiveTheme.Get(), type, variation, entry, outPath);
}

std::shared_ptr<components::StyleBox> ThemeManager::GetStyleBox(
        const std::string& type, const std::string& variation, const std::string& entry) const {
    return ResolveStyleBox(m_ActiveTheme.Get(), type, variation, entry);
}

// ============================================================ Runtime token edits

void ThemeManager::EnsureActiveTheme() {
    if (!m_ActiveTheme) {
        m_ActiveTheme = asset::AssetRef<ThemeAsset>(new ThemeAsset());
    }
}

bool ThemeManager::SetPaletteColor(const std::string& key, const math::Color& color) {
    EnsureActiveTheme();
    m_ActiveTheme->SetPaletteColor(key, color);
    BumpVersion();
    return true;
}

bool ThemeManager::SetVariable(const std::string& key, float value) {
    EnsureActiveTheme();
    m_ActiveTheme->SetVariable(key, value);
    BumpVersion();
    return true;
}

} // namespace ui
} // namespace lupine
