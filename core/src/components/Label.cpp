#include "lupine/components/Label.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Project.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/math/OBB.hpp"
#include "lupine/math/Ray.hpp"
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

Label::Label()
    : Component("Label")
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
    : Component(name)
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

    DefineProperty(PROPERTY_DEFAULT_GROUP(text, String, std::string("Label"), "Text"));
    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 256.0f, 1.0f, "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(color, Color, Color::White(), "Text"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(centered, Bool, false, "Layout"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(offset, Vec2, Vec2(0.0f, 0.0f), "Layout"));
}

void Label::OnAwake() {

    std::string fontPath = GetFontPath();
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

bool Label::OnGizmoScale(float scaleDelta, int axis, bool is3D) {
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

    SetFontPath(filepath);

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

    std::string text = GetText();
    float fontSize = GetFontSize();

    return m_FontAsset->MeasureText(text, fontSize);
}

const std::string& Label::GetText() const {
    static std::string cachedText;
    cachedText = GetPropertyValue<std::string>("text");
    return cachedText;
}

void Label::SetText(const std::string& text) {
    SetPropertyValue("text", text);
    m_MeshNeedsRegeneration = true;
}

const std::string& Label::GetFontPath() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("fontPath");
    return cachedPath;
}

void Label::SetFontPath(const std::string& path) {
    SetPropertyValue("fontPath", path);
    m_MeshNeedsRegeneration = true;
}

float Label::GetFontSize() const {
    return GetPropertyValue<float>("fontSize");
}

void Label::SetFontSize(float size) {
    SetPropertyValue("fontSize", size);

    if (m_FontHandle.isValid()) {
        m_FontHandle = FontHandle();
    }
    m_MeshNeedsRegeneration = true;
}

const Color& Label::GetColor() const {
    static Color cachedColor;
    cachedColor = GetPropertyValue<Color>("color");
    return cachedColor;
}

void Label::SetColor(const Color& color) {
    SetPropertyValue("color", color);
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
}

void Label::RegenerateTextMesh(RenderContext& ctx) {

    if (m_TextMesh.isValid() && ctx.getDevice()) {
        ctx.getDevice()->destroyMesh(m_TextMesh);
        m_TextMesh = MeshHandle();
    }

    std::string text = GetText();
    float fontSize = GetFontSize();

    if (text.empty() || !m_FontHandle.isValid()) {
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
    Vec2 offset = GetOffset();

    if (GetCentered()) {
        Vec2 textSize = CalculateTextSize();
        offset.x -= textSize.x * 0.5f;
        offset.y -= textSize.y * 0.5f;
    }

    Vec2 rotatedOffset = offset;
    if (std::abs(globalRotation) > 0.0001f) {
        float cosR = std::cos(globalRotation);
        float sinR = std::sin(globalRotation);
        rotatedOffset.x = offset.x * cosR - offset.y * sinR;
        rotatedOffset.y = offset.x * sinR + offset.y * cosR;
    }

    Vec2 finalPos = globalPos + rotatedOffset;
    Color color = GetColor();

    MeshData textMeshData;
    float scale = fontSize / fontAtlas->fontSize;
    Vec2 cursor = finalPos;

    uint32_t vertexOffset = 0;
    for (size_t i = 0; i < text.length(); ++i) {
        char c = text[i];
        uint32_t codepoint = static_cast<uint32_t>(c);

        if (c == '\n') {
            cursor.x = finalPos.x;
            cursor.y += fontAtlas->lineHeight * scale;
            continue;
        }

        const Glyph* glyph = fontAtlas->getGlyph(codepoint);
        if (!glyph) {
            if (c == ' ') {
                cursor.x += fontAtlas->fontSize * 0.25f * scale;
            }
            continue;
        }

        float glyphX = cursor.x + glyph->bearing.x * scale;
        float glyphY = cursor.y - glyph->bearing.y * scale;
        float glyphW = glyph->size.x * scale;
        float glyphH = glyph->size.y * scale;

        auto rotateVertex = [&](float x, float y) -> Vec3 {
            if (std::abs(globalRotation) > 0.0001f) {

                float relX = x - globalPos.x;
                float relY = y - globalPos.y;

                float cosR = std::cos(globalRotation);
                float sinR = std::sin(globalRotation);
                float rotX = relX * cosR - relY * sinR;
                float rotY = relX * sinR + relY * cosR;

                return Vec3(rotX + globalPos.x, rotY + globalPos.y, 0.0f);
            }
            return Vec3(x, y, 0.0f);
        };

        Vertex v0, v1, v2, v3;
        v0.position = rotateVertex(glyphX, glyphY - glyphH);
        v0.texCoord = Vec2(glyph->uvMin.x, glyph->uvMax.y);
        v0.color = Vec4(color.r, color.g, color.b, color.a);

        v1.position = rotateVertex(glyphX + glyphW, glyphY - glyphH);
        v1.texCoord = Vec2(glyph->uvMax.x, glyph->uvMax.y);
        v1.color = Vec4(color.r, color.g, color.b, color.a);

        v2.position = rotateVertex(glyphX + glyphW, glyphY);
        v2.texCoord = Vec2(glyph->uvMax.x, glyph->uvMin.y);
        v2.color = Vec4(color.r, color.g, color.b, color.a);

        v3.position = rotateVertex(glyphX, glyphY);
        v3.texCoord = Vec2(glyph->uvMin.x, glyph->uvMin.y);
        v3.color = Vec4(color.r, color.g, color.b, color.a);

        textMeshData.vertices.push_back(v0);
        textMeshData.vertices.push_back(v1);
        textMeshData.vertices.push_back(v2);
        textMeshData.vertices.push_back(v3);

        textMeshData.indices.push_back(vertexOffset + 0);
        textMeshData.indices.push_back(vertexOffset + 1);
        textMeshData.indices.push_back(vertexOffset + 2);
        textMeshData.indices.push_back(vertexOffset + 0);
        textMeshData.indices.push_back(vertexOffset + 2);
        textMeshData.indices.push_back(vertexOffset + 3);

        vertexOffset += 4;

        cursor.x += glyph->advance * scale;
    }

    if (textMeshData.vertices.empty()) {
        m_MeshNeedsRegeneration = false;
        return;
    }

    textMeshData.calculateBounds();

    m_TextMesh = ctx.getDevice()->createMesh(textMeshData);
    if (!m_TextMesh.isValid()) {

    } else {

        m_CachedMeshBounds = textMeshData.bounds;
    }

    m_CachedText = text;
    m_CachedFontSize = fontSize;
    m_CachedPosition = globalPos;
    m_CachedRotation = globalRotation;
    m_MeshNeedsRegeneration = false;
}

void Label::buildDrawCommands(RenderContext& ctx) {

    if (!m_Owner) {

        return;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {

        return;
    }

    std::string fontPath = GetFontPath();
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
        fontDesc.fontPath = m_FontAsset->GetPath();
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

    if (text != m_CachedText || GetFontSize() != m_CachedFontSize ||
        currentPos != m_CachedPosition || std::abs(currentRotation - m_CachedRotation) > 0.0001f) {
        m_MeshNeedsRegeneration = true;
    }

    if (m_MeshNeedsRegeneration || !m_TextMesh.isValid()) {
        RegenerateTextMesh(ctx);
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

    std::string text = GetText();
    if (text.empty() || !m_FontAsset.IsValid() || !m_FontAsset->IsLoaded()) {

        Vec2 globalPos = node2D->GetGlobalPosition();
        return AABB(
            Vec3(globalPos.x - 5.0f, globalPos.y - 5.0f, -0.1f),
            Vec3(globalPos.x + 5.0f, globalPos.y + 5.0f, 0.1f)
        );
    }

    Vec2 globalPos = node2D->GetGlobalPosition();
    float globalRotation = node2D->GetGlobalRotation();
    Vec2 offset = GetOffset();
    float fontSize = GetFontSize();

    if (GetCentered()) {
        Vec2 textSize = CalculateTextSize();
        offset.x -= textSize.x * 0.5f;
        offset.y -= textSize.y * 0.5f;
    }

    float scale = fontSize / m_FontAsset->GetFontSize();
    Vec2 localCursor = Vec2(0.0f, 0.0f);

    float localMinX = 0.0f;
    float localMaxX = 0.0f;
    float localMinY = 0.0f;
    float localMaxY = 0.0f;

    bool hasGlyphs = false;

    for (size_t i = 0; i < text.length(); ++i) {
        char c = text[i];
        uint32_t codepoint = static_cast<uint32_t>(c);

        if (c == '\n') {
            localCursor.x = 0.0f;
            localCursor.y += m_FontAsset->GetLineHeight() * scale;
            continue;
        }

        const asset::Glyph* glyph = m_FontAsset->GetGlyph(codepoint);
        if (!glyph) {
            if (c == ' ') {
                localCursor.x += m_FontAsset->GetFontSize() * 0.25f * scale;
            }
            continue;
        }

        float glyphX = localCursor.x + glyph->bearingX * scale;
        float glyphY = localCursor.y + glyph->bearingY * scale;
        float glyphW = glyph->width * scale;
        float glyphH = glyph->height * scale;

        float quadMinX = glyphX;
        float quadMaxX = glyphX + glyphW;
        float quadMinY = glyphY - glyphH;
        float quadMaxY = glyphY;

        if (!hasGlyphs) {
            localMinX = quadMinX;
            localMaxX = quadMaxX;
            localMinY = quadMinY;
            localMaxY = quadMaxY;
            hasGlyphs = true;
        } else {
            localMinX = std::min(localMinX, quadMinX);
            localMaxX = std::max(localMaxX, quadMaxX);
            localMinY = std::min(localMinY, quadMinY);
            localMaxY = std::max(localMaxY, quadMaxY);
        }

        localCursor.x += glyph->advance * scale;
    }

    Vec2 localCorners[4] = {
        Vec2(localMinX + offset.x, localMinY + offset.y),
        Vec2(localMaxX + offset.x, localMinY + offset.y),
        Vec2(localMaxX + offset.x, localMaxY + offset.y),
        Vec2(localMinX + offset.x, localMaxY + offset.y)
    };

    float minX, maxX, minY, maxY;

    if (std::abs(globalRotation) > 0.0001f) {
        float cosR = std::cos(globalRotation);
        float sinR = std::sin(globalRotation);

        float rotX = localCorners[0].x * cosR - localCorners[0].y * sinR + globalPos.x;
        float rotY = localCorners[0].x * sinR + localCorners[0].y * cosR + globalPos.y;
        minX = maxX = rotX;
        minY = maxY = rotY;

        for (int i = 1; i < 4; ++i) {
            rotX = localCorners[i].x * cosR - localCorners[i].y * sinR + globalPos.x;
            rotY = localCorners[i].x * sinR + localCorners[i].y * cosR + globalPos.y;
            minX = std::min(minX, rotX);
            maxX = std::max(maxX, rotX);
            minY = std::min(minY, rotY);
            maxY = std::max(maxY, rotY);
        }
    } else {

        minX = localCorners[0].x + globalPos.x;
        maxX = localCorners[2].x + globalPos.x;
        minY = localCorners[0].y + globalPos.y;
        maxY = localCorners[2].y + globalPos.y;
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

    std::string text = GetText();
    if (text.empty() || !m_FontAsset.IsValid() || !m_FontAsset->IsLoaded()) {

        Vec2 globalPos = node2D->GetGlobalPosition();
        float globalRotation = node2D->GetGlobalRotation();
        Quat rotation = Quat::FromAxisAngle(Vec3::UnitZ(), globalRotation);
        return math::OBB(Vec3(globalPos.x, globalPos.y, 0.0f), Vec3(5.0f, 5.0f, 0.1f), rotation);
    }

    Vec2 globalPos = node2D->GetGlobalPosition();
    float globalRotation = node2D->GetGlobalRotation();
    Vec2 offset = GetOffset();
    float fontSize = GetFontSize();

    if (GetCentered()) {
        Vec2 textSize = CalculateTextSize();
        offset.x -= textSize.x * 0.5f;
        offset.y -= textSize.y * 0.5f;
    }

    float scale = fontSize / m_FontAsset->GetFontSize();
    Vec2 localCursor = Vec2(0.0f, 0.0f);

    float localMinX = 0.0f;
    float localMaxX = 0.0f;
    float localMinY = 0.0f;
    float localMaxY = 0.0f;

    bool hasGlyphs = false;

    for (size_t i = 0; i < text.length(); ++i) {
        char c = text[i];
        uint32_t codepoint = static_cast<uint32_t>(c);

        if (c == '\n') {
            localCursor.x = 0.0f;
            localCursor.y += m_FontAsset->GetLineHeight() * scale;
            continue;
        }

        const asset::Glyph* glyph = m_FontAsset->GetGlyph(codepoint);
        if (!glyph) {
            if (c == ' ') {
                localCursor.x += m_FontAsset->GetFontSize() * 0.25f * scale;
            }
            continue;
        }

        float glyphX = localCursor.x + glyph->bearingX * scale;
        float glyphY = localCursor.y + glyph->bearingY * scale;
        float glyphW = glyph->width * scale;
        float glyphH = glyph->height * scale;

        float quadMinX = glyphX;
        float quadMaxX = glyphX + glyphW;
        float quadMinY = glyphY - glyphH;
        float quadMaxY = glyphY;

        if (!hasGlyphs) {
            localMinX = quadMinX;
            localMaxX = quadMaxX;
            localMinY = quadMinY;
            localMaxY = quadMaxY;
            hasGlyphs = true;
        } else {
            localMinX = std::min(localMinX, quadMinX);
            localMaxX = std::max(localMaxX, quadMaxX);
            localMinY = std::min(localMinY, quadMinY);
            localMaxY = std::max(localMaxY, quadMaxY);
        }

        localCursor.x += glyph->advance * scale;
    }

    Vec2 localCenter = Vec2((localMinX + localMaxX) * 0.5f, (localMinY + localMaxY) * 0.5f);
    Vec2 localExtents = Vec2((localMaxX - localMinX) * 0.5f, (localMaxY - localMinY) * 0.5f);

    localCenter += offset;

    Vec2 rotatedCenter = localCenter;
    if (std::abs(globalRotation) > 0.0001f) {
        float cosR = std::cos(globalRotation);
        float sinR = std::sin(globalRotation);
        rotatedCenter.x = localCenter.x * cosR - localCenter.y * sinR;
        rotatedCenter.y = localCenter.x * sinR + localCenter.y * cosR;
    }

    Vec3 worldCenter = Vec3(globalPos.x + rotatedCenter.x, globalPos.y + rotatedCenter.y, 0.0f);
    Vec3 worldExtents = Vec3(localExtents.x, localExtents.y, 0.1f);
    Quat rotation = Quat::FromAxisAngle(Vec3::UnitZ(), globalRotation);

    return math::OBB(worldCenter, worldExtents, rotation);
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

}
}

