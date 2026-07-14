#include "lupine/components/RichTextLabel.hpp"
#include "lupine/localization/LocalizationManager.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

namespace {

uint32_t NextAtlasSize(float fontSize) {
    uint32_t minAtlasSize = static_cast<uint32_t>(fontSize * 10.0f);
    uint32_t atlasSize = 512;
    while (atlasSize < minAtlasSize && atlasSize < 4096) atlasSize *= 2;
    return atlasSize;
}

std::vector<uint32_t> DecodeUTF8(const std::string& str) {
    std::vector<uint32_t> out;
    out.reserve(str.size());
    const unsigned char* b = reinterpret_cast<const unsigned char*>(str.data());
    const size_t n = str.size();
    size_t i = 0;
    while (i < n) {
        unsigned char lead = b[i];
        uint32_t cp = 0; size_t extra = 0;
        if (lead < 0x80) { cp = lead; extra = 0; }
        else if ((lead & 0xE0) == 0xC0) { cp = lead & 0x1F; extra = 1; }
        else if ((lead & 0xF0) == 0xE0) { cp = lead & 0x0F; extra = 2; }
        else if ((lead & 0xF8) == 0xF0) { cp = lead & 0x07; extra = 3; }
        else { ++i; continue; }
        ++i; bool ok = true;
        for (size_t k = 0; k < extra; ++k) {
            if (i >= n || (b[i] & 0xC0) != 0x80) { ok = false; break; }
            cp = (cp << 6) | (b[i] & 0x3F); ++i;
        }
        if (ok) out.push_back(cp);
    }
    return out;
}

Color ParseColor(const std::string& s, const Color& fallback) {
    if (s.empty()) return fallback;
    if (s[0] == '#') {
        auto hex = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return 0;
        };
        std::string h = s.substr(1);
        if (h.size() == 6 || h.size() == 8) {
            float r = (hex(h[0]) * 16 + hex(h[1])) / 255.0f;
            float g = (hex(h[2]) * 16 + hex(h[3])) / 255.0f;
            float b = (hex(h[4]) * 16 + hex(h[5])) / 255.0f;
            float a = (h.size() == 8) ? (hex(h[6]) * 16 + hex(h[7])) / 255.0f : 1.0f;
            return Color(r, g, b, a);
        }
        return fallback;
    }
    if (s == "red") return Color(1, 0, 0, 1);
    if (s == "green") return Color(0, 1, 0, 1);
    if (s == "blue") return Color(0.2f, 0.4f, 1, 1);
    if (s == "white") return Color(1, 1, 1, 1);
    if (s == "black") return Color(0, 0, 0, 1);
    if (s == "yellow") return Color(1, 1, 0, 1);
    if (s == "cyan") return Color(0, 1, 1, 1);
    if (s == "magenta") return Color(1, 0, 1, 1);
    if (s == "orange") return Color(1, 0.6f, 0, 1);
    if (s == "gray" || s == "grey") return Color(0.5f, 0.5f, 0.5f, 1);
    return fallback;
}

struct Run {
    std::string text;
    Color color;
    float size;
    bool bold;
    bool italic;
};

std::vector<Run> ParseBBCode(const std::string& s, const Color& baseColor, float baseSize) {
    struct Style { Color color; float size; bool bold; bool italic; };
    std::vector<Style> stack;
    stack.push_back({baseColor, baseSize, false, false});

    std::vector<Run> runs;
    std::string buf;
    auto flush = [&]() {
        if (!buf.empty()) {
            const Style& st = stack.back();
            runs.push_back({buf, st.color, st.size, st.bold, st.italic});
            buf.clear();
        }
    };

    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '[') {
            size_t close = s.find(']', i);
            if (close == std::string::npos) { buf.push_back(s[i]); ++i; continue; }
            std::string tag = s.substr(i + 1, close - i - 1);
            const Style top = stack.back();
            if (tag == "b") { flush(); stack.push_back({top.color, top.size, true, top.italic}); }
            else if (tag == "i") { flush(); stack.push_back({top.color, top.size, top.bold, true}); }
            else if (tag.rfind("color=", 0) == 0) { flush(); stack.push_back({ParseColor(tag.substr(6), top.color), top.size, top.bold, top.italic}); }
            else if (tag.rfind("size=", 0) == 0) {
                flush();
                float sz = static_cast<float>(std::atof(tag.substr(5).c_str()));
                if (sz <= 0.0f) sz = top.size;
                stack.push_back({top.color, sz, top.bold, top.italic});
            } else if (tag == "/b" || tag == "/i" || tag == "/color" || tag == "/size" || tag == "/font") {
                flush();
                if (stack.size() > 1) stack.pop_back();
            } else {
                // Unknown tag: ignore (consume).
            }
            i = close + 1;
        } else {
            buf.push_back(s[i]);
            ++i;
        }
    }
    flush();
    return runs;
}

struct PlacedGlyph {
    const Glyph* g;
    float x;       // pen x including bearing
    float scale;
    Color color;
    bool bold;
    bool italic;
    bool isSpace;  // an inter-word gap, widened by Fill justification
};

struct LineData {
    std::vector<PlacedGlyph> glyphs;
    float width = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
};

void EmitQuad(MeshData& md, uint32_t& base,
              const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3,
              const Vec2& u0, const Vec2& u1, const Vec2& u2, const Vec2& u3,
              const Color& color) {
    Vec4 c(color.r, color.g, color.b, color.a);
    auto push = [&](const Vec2& p, const Vec2& uv) {
        Vertex v;
        v.position = Vec3(p.x, p.y, 0.0f);
        v.normal = Vec3(0.0f, 0.0f, 1.0f);
        v.texCoord = uv;
        v.color = c;
        md.vertices.push_back(v);
    };
    push(p0, u0); push(p1, u1); push(p2, u2); push(p3, u3);
    md.indices.push_back(base + 0); md.indices.push_back(base + 1); md.indices.push_back(base + 2);
    md.indices.push_back(base + 0); md.indices.push_back(base + 2); md.indices.push_back(base + 3);
    base += 4;
}

// Build a styled text mesh (world coords via origin offset) and report its size.
MeshData BuildRichMesh(const FontAtlas& atlas, const std::vector<Run>& runs, float baseSize,
                       float boxWidth, bool wordWrap, TextHAlign hAlign, float lineSpacing,
                       int tabSize, bool clipText, float boxHeight,
                       const Vec2& origin, Vec2& outSize) {
    std::vector<LineData> lines;
    LineData cur;
    float penX = 0.0f;
    int wordStart = 0;
    float wordStartX = 0.0f;
    uint32_t prevCp = 0;   // left-hand codepoint of the current kerning pair

    auto finishLine = [&]() {
        cur.width = penX;
        lines.push_back(cur);
        cur = LineData{};
        penX = 0.0f;
        wordStart = 0;
        wordStartX = 0.0f;
    };

    for (const Run& run : runs) {
        const float scale = (atlas.fontSize > 0.0f) ? (run.size / atlas.fontSize) : 1.0f;
        std::vector<uint32_t> cps = DecodeUTF8(run.text);
        for (uint32_t cp : cps) {
            if (cp == '\r') continue;
            if (cp == '\n') { finishLine(); continue; }

            const Glyph* g = atlas.getGlyph(cp);
            const bool isSpace = (cp == ' ' || cp == '\t');

            // Same tab-stop and kerning rules as TextLayout, so a RichTextLabel and a Label
            // showing the same string lay it out identically. The tab advance here was a
            // hardcoded 1em that no caller could change and that TextLayout did not agree
            // with, and kerning was ignored on both sides.
            const int stops = (tabSize > 0) ? tabSize : 4;
            float adv = g ? g->advance * scale
                          : (cp == '\t' ? atlas.fontSize * 0.25f * static_cast<float>(stops) * scale
                                        : (cp == ' ' ? atlas.fontSize * 0.25f * scale : 0.0f));
            if (g && prevCp != 0) {
                adv += atlas.getKerning(prevCp, cp) * scale;
            }
            prevCp = cp;

            if (wordWrap && boxWidth > 0.0f && (penX + adv) > boxWidth && !cur.glyphs.empty() && !isSpace) {
                if (wordStart < static_cast<int>(cur.glyphs.size())) {
                    LineData nl;
                    for (int k = wordStart; k < static_cast<int>(cur.glyphs.size()); ++k) {
                        PlacedGlyph pg = cur.glyphs[k];
                        pg.x -= wordStartX;
                        nl.glyphs.push_back(pg);
                        nl.ascent = std::max(nl.ascent, atlas.getAscent() * pg.scale);
                        nl.descent = std::max(nl.descent, atlas.getDescent() * pg.scale);
                    }
                    cur.glyphs.resize(wordStart);
                    cur.width = wordStartX;
                    lines.push_back(cur);
                    cur = nl;
                    penX -= wordStartX;
                    wordStart = 0;
                    wordStartX = 0.0f;
                } else {
                    finishLine();
                }
            }

            if (g) {
                PlacedGlyph pg{g, penX + g->bearing.x * scale, scale, run.color, run.bold,
                               run.italic, isSpace};
                cur.glyphs.push_back(pg);
                // Whole-font metrics, not per-glyph bearings: Glyph::bearing.y is the
                // negative stb `yoff`, so max()-ing it yields no usable ascent, and a
                // per-string ascent would shift the baseline with the text's content.
                cur.ascent = std::max(cur.ascent, atlas.getAscent() * scale);
                cur.descent = std::max(cur.descent, atlas.getDescent() * scale);
            }
            penX += adv;
            if (isSpace) { wordStart = static_cast<int>(cur.glyphs.size()); wordStartX = penX; }
        }
    }
    finishLine();
    if (lines.size() > 1 && lines.back().glyphs.empty()) {
        lines.pop_back();
    }

    float maxLineWidth = 0.0f;
    for (const LineData& ln : lines) maxLineWidth = std::max(maxLineWidth, ln.width);

    const float baseScale = (atlas.fontSize > 0.0f) ? (baseSize / atlas.fontSize) : 1.0f;
    const float baseLineH = atlas.getLineHeight() * baseScale;
    const float emptyAscent = atlas.getAscent() * baseScale;
    const float emptyDescent = atlas.getDescent() * baseScale;
    const float boxW = (boxWidth > 0.0f) ? boxWidth : maxLineWidth;

    MeshData md;
    uint32_t base = 0;
    float penY = 0.0f;

    for (size_t li = 0; li < lines.size(); ++li) {
        const LineData& ln = lines[li];
        const bool isLastLine = (li + 1 == lines.size());

        const float asc = ln.glyphs.empty() ? emptyAscent : ln.ascent;
        const float desc = ln.glyphs.empty() ? emptyDescent : ln.descent;
        float lineH = std::max(baseLineH, asc + desc) * (lineSpacing > 0.0f ? lineSpacing : 1.0f);
        // `origin` is the box TOP-left and the canvas is Y-up, so the block descends
        // into negative local Y: each baseline sits one ascent below its line's top
        // edge, and penY accumulates downward. Matches the TextLayout contract.
        const float baselineY = -(penY + asc);

        float alignOff = 0.0f;
        float extraPerGap = 0.0f;
        if (hAlign == TextHAlign::Center) {
            alignOff = (boxW - ln.width) * 0.5f;
        } else if (hAlign == TextHAlign::Right) {
            alignOff = (boxW - ln.width);
        } else if (hAlign == TextHAlign::Fill && !isLastLine && boxWidth > 0.0f) {
            // Justify by widening the inter-word gaps, matching TextLayout::Fill. The
            // last line is left alone, and a line with no gaps has nothing to stretch.
            int gaps = 0;
            for (const PlacedGlyph& pg : ln.glyphs) {
                if (pg.isSpace) ++gaps;
            }
            if (gaps > 0) {
                extraPerGap = (boxW - ln.width) / static_cast<float>(gaps);
            }
        }

        float justifyOff = 0.0f;
        for (const PlacedGlyph& pg : ln.glyphs) {
            const Glyph* g = pg.g;
            const float gy = baselineY - g->bearing.y * pg.scale;
            const float gw = g->size.x * pg.scale;
            const float gh = g->size.y * pg.scale;
            const float gx = origin.x + pg.x + alignOff + justifyOff;
            const float gyy = origin.y + gy;
            if (pg.isSpace) {
                justifyOff += extraPerGap;
            }

            Vec2 p0(gx, gyy - gh), p1(gx + gw, gyy - gh), p2(gx + gw, gyy), p3(gx, gyy);
            if (pg.italic) {
                const float slant = 0.22f * gh;
                p0.x += slant; p1.x += slant;
            }
            const Vec2 u0(g->uvMin.x, g->uvMax.y), u1(g->uvMax.x, g->uvMax.y),
                       u2(g->uvMax.x, g->uvMin.y), u3(g->uvMin.x, g->uvMin.y);

            // clipText: discard any glyph not wholly inside the box, matching TextLayout.
            // Long rich text used to spill out of the control unclipped.
            if (clipText) {
                const float localX = gx - origin.x;
                const float localY = gyy - origin.y;
                if (boxWidth > 0.0f && (localX < 0.0f || localX + gw > boxWidth)) {
                    continue;
                }
                if (boxHeight > 0.0f && (localY > 0.0f || localY - gh < -boxHeight)) {
                    continue;
                }
            }

            EmitQuad(md, base, p0, p1, p2, p3, u0, u1, u2, u3, pg.color);
            if (pg.bold) {
                const float bOff = std::max(0.6f, gh * 0.04f);
                EmitQuad(md, base, Vec2(p0.x + bOff, p0.y), Vec2(p1.x + bOff, p1.y),
                         Vec2(p2.x + bOff, p2.y), Vec2(p3.x + bOff, p3.y), u0, u1, u2, u3, pg.color);
            }
        }
        penY += lineH;
    }

    outSize = Vec2(maxLineWidth, penY);
    if (!md.vertices.empty()) md.calculateBounds();
    return md;
}

} // anonymous namespace

RichTextLabel::RichTextLabel()
    : UIControl("RichTextLabel")
{
}

RichTextLabel::RichTextLabel(const std::string& name)
    : UIControl(name)
{
}

RichTextLabel::~RichTextLabel() {
}

void RichTextLabel::DefineProperties() {
    DefineUIControlProperties(240.0f, 0.0f, "uiSpace", "Layout");

    DefineProperty(PROPERTY_DEFAULT_GROUP(text, String, std::string("[b]Rich[/b] [color=#66ccff]Text[/color]"), "Text"));

    // localizationKey overrides "text" and resolves through the LocalizationManager
    // (the resolved string may itself contain BBCode markup).
    DefineProperty(PROPERTY_DEFAULT_GROUP(localizationKey, String, std::string(""), "Localization"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(localizationTable, String, std::string(""), "Localization"));
    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 256.0f, 1.0f, "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(color, Color, Color::White(), "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(wordWrap, Bool, true, "Text"));
    DefineProperty(PROPERTY_ENUM_GROUP(horizontalAlign, 0, "Text", Left, Center, Right, Fill));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(lineSpacing, 1.0f, 0.1f, 5.0f, 0.05f, "Text"));
    // Parity with Label/Button: clip overflowing rich text instead of letting it spill, and
    // make the tab stop configurable (it was a hardcoded 1em that TextLayout disagreed with).
    DefineProperty(PROPERTY_DEFAULT_GROUP(clipText, Bool, false, "Text"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(tabSize, 4, 1, 16, 1, "Text"));
}

void RichTextLabel::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    UIControl::OnPropertyChanged(propertyName, newValue);
    m_Dirty = true;
}

const std::vector<UIControl::ThemeBinding>& RichTextLabel::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = {
        { "color",    "font_color", ThemeBinding::Kind::Color },
        { "fontPath", "font",       ThemeBinding::Kind::Font },
        { "fontSize", "font_size",  ThemeBinding::Kind::Constant }
    };
    return kBindings;
}

std::string RichTextLabel::GetText() const {
    std::string locKey = GetPropertyValue<std::string>("localizationKey");
    if (!locKey.empty()) {
        return localization::LocalizationManager::GetInstance().Translate(
            locKey, GetPropertyValue<std::string>("localizationTable"));
    }
    return GetPropertyValue<std::string>("text");
}
void RichTextLabel::SetText(const std::string& text) { SetPropertyValue<std::string>("text", text); m_Dirty = true; }
std::string RichTextLabel::GetFontPath() const { return ResolveThemedFontPath("fontPath", "font"); }
void RichTextLabel::SetFontPath(const std::string& path) { SetThemedProperty<std::string>("fontPath", path); m_Dirty = true; }
float RichTextLabel::GetFontSize() const { return ResolveThemedFontSize("fontSize", "font_size", "font"); }
void RichTextLabel::SetFontSize(float size) { SetThemedProperty<float>("fontSize", size); m_Dirty = true; }
Color RichTextLabel::GetColor() const {
    return ResolveThemedColor("color", "font_color");
}
void RichTextLabel::SetColor(const Color& color) { SetThemedProperty<Color>("color", color); m_Dirty = true; }
bool RichTextLabel::GetWordWrap() const { return GetPropertyValue<bool>("wordWrap"); }
void RichTextLabel::SetWordWrap(bool wrap) { SetPropertyValue<bool>("wordWrap", wrap); m_Dirty = true; }
TextHAlign RichTextLabel::GetHorizontalAlign() const { return static_cast<TextHAlign>(GetPropertyValue<int>("horizontalAlign")); }
void RichTextLabel::SetHorizontalAlign(TextHAlign align) { SetPropertyValue<int>("horizontalAlign", static_cast<int>(align)); m_Dirty = true; }
float RichTextLabel::GetLineSpacing() const { return GetPropertyValue<float>("lineSpacing"); }
void RichTextLabel::SetLineSpacing(float spacing) { SetPropertyValue<float>("lineSpacing", spacing); m_Dirty = true; }

Vec2 RichTextLabel::GetCenter() const {
    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    return node2D ? node2D->GetGlobalPosition() : Vec2(0.0f, 0.0f);
}

void RichTextLabel::EnsureFont(RenderContext& ctx) {
    std::string fontPath = GetFontPath();
    if (fontPath.empty()) {
        auto& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) fontPath = assetDb.GetDefaultFontPath();
    }
    const float fontSize = GetFontSize();
    if (fontPath != m_CurrentFontPath || fontSize != m_CurrentFontSize) {
        m_CurrentFontPath = fontPath;
        m_CurrentFontSize = fontSize;
        m_FontHandle = FontHandle();
        m_FontAsset.Reset();
        m_Dirty = true;
        if (!fontPath.empty()) {
            m_FontAsset = asset::AssetRef<asset::FontAsset>(new asset::FontAsset());
            const uint32_t atlasSize = NextAtlasSize(fontSize);
            if (!m_FontAsset->LoadFromFile(fontPath, fontSize, atlasSize, atlasSize)) m_FontAsset.Reset();
        }
    }
    if (m_FontAsset.IsValid() && m_FontAsset->IsLoaded() && !m_FontHandle.isValid()) {
        FontDesc fontDesc;
        fontDesc.fontPath = m_FontAsset->GetPhysicalPath();
        fontDesc.fontSize = fontSize;
        fontDesc.atlasWidth = m_FontAsset->GetAtlasWidth();
        fontDesc.atlasHeight = m_FontAsset->GetAtlasHeight();
        if (IGfxDevice* device = ctx.getDevice()) {
            m_FontHandle = device->createFontAtlas(fontDesc);
            m_Dirty = true;
        }
    }
}

bool RichTextLabel::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
    std::string fontPath = GetFontPath();
    if (fontPath.empty()) return false;
    auto& assetDb = asset::AssetDatabase::GetInstance();
    std::string resolvedFontPath;
    if (assetDb.IsInitialized()) resolvedFontPath = assetDb.ResolveAsset(fontPath);
    bool matches = (fontPath == changedPath) ||
                   (!resolvedFontPath.empty() && !resolvedChangedPath.empty() && resolvedFontPath == resolvedChangedPath);
    if (matches) {
        m_FontHandle = FontHandle();
        m_FontAsset.Reset();
        m_CurrentFontPath.clear();
        m_Dirty = true;
        return true;
    }
    return false;
}

void RichTextLabel::buildDrawCommands(RenderContext& ctx) {
    if (!m_Owner) {
        return;
    }
    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return;
    }

    // A resolution change re-bakes the font atlas at a new oversampling density,
    // which repacks glyph UVs; regenerate the cached text mesh so it matches.
    if (ConsumeFontOversampleChanged()) {
        m_TextMeshKey.clear();
    }

    EnsureFont(ctx);
    const FontAtlas* atlas = m_FontHandle.isValid() ? ctx.getDevice()->getFontAtlas(m_FontHandle) : nullptr;
    if (!atlas) {
        return;
    }

    const float fontSize = GetFontSize();
    const bool wrap = GetWordWrap();
    const TextHAlign hAlign = GetHorizontalAlign();
    const float lineSpacing = GetLineSpacing();

    // Align against the RESOLVED rect, not the authored width property: an anchored
    // or container-sized RichTextLabel has a real width but a zero width property,
    // and passing zero here would silently disable wrapping and alignment alike.
    const math::Rect rect = GetResolvedRect();
    const float boxWidth = (rect.size.x > 0.0f) ? rect.size.x : 0.0f;

    // The layout origin BuildRichMesh expects is the box's TOP-left, which the box rect
    // -- the same rect getWorldBounds() reports -- exposes directly.
    const Vec2 boxTopLeft = GetTextBoxRect().GetTopLeft();

    std::string text = GetText();
    Color baseColor = GetColor();

    char keyBuf[160];
    std::snprintf(keyBuf, sizeof(keyBuf), "%.1f|%.1f|%.1f|%.1f|%d|%d|%.2f|%.3f|%.3f|%.3f|%.3f",
                  boxTopLeft.x, boxTopLeft.y, fontSize, boxWidth, wrap ? 1 : 0,
                  static_cast<int>(hAlign), lineSpacing, baseColor.r, baseColor.g, baseColor.b, baseColor.a);
    std::string key = text + "\x1f" + keyBuf;

    if (key != m_TextMeshKey || m_Dirty) {
        if (m_TextMesh.isValid()) {
            ctx.getDevice()->destroyMesh(m_TextMesh);
            m_TextMesh = MeshHandle();
        }
        std::vector<Run> runs = ParseBBCode(text, baseColor, fontSize);
        Vec2 size;
        MeshData md = BuildRichMesh(*atlas, runs, fontSize,
                                    (wrap ? boxWidth : 0.0f), wrap, hAlign, lineSpacing,
                                    GetPropertyValue<int>("tabSize"),
                                    GetPropertyValue<bool>("clipText"),
                                    rect.size.y,
                                    boxTopLeft, size);

        // GetContentMinSize() reports this cache, and it is only ever filled in HERE -- in
        // the draw pass. So a RichTextLabel measures as (0,0) until the frame it first
        // renders, and any parent container has by then already arranged it as empty. Once
        // the real size is known, tell the container to measure again.
        if (size != m_CachedSize) {
            m_CachedSize = size;
            NotifyContentSizeChanged();
        }

        if (!md.vertices.empty()) {
            m_TextMesh = ctx.getDevice()->createMesh(md);
        }
        m_TextMeshKey = key;
        m_Dirty = false;
    }

    if (m_TextMesh.isValid()) {
        MaterialPropertyBlock overrides;
        overrides.setTexture("u_FontAtlas", atlas->texture);
        ctx.drawMesh(m_TextMesh, ctx.getDefaultTextMaterial(), Mat4::Identity(), overrides);
    }
}

math::Rect RichTextLabel::GetTextBoxRect() const {
    const math::Rect resolved = GetResolvedRect();

    if (resolved.size.x > 0.0f || resolved.size.y > 0.0f) {
        // LayoutMode::Position resolves the rect from the authored width/height, which are
        // 0 on any axis left to the content. Apply the same min/max clamp GetBoundsSize()
        // does so the box actually contains the text. The text descends from the top edge,
        // so the extra height comes off the MIN corner and the top edge stays put.
        const Vec2 size = ClampToMinMax(resolved.size);
        return math::Rect(Vec2(resolved.position.x,
                               resolved.position.y - (size.y - resolved.size.y)),
                          size);
    }

    // Never laid out and no authored size: the box collapses onto the text, whose
    // top-left sits at the node position.
    const Vec2 size = ClampToMinMax(Vec2(0.0f, 0.0f));
    const Vec2 topLeft = GetCenter();
    return math::Rect(Vec2(topLeft.x, topLeft.y - size.y), size);
}

AABB RichTextLabel::getWorldBounds() const {
    // The bounds ARE the layout box. Deriving them from the rect's min corner (as this
    // used to) anchored the box to the BOTTOM edge and grew it upward, while the text
    // descends from the TOP edge -- so the box landed one text-height above the glyphs.
    const math::Rect box = GetTextBoxRect();
    const Vec2 topLeft = box.GetTopLeft();

    // A RichTextLabel measures itself in Render(), so before the first draw the box can
    // still be empty. Keep it pickable rather than degenerate.
    const float width = (box.size.x > 0.0f) ? box.size.x : 10.0f;
    const float height = (box.size.y > 0.0f) ? box.size.y : 10.0f;

    return AABB(Vec3(topLeft.x, topLeft.y - height, -0.1f),
                Vec3(topLeft.x + width, topLeft.y, 0.1f));
}

RenderLayer RichTextLabel::getRenderLayer() const {
    return RenderLayer::Transparent;
}

Vec2 RichTextLabel::GetContentMinSize() const {
    return m_CachedSize;
}

} // namespace components
} // namespace lupine
