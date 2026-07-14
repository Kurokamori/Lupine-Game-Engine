#include "lupine/components/Label.hpp"
#include "lupine/localization/LocalizationManager.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Project.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/math/OBB.hpp"
#include "lupine/math/Ray.hpp"
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

Label::Label()
    : UIControl("Label")
    , m_FontHandle()
    , m_FontNeedsUpload(false)
    , m_CurrentFontSize(0.0f)
    , m_TextMesh()
    , m_CachedFontSize(0.0f)
    , m_CachedPosition(0.0f, 0.0f)
    , m_CachedRotation(0.0f)
    , m_MeshNeedsRegeneration(true)
{
}

Label::Label(const std::string& name)
    : UIControl(name)
    , m_FontHandle()
    , m_FontNeedsUpload(false)
    , m_CurrentFontSize(0.0f)
    , m_TextMesh()
    , m_CachedFontSize(0.0f)
    , m_CachedPosition(0.0f, 0.0f)
    , m_CachedRotation(0.0f)
    , m_MeshNeedsRegeneration(true)
{
}

Label::~Label() {

}

void Label::DefineProperties() {

    // Shared UIControl layout properties (anchors/size-flags/uiSpace + width/height).
    DefineUIControlProperties(0.0f, 0.0f, "uiSpace", "Layout");

    DefineProperty(PROPERTY_DEFAULT_GROUP(text, String, std::string("Label"), "Text"));

    // When localizationKey is set it overrides "text": the displayed string is
    // resolved through the LocalizationManager for the active locale (live-updates
    // on locale change). localizationTable optionally restricts the lookup to one
    // table; leave empty to search all tables.
    DefineProperty(PROPERTY_DEFAULT_GROUP(localizationKey, String, std::string(""), "Localization"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(localizationTable, String, std::string(""), "Localization"));
    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 256.0f, 1.0f, "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(color, Color, Color::White(), "Text"));

    DefineProperty(PROPERTY_ENUM_GROUP(horizontalAlign, 0, "Text", Left, Center, Right, Fill));
    DefineProperty(PROPERTY_ENUM_GROUP(verticalAlign, 0, "Text", Top, Center, Bottom));
    DefineProperty(PROPERTY_DEFAULT_GROUP(wordWrap, Bool, false, "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(multiline, Bool, true, "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(lineSpacing, 1.0f, 0.1f, 5.0f, 0.05f, "Text"));
    // Line-breaking and overflow. `wordWrap` remains the simple on/off switch and still means
    // WordSmart; autowrapMode selects the other modes. Off keeps `wordWrap` in charge.
    DefineProperty(PROPERTY_ENUM_GROUP(autowrapMode, 0, "Text", Off, Arbitrary, Word, WordSmart));
    DefineProperty(PROPERTY_ENUM_GROUP(overrunBehavior, 0, "Text",
        None, TrimChar, TrimWord, EllipsisChar, EllipsisWord));
    DefineProperty(PROPERTY_DEFAULT_GROUP(clipText, Bool, false, "Text"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(tabSize, 4, 1, 16, 1, "Text"));
    // Drop shadow. StyleBoxFlat has a box shadow; text had none at all.
    DefineProperty(PROPERTY_DEFAULT_GROUP(shadowOffset, Vec2, Vec2(0.0f, 0.0f), "Shadow"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(shadowColor, Color, Color(0.0f, 0.0f, 0.0f, 0.0f), "Shadow"));
    // Auto-shrink: step the font down (never past minFontSize) until the text fits the box.
    // Button has scaleMode = FitToText, which grows the CONTROL to fit the text; this is the
    // opposite and complementary knob, and Label had neither.
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoShrink, Bool, false, "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(minFontSize, 8.0f, 1.0f, 256.0f, 1.0f, "Text"));

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(outlineWidth, 0.0f, 0.0f, 16.0f, 0.5f, "Outline"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(outlineColor, Color, Color::Black(), "Outline"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(centered, Bool, false, "Layout"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(offset, Vec2, Vec2(0.0f, 0.0f), "Layout"));
}

void Label::OnAwake() {

    std::string fontPath = GetFontPath();

    // If no font path set, try to use the default font
    if (fontPath.empty()) {
        auto& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) {
            fontPath = assetDb.GetDefaultFontPath();
        }
    }

    if (!fontPath.empty()) {
        LoadFont(fontPath);
    }
}

void Label::OnReady() {

    if (m_FontNeedsUpload) {
        UploadFontToGPU();
    }
}

void Label::OnRender() {

}

bool Label::OnGizmoScale(float scaleDelta, int, bool is3D) {
    if (!is3D) {

        float currentSize = GetFontSize();

        float newSize = std::max(1.0f, currentSize + scaleDelta * currentSize);

        newSize = std::min(256.0f, newSize);

        SetFontSize(newSize);

        m_MeshNeedsRegeneration = true;

        return true;
    }
    return false;
}

bool Label::LoadFont(const std::string& filepath) {
    if (filepath.empty()) {

        return false;
    }

    m_FontAsset = asset::AssetRef<asset::FontAsset>(new asset::FontAsset());

    float fontSize = GetFontSize();

    uint32_t minAtlasSize = static_cast<uint32_t>(fontSize * 10.0f);
    uint32_t atlasSize = 512;
    while (atlasSize < minAtlasSize && atlasSize < 4096) {
        atlasSize *= 2;
    }

    bool loaded = m_FontAsset->LoadFromFile(filepath, fontSize, atlasSize, atlasSize);

    if (!loaded) {

        m_FontAsset.Reset();
        return false;
    }

    m_FontNeedsUpload = true;
    m_MeshNeedsRegeneration = true;

    // The font is what the text is MEASURED with, and fonts load lazily -- the first time
    // this control is drawn, which is after any parent container has already measured and
    // arranged it against a zero-size (fontless) estimate. Tell the container its
    // assumptions just changed, or the row keeps the height it was given before the glyphs
    // existed.
    NotifyContentSizeChanged();

    // NOTE: deliberately does NOT write filepath back into the fontPath property.
    // The effective path may come from the theme (font role); writing it back would
    // both bake the themed path into the instance and mark it a local override,
    // defeating font-face theming. Explicit path changes go through SetFontPath.
    return true;
}

void Label::SetFont(const asset::AssetRef<asset::FontAsset>& font) {
    m_FontAsset = font;
    m_FontNeedsUpload = true;

    if (font.IsValid()) {
        SetFontPath(font->GetPath());
    }
}

void Label::UploadFontToGPU() {
    if (!m_FontAsset.IsValid() || !m_FontAsset->IsLoaded()) {
        m_FontNeedsUpload = false;
        return;
    }

    m_FontNeedsUpload = false;
}

Vec2 Label::CalculateTextSize() const {
    if (!m_FontAsset.IsValid() || !m_FontAsset->IsLoaded()) {
        return Vec2(0.0f, 0.0f);
    }

    // Measure through the same layout engine that positions the glyphs, so the
    // reported content size honors word wrap, line spacing and multiline exactly as
    // drawn. (FontAsset::MeasureText does none of that and accumulates every line's
    // width into one total, which made auto-sized multiline Labels mis-measure.)
    //
    // Only the wrap width is taken from the resolved rect: the box height would not
    // change the measured size, and reading GetBoundsSize() here would recurse
    // (GetBoundsSize -> GetMinSize -> GetContentMinSize -> here).
    const Vec2 measureBox(GetResolvedRect().size.x, 0.0f);
    TextLayoutParams params = BuildLayoutParams(measureBox);

    return TextLayout::Measure(GetText(), m_FontAsset->GetMetricsAtlas(), params);
}

Vec2 Label::GetContentMinSize() const {
    Vec2 size = CalculateTextSize();

    // The glyphs are inset from the control's rect by the content margins, and the outline
    // is stroked OUTSIDE them (up to 16px). Neither was counted, so a container sized a
    // Label to its bare text and then clipped the padding and the outline right off it.
    const Vec4 margins = GetContentMargins();
    const float outline = GetOutlineWidth();

    size.x += margins.w + margins.y + outline * 2.0f;   // left + right
    size.y += margins.x + margins.z + outline * 2.0f;   // top + bottom

    return size;
}

math::Rect Label::GetTextBoxRect() const {
    const math::Rect resolved = GetResolvedRect();
    const Vec2 offset = GetOffset();

    if (resolved.size.x > 0.0f || resolved.size.y > 0.0f) {
        // LayoutMode::Position resolves the rect straight from the authored width/height,
        // which are 0 on any axis the author left to the content (the normal way a Label
        // is written: give it a width, let the text set the height). Apply the same
        // min/max clamp GetBoundsSize() does, so the box always contains the text instead
        // of collapsing to zero height. Growing must keep the TOP edge where it is --
        // Rect::position is the MIN corner on the Y-up canvas and the text descends from
        // the top -- so the extra height is taken off the min corner, not added to it.
        const Vec2 size = ClampToMinMax(resolved.size);
        const Vec2 position(resolved.position.x,
                            resolved.position.y - (size.y - resolved.size.y));

        // Inset by the content margins, so a themed StyleBox's content margins (which
        // nothing read before) and any authored padding actually move the text.
        return GetContentInsetRect(math::Rect(position + offset, size));
    }

    // No resolved size: the box collapses onto the text itself, anchored at the node.
    const Node2D* node2D = dynamic_cast<const Node2D*>(m_Owner);
    const Vec2 globalPos = node2D ? node2D->GetGlobalPosition() : Vec2(0.0f, 0.0f);
    const Vec2 textSize = CalculateTextSize();

    Vec2 topLeft = globalPos + offset;
    if (GetCentered()) {
        topLeft.x -= textSize.x * 0.5f;
        topLeft.y += textSize.y * 0.5f;
    }

    return math::Rect(Vec2(topLeft.x, topLeft.y - textSize.y), textSize);
}

const std::string& Label::GetText() const {
    static std::string cachedText;
    std::string locKey = GetPropertyValue<std::string>("localizationKey");
    if (!locKey.empty()) {
        cachedText = localization::LocalizationManager::GetInstance().Translate(
            locKey, GetPropertyValue<std::string>("localizationTable"));
        return cachedText;
    }
    cachedText = GetPropertyValue<std::string>("text");
    return cachedText;
}

void Label::SetText(const std::string& text) {
    SetPropertyValue("text", text);
    m_MeshNeedsRegeneration = true;
}

const std::string& Label::GetFontPath() const {
    static std::string cachedPath;
    cachedPath = ResolveThemedFontPath("fontPath", "font");
    return cachedPath;
}

void Label::SetFontPath(const std::string& path) {
    // Convert to res:// path if possible
    std::string resPath = path;
    if (!path.empty() && !(path.size() >= 6 && path.substr(0, 6) == "res://")) {
        auto& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) {
            std::string converted = assetDb.ToResourcePath(path);
            if (!converted.empty()) {
                resPath = converted;
            }
        }
    }
    SetThemedProperty<std::string>("fontPath", resPath);
    m_MeshNeedsRegeneration = true;
}

float Label::GetFontSize() const {
    return ResolveThemedFontSize("fontSize", "font_size", "font");
}

void Label::SetFontSize(float size) {
    SetThemedProperty<float>("fontSize", size);

    if (m_FontHandle.isValid()) {
        m_FontHandle = FontHandle();
    }
    m_MeshNeedsRegeneration = true;
}

const std::vector<UIControl::ThemeBinding>& Label::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = {
        { "color",        "font_color",    ThemeBinding::Kind::Color },
        { "outlineColor", "outline_color", ThemeBinding::Kind::Color },
        { "fontPath",     "font",          ThemeBinding::Kind::Font },
        { "fontSize",     "font_size",     ThemeBinding::Kind::Constant }
    };
    return kBindings;
}

Color Label::GetColor() const {
    return ResolveThemedColor("color", "font_color");
}

void Label::SetColor(const Color& color) {
    SetThemedProperty<Color>("color", color);
}

bool Label::GetCentered() const {
    return GetPropertyValue<bool>("centered");
}

void Label::SetCentered(bool centered) {
    SetPropertyValue("centered", centered);
}

const Vec2& Label::GetOffset() const {
    static Vec2 cachedOffset;
    cachedOffset = GetPropertyValue<Vec2>("offset");
    return cachedOffset;
}

void Label::SetOffset(const Vec2& offset) {
    SetPropertyValue("offset", offset);
    m_MeshNeedsRegeneration = true;
}

TextHAlign Label::GetHorizontalAlign() const {
    return static_cast<TextHAlign>(GetPropertyValue<int>("horizontalAlign"));
}

void Label::SetHorizontalAlign(TextHAlign align) {
    SetPropertyValue<int>("horizontalAlign", static_cast<int>(align));
    m_MeshNeedsRegeneration = true;
}

TextVAlign Label::GetVerticalAlign() const {
    return static_cast<TextVAlign>(GetPropertyValue<int>("verticalAlign"));
}

void Label::SetVerticalAlign(TextVAlign align) {
    SetPropertyValue<int>("verticalAlign", static_cast<int>(align));
    m_MeshNeedsRegeneration = true;
}

bool Label::GetWordWrap() const {
    return GetPropertyValue<bool>("wordWrap");
}

void Label::SetWordWrap(bool wrap) {
    SetPropertyValue("wordWrap", wrap);
    m_MeshNeedsRegeneration = true;
}

bool Label::GetMultiline() const {
    return GetPropertyValue<bool>("multiline");
}

void Label::SetMultiline(bool multiline) {
    SetPropertyValue("multiline", multiline);
    m_MeshNeedsRegeneration = true;
}

float Label::GetLineSpacing() const {
    return GetPropertyValue<float>("lineSpacing");
}

void Label::SetLineSpacing(float spacing) {
    SetPropertyValue("lineSpacing", spacing);
    m_MeshNeedsRegeneration = true;
}

float Label::GetOutlineWidth() const {
    return GetPropertyValue<float>("outlineWidth");
}

void Label::SetOutlineWidth(float width) {
    SetPropertyValue("outlineWidth", width);
    m_MeshNeedsRegeneration = true;
}

Color Label::GetOutlineColor() const {
    return ResolveThemedColor("outlineColor", "outline_color");
}

void Label::SetOutlineColor(const Color& color) {
    SetThemedProperty<Color>("outlineColor", color);
    m_MeshNeedsRegeneration = true;
}

float Label::GetEffectiveFontSize(const Vec2& boxSize) const {
    const float authored = GetFontSize();

    if (!GetPropertyValue<bool>("autoShrink")) {
        return authored;
    }
    // Nothing to shrink into: with no box there is no overflow to avoid.
    if (boxSize.x <= 0.0f && boxSize.y <= 0.0f) {
        return authored;
    }
    if (!m_FontAsset.IsValid() || !m_FontAsset->IsLoaded()) {
        return authored;
    }

    const float minSize = std::max(1.0f, GetPropertyValue<float>("minFontSize"));
    if (authored <= minSize) {
        return authored;
    }

    // Step down until the measured text fits both axes of the box. Half-point steps: finer
    // than the eye can resolve at UI sizes, and bounded by (authored - minSize) * 2 measures.
    for (float size = authored; size > minSize; size -= 0.5f) {
        TextLayoutParams probe = BuildLayoutParamsAt(boxSize, size);
        const Vec2 measured = TextLayout::Measure(GetText(), m_FontAsset->GetMetricsAtlas(), probe);

        const bool fitsX = (boxSize.x <= 0.0f) || (measured.x <= boxSize.x + 0.5f);
        const bool fitsY = (boxSize.y <= 0.0f) || (measured.y <= boxSize.y + 0.5f);
        if (fitsX && fitsY) {
            return size;
        }
    }

    return minSize;
}

TextLayoutParams Label::BuildLayoutParams(const Vec2& boxSize) const {
    return BuildLayoutParamsAt(boxSize, GetEffectiveFontSize(boxSize));
}

TextLayoutParams Label::BuildLayoutParamsAt(const Vec2& boxSize, float fontSize) const {
    TextLayoutParams params;
    params.fontSize = fontSize;
    params.color = GetColor();
    params.hAlign = GetHorizontalAlign();
    params.vAlign = GetVerticalAlign();
    params.multiline = GetMultiline();
    params.wordWrap = GetWordWrap();
    params.autowrapMode = static_cast<TextAutowrapMode>(GetPropertyValue<int>("autowrapMode"));
    params.overrunBehavior = static_cast<TextOverrunBehavior>(GetPropertyValue<int>("overrunBehavior"));
    params.clipText = GetPropertyValue<bool>("clipText");
    params.tabSize = GetPropertyValue<int>("tabSize");
    params.lineSpacing = GetLineSpacing();
    params.outlineWidth = GetOutlineWidth();
    params.outlineColor = GetOutlineColor();
    params.shadowOffset = GetPropertyValue<Vec2>("shadowOffset");
    params.shadowColor = GetPropertyValue<Color>("shadowColor");
    params.boxWidth = (boxSize.x > 0.0f) ? boxSize.x : 0.0f;
    params.boxHeight = (boxSize.y > 0.0f) ? boxSize.y : 0.0f;
    return params;
}

void Label::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    UIControl::OnPropertyChanged(propertyName, newValue);

    static const char* kTextProps[] = {
        "text", "fontPath", "fontSize", "color",
        "horizontalAlign", "verticalAlign", "wordWrap", "multiline", "lineSpacing",
        "outlineWidth", "outlineColor", "centered", "offset", "width", "height"
    };
    for (const char* p : kTextProps) {
        if (propertyName == p) {
            m_MeshNeedsRegeneration = true;
            break;
        }
    }
}

void Label::RegenerateTextMesh(RenderContext& ctx) {

    if (m_TextMesh.isValid() && ctx.getDevice()) {
        ctx.getDevice()->destroyMesh(m_TextMesh);
        m_TextMesh = MeshHandle();
    }

    std::string text = GetText();

    if (text.empty() || !m_FontHandle.isValid()) {
        m_CachedTextSize = Vec2(0.0f, 0.0f);
        m_MeshNeedsRegeneration = false;
        return;
    }

    const FontAtlas* fontAtlas = ctx.getDevice()->getFontAtlas(m_FontHandle);
    if (!fontAtlas) {
        m_MeshNeedsRegeneration = false;
        return;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        m_MeshNeedsRegeneration = false;
        return;
    }

    Vec2 globalPos = node2D->GetGlobalPosition();
    float globalRotation = node2D->GetGlobalRotation();

    // Lay the text out inside the control's own box, so alignment has real slack to
    // work with and the glyphs land inside the rectangle the Label reports as its
    // bounds. TextLayout's local origin is the box TOP-left; on the Y-up canvas that
    // is the rect's max-Y corner, not Rect::position (which is the min corner).
    const math::Rect box = GetTextBoxRect();
    const Vec2 boxTopLeft(box.position.x, box.position.y + box.size.y);

    TextLayoutParams params = BuildLayoutParams(box.size);
    TextLayoutResult layout = TextLayout::Layout(text, *fontAtlas, params);
    m_CachedTextSize = layout.size;
    m_CachedBoxSize = box.size;

    if (layout.quads.empty()) {
        m_CachedText = text;
        m_CachedFontSize = params.fontSize;
        m_CachedPosition = globalPos;
        m_CachedRotation = globalRotation;
        m_MeshNeedsRegeneration = false;
        return;
    }

    const float cosR = std::cos(globalRotation);
    const float sinR = std::sin(globalRotation);
    const bool rotated = std::abs(globalRotation) > 0.0001f;
    auto toWorld = [&](float lx, float ly) -> Vec3 {
        float wx = boxTopLeft.x + lx;
        float wy = boxTopLeft.y + ly;
        if (rotated) {
            float relX = wx - globalPos.x;
            float relY = wy - globalPos.y;
            return Vec3(relX * cosR - relY * sinR + globalPos.x,
                        relX * sinR + relY * cosR + globalPos.y, 0.0f);
        }
        return Vec3(wx, wy, 0.0f);
    };

    MeshData textMeshData;
    textMeshData.vertices.reserve(layout.quads.size() * 4);
    textMeshData.indices.reserve(layout.quads.size() * 6);

    uint32_t vertexOffset = 0;
    for (const TextGlyphQuad& q : layout.quads) {
        for (int k = 0; k < 4; ++k) {
            Vertex vtx;
            vtx.position = toWorld(q.pos[k].x, q.pos[k].y);
            vtx.normal = Vec3(0.0f, 0.0f, 1.0f);
            vtx.texCoord = q.uv[k];
            vtx.color = Vec4(q.color.r, q.color.g, q.color.b, q.color.a);
            textMeshData.vertices.push_back(vtx);
        }
        textMeshData.indices.push_back(vertexOffset + 0);
        textMeshData.indices.push_back(vertexOffset + 1);
        textMeshData.indices.push_back(vertexOffset + 2);
        textMeshData.indices.push_back(vertexOffset + 0);
        textMeshData.indices.push_back(vertexOffset + 2);
        textMeshData.indices.push_back(vertexOffset + 3);
        vertexOffset += 4;
    }

    textMeshData.calculateBounds();

    m_TextMesh = ctx.getDevice()->createMesh(textMeshData);
    if (m_TextMesh.isValid()) {
        m_CachedMeshBounds = textMeshData.bounds;
    }

    m_CachedText = text;
    m_CachedFontSize = params.fontSize;
    m_CachedPosition = globalPos;
    m_CachedRotation = globalRotation;
    m_MeshNeedsRegeneration = false;
}

void Label::buildDrawCommands(RenderContext& ctx) {

    if (!m_Owner) {

        return;
    }


    // Regenerate the (colour-baked) text mesh when the theme/palette changes.
    if (ConsumeThemeVersionChanged()) {
        m_MeshNeedsRegeneration = true;
    }

    // A resolution change re-bakes the font atlas at a new oversampling density,
    // which repacks glyph UVs; regenerate the cached mesh so it matches.
    if (ConsumeFontOversampleChanged()) {
        m_MeshNeedsRegeneration = true;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {

        return;
    }

    std::string fontPath = GetFontPath();

    // If no font path set, try to use the default font
    if (fontPath.empty()) {
        auto& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) {
            fontPath = assetDb.GetDefaultFontPath();
        }
    }

    float currentFontSize = GetFontSize();
    bool fontPathChanged = (fontPath != m_CurrentFontPath);
    bool fontSizeChanged = (currentFontSize != m_CurrentFontSize);

    if (fontPathChanged || fontSizeChanged) {
        if (fontPathChanged) {

        }
        if (fontSizeChanged) {

        }

        m_CurrentFontPath = fontPath;
        m_CurrentFontSize = currentFontSize;

        if (m_FontHandle.isValid()) {

            m_FontHandle = FontHandle();
        }

        if (m_TextMesh.isValid() && ctx.getDevice()) {
            ctx.getDevice()->destroyMesh(m_TextMesh);
            m_TextMesh = MeshHandle();
        }

        if (!fontPath.empty()) {
            LoadFont(fontPath);
        } else {

            m_FontAsset.Reset();
        }
        m_MeshNeedsRegeneration = true;
    }

    if (m_FontAsset.IsValid() && m_FontAsset->IsLoaded() && !m_FontHandle.isValid()) {

        FontDesc fontDesc;
        // Use GetPhysicalPath() to resolve res:// path to filesystem path for file loading
        fontDesc.fontPath = m_FontAsset->GetPhysicalPath();
        fontDesc.fontSize = GetFontSize();
        fontDesc.atlasWidth = m_FontAsset->GetAtlasWidth();
        fontDesc.atlasHeight = m_FontAsset->GetAtlasHeight();

        IGfxDevice* device = ctx.getDevice();
        if (device) {
            m_FontHandle = device->createFontAtlas(fontDesc);
            if (m_FontHandle.isValid()) {

            } else {

            }
        }
        m_MeshNeedsRegeneration = true;
    }

    if (!m_FontHandle.isValid()) {

        return;
    }

    std::string text = GetText();
    if (text.empty()) {
        return;
    }

    Vec2 currentPos = node2D->GetGlobalPosition();
    float currentRotation = node2D->GetGlobalRotation();

    // Re-lay-out when the box the text is aligned in changes, which includes anchor
    // stretching and container-assigned sizes, not just the authored width/height.
    const Vec2 boxSize = GetTextBoxRect().size;

    if (text != m_CachedText || GetFontSize() != m_CachedFontSize ||
        currentPos != m_CachedPosition || std::abs(currentRotation - m_CachedRotation) > 0.0001f ||
        boxSize != m_CachedBoxSize) {
        m_MeshNeedsRegeneration = true;
    }

    if (m_MeshNeedsRegeneration || !m_TextMesh.isValid()) {
        RegenerateTextMesh(ctx);
    }

    // When the laid-out text size changes (notably the first time the font finishes
    // loading, when it jumps from zero to the real size), tell the parent container so
    // an auto-sized layout — e.g. a VerticalContainer inside a ScrollContainer — picks
    // up the new content height instead of staying at the stale measurement.
    if (m_CachedTextSize != m_LastPropagatedTextSize) {
        m_LastPropagatedTextSize = m_CachedTextSize;
        InvalidateParentContainerLayout();
    }

    if (!m_TextMesh.isValid()) {
        return;
    }

    const FontAtlas* fontAtlas = ctx.getDevice()->getFontAtlas(m_FontHandle);
    if (!fontAtlas) {
        return;
    }

    MaterialPropertyBlock overrides;
    overrides.setTexture("u_FontAtlas", fontAtlas->texture);
    overrides.setColor("u_TextColor", GetColor());

    Mat4 transform = Mat4::Identity();
    ctx.drawMesh(m_TextMesh, ctx.getDefaultTextMaterial(), transform, overrides);
}

AABB Label::getWorldBounds() const {
    if (!m_Owner) {
        return AABB();
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return AABB();
    }

    const Vec2 globalPos = node2D->GetGlobalPosition();

    // The bounds ARE the layout box, so the selection rectangle in the editor is the
    // rectangle the text is aligned inside. Deriving them from a separate glyph walk
    // (as this used to) makes them disagree with the drawn text the moment any
    // alignment, wrapping or line spacing is in play.
    const math::Rect box = GetTextBoxRect();
    if (box.size.x <= 0.0f && box.size.y <= 0.0f) {
        return AABB(
            Vec3(globalPos.x - 5.0f, globalPos.y - 5.0f, -0.1f),
            Vec3(globalPos.x + 5.0f, globalPos.y + 5.0f, 0.1f)
        );
    }

    const float globalRotation = node2D->GetGlobalRotation();

    const Vec2 corners[4] = {
        Vec2(box.position.x, box.position.y),
        Vec2(box.position.x + box.size.x, box.position.y),
        Vec2(box.position.x + box.size.x, box.position.y + box.size.y),
        Vec2(box.position.x, box.position.y + box.size.y)
    };

    float minX, maxX, minY, maxY;

    if (std::abs(globalRotation) > 0.0001f) {
        const float cosR = std::cos(globalRotation);
        const float sinR = std::sin(globalRotation);

        // The box is already in world space, so rotate it about the node's origin.
        auto rotate = [&](const Vec2& p) -> Vec2 {
            const float relX = p.x - globalPos.x;
            const float relY = p.y - globalPos.y;
            return Vec2(relX * cosR - relY * sinR + globalPos.x,
                        relX * sinR + relY * cosR + globalPos.y);
        };

        Vec2 r = rotate(corners[0]);
        minX = maxX = r.x;
        minY = maxY = r.y;

        for (int i = 1; i < 4; ++i) {
            r = rotate(corners[i]);
            minX = std::min(minX, r.x);
            maxX = std::max(maxX, r.x);
            minY = std::min(minY, r.y);
            maxY = std::max(maxY, r.y);
        }
    } else {
        minX = corners[0].x;
        maxX = corners[2].x;
        minY = corners[0].y;
        maxY = corners[2].y;
    }

    return AABB(
        Vec3(minX, minY, -0.1f),
        Vec3(maxX, maxY, 0.1f)
    );
}

RenderLayer Label::getRenderLayer() const {

    return RenderLayer::Transparent;
}

math::OBB Label::getOrientedBounds() const {
    if (!m_Owner) {
        return math::OBB();
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return math::OBB();
    }

    const Vec2 globalPos = node2D->GetGlobalPosition();
    const float globalRotation = node2D->GetGlobalRotation();
    const Quat rotation = Quat::FromAxisAngle(Vec3::UnitZ(), globalRotation);

    // Same box as getWorldBounds(): the rect the text is laid out in.
    const math::Rect box = GetTextBoxRect();
    if (box.size.x <= 0.0f && box.size.y <= 0.0f) {
        return math::OBB(Vec3(globalPos.x, globalPos.y, 0.0f), Vec3(5.0f, 5.0f, 0.1f), rotation);
    }

    const Vec2 boxCenter(box.position.x + box.size.x * 0.5f,
                         box.position.y + box.size.y * 0.5f);

    Vec2 worldCenter = boxCenter;
    if (std::abs(globalRotation) > 0.0001f) {
        const float cosR = std::cos(globalRotation);
        const float sinR = std::sin(globalRotation);
        const float relX = boxCenter.x - globalPos.x;
        const float relY = boxCenter.y - globalPos.y;
        worldCenter.x = relX * cosR - relY * sinR + globalPos.x;
        worldCenter.y = relX * sinR + relY * cosR + globalPos.y;
    }

    return math::OBB(Vec3(worldCenter.x, worldCenter.y, 0.0f),
                     Vec3(box.size.x * 0.5f, box.size.y * 0.5f, 0.1f),
                     rotation);
}

bool Label::IntersectRay(const math::Ray& ray, float& outDistance) const {

    math::OBB obb = getOrientedBounds();

    Vec3 localRayOrigin = obb.rotation.Inverse() * (ray.origin - obb.center);
    Vec3 localRayDir = obb.rotation.Inverse() * ray.direction;

    AABB localAABB(Vec3(-obb.extents.x, -obb.extents.y, -obb.extents.z),
                   Vec3(obb.extents.x, obb.extents.y, obb.extents.z));

    math::Ray localRay(localRayOrigin, localRayDir);
    return localRay.IntersectAABB(localAABB, outDistance);
}

bool Label::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
    std::string fontPath = GetFontPath();
    if (fontPath.empty()) return false;

    auto& assetDb = asset::AssetDatabase::GetInstance();
    std::string resolvedFontPath;
    if (assetDb.IsInitialized()) {
        resolvedFontPath = assetDb.ResolveAsset(fontPath);
    }

    bool matches = (fontPath == changedPath) ||
                   (!resolvedFontPath.empty() && !resolvedChangedPath.empty() &&
                    resolvedFontPath == resolvedChangedPath);

    if (matches) {
        
        m_FontHandle = FontHandle();
        m_FontAsset.Reset();
        m_CurrentFontPath.clear();
        m_FontNeedsUpload = true;
        m_MeshNeedsRegeneration = true;
        return true;
    }

    return false;
}

}
}

