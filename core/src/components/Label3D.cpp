#include "lupine/components/Label3D.hpp"
#include "lupine/localization/LocalizationManager.hpp"
#include "lupine/components/Sprite3D.hpp"
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
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

Label3D::Label3D()
    : Component("Label3D")
    , m_FontHandle()
    , m_FontNeedsUpload(false)
    , m_CurrentFontSize(0.0f)
    , m_TextMesh()
    , m_CachedFontSize(0.0f)
    , m_MeshNeedsRegeneration(true)
{
}

Label3D::Label3D(const std::string& name)
    : Component(name)
    , m_FontHandle()
    , m_FontNeedsUpload(false)
    , m_CurrentFontSize(0.0f)
    , m_TextMesh()
    , m_CachedFontSize(0.0f)
    , m_MeshNeedsRegeneration(true)
{
}

Label3D::~Label3D() {

}

void Label3D::DefineProperties() {

    DefineProperty(PROPERTY_DEFAULT_GROUP(text, String, std::string("Label3D"), "Text"));

    // localizationKey overrides "text" and resolves through the LocalizationManager.
    DefineProperty(PROPERTY_DEFAULT_GROUP(localizationKey, String, std::string(""), "Localization"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(localizationTable, String, std::string(""), "Localization"));
    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 256.0f, 1.0f, "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(color, Color, Color::White(), "Text"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(centered, Bool, false, "Layout"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(offset, Vec2, Vec2(0.0f, 0.0f), "Layout"));

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pixelSize, 0.0f, 0.0f, 10.0f, 0.01f, "Transform"));
    DefineProperty(PROPERTY_ENUM_GROUP(billboardMode, 0, "Transform", Disabled, Enabled, YAxisOnly));
}

void Label3D::OnAwake() {

    std::string fontPath = GetFontPath();
    if (!fontPath.empty()) {
        LoadFont(fontPath);
    }
}

void Label3D::OnReady() {

    if (m_FontNeedsUpload) {
        UploadFontToGPU();
    }
}

void Label3D::OnRender() {

}

bool Label3D::OnGizmoScale(float scaleDelta, int, bool is3D) {
    if (is3D) {

        float currentSize = GetFontSize();

        float newSize = std::max(1.0f, currentSize + scaleDelta * currentSize);

        newSize = std::min(256.0f, newSize);

        SetFontSize(newSize);

        m_MeshNeedsRegeneration = true;

        return true;
    }
    return false;
}

bool Label3D::LoadFont(const std::string& filepath) {
    if (filepath.empty()) {
        return false;
    }

    m_FontAsset = asset::AssetRef<asset::FontAsset>(new asset::FontAsset());

    float fontSize = GetFontSize();

    uint32_t minAtlasSize = static_cast<uint32_t>(fontSize * 20.0f);
    uint32_t atlasSize = 1024;
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

void Label3D::SetFont(const asset::AssetRef<asset::FontAsset>& font) {
    m_FontAsset = font;
    m_FontNeedsUpload = true;

    if (font.IsValid()) {
        SetFontPath(font->GetPath());
    }
}

void Label3D::UploadFontToGPU() {
    if (!m_FontAsset.IsValid() || !m_FontAsset->IsLoaded()) {
        m_FontNeedsUpload = false;
        return;
    }

    m_FontNeedsUpload = false;
}

Vec2 Label3D::CalculateTextSize() const {
    if (!m_FontAsset.IsValid() || !m_FontAsset->IsLoaded()) {
        return Vec2(0.0f, 0.0f);
    }

    std::string text = GetText();
    float fontSize = GetFontSize();

    return m_FontAsset->MeasureText(text, fontSize);
}

Vec2 Label3D::CalculateRenderSize() const {
    Vec2 size = CalculateTextSize();
    float pixelSize = GetPixelSize();

    if (pixelSize > 0.0f) {
        if (size.y > 0.0f) {
            float aspect = size.x / size.y;
            size.x = pixelSize * aspect;
            size.y = pixelSize;
        }
    } else {

        size.x *= 0.01f;
        size.y *= 0.01f;
    }

    return size;
}

Mat4 Label3D::CalculateBillboardTransform(const Mat4& viewMatrix) const {
    BillboardMode mode = GetBillboardMode();

    if (mode == BillboardMode::Disabled) {
        return Mat4::Identity();
    }

    if (!m_Owner) {
        return Mat4::Identity();
    }

    core::Node3D* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (!node3D) {
        return Mat4::Identity();
    }

    Vec3 position = node3D->GetGlobalPosition();
    Vec3 scale = node3D->GetGlobalScale();

    if (mode == BillboardMode::Enabled) {

        glm::mat4 glmView = viewMatrix.ToGLM();
        Vec3 right(glmView[0][0], glmView[1][0], glmView[2][0]);
        Vec3 up(glmView[0][1], glmView[1][1], glmView[2][1]);
        Vec3 forward = Cross(right, up).Normalized();

        glm::mat4 billboard(1.0f);
        billboard[0] = glm::vec4(right.x * scale.x, right.y * scale.x, right.z * scale.x, 0.0f);
        billboard[1] = glm::vec4(up.x * scale.y, up.y * scale.y, up.z * scale.y, 0.0f);
        billboard[2] = glm::vec4(forward.x * scale.z, forward.y * scale.z, forward.z * scale.z, 0.0f);
        billboard[3] = glm::vec4(position.x, position.y, position.z, 1.0f);

        return Mat4(billboard);
    } else if (mode == BillboardMode::YAxisOnly) {

        glm::mat4 glmView = viewMatrix.ToGLM();
        glm::mat4 invView = glm::inverse(glmView);
        Vec3 cameraPos(invView[3][0], invView[3][1], invView[3][2]);

        Vec3 toCamera = (cameraPos - position);
        toCamera.y = 0.0f;
        toCamera = toCamera.Normalized();

        Vec3 up(0, 1, 0);
        Vec3 right = Cross(up, toCamera).Normalized();
        Vec3 forward = toCamera;

        glm::mat4 billboard(1.0f);
        billboard[0] = glm::vec4(right.x * scale.x, right.y * scale.x, right.z * scale.x, 0.0f);
        billboard[1] = glm::vec4(up.x * scale.y, up.y * scale.y, up.z * scale.y, 0.0f);
        billboard[2] = glm::vec4(forward.x * scale.z, forward.y * scale.z, forward.z * scale.z, 0.0f);
        billboard[3] = glm::vec4(position.x, position.y, position.z, 1.0f);

        return Mat4(billboard);
    }

    return Mat4::Identity();
}

const std::string& Label3D::GetText() const {
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

void Label3D::SetText(const std::string& text) {
    SetPropertyValue("text", text);
    m_MeshNeedsRegeneration = true;
}

const std::string& Label3D::GetFontPath() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("fontPath");
    return cachedPath;
}

void Label3D::SetFontPath(const std::string& path) {
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
    SetPropertyValue("fontPath", resPath);
    m_MeshNeedsRegeneration = true;
}

float Label3D::GetFontSize() const {
    return GetPropertyValue<float>("fontSize");
}

void Label3D::SetFontSize(float size) {
    SetPropertyValue("fontSize", size);

    if (m_FontHandle.isValid()) {
        m_FontHandle = FontHandle();
    }
    m_MeshNeedsRegeneration = true;
}

Color Label3D::GetColor() const {
    return GetPropertyValue<Color>("color");
}

void Label3D::SetColor(const Color& color) {
    SetPropertyValue("color", color);
}

bool Label3D::GetCentered() const {
    return GetPropertyValue<bool>("centered");
}

void Label3D::SetCentered(bool centered) {
    SetPropertyValue("centered", centered);
}

const Vec2& Label3D::GetOffset() const {
    static Vec2 cachedOffset;
    cachedOffset = GetPropertyValue<Vec2>("offset");
    return cachedOffset;
}

void Label3D::SetOffset(const Vec2& offset) {
    SetPropertyValue("offset", offset);
}

BillboardMode Label3D::GetBillboardMode() const {
    int mode = GetPropertyValue<int>("billboardMode");
    return static_cast<BillboardMode>(mode);
}

void Label3D::SetBillboardMode(BillboardMode mode) {
    SetPropertyValue<int>("billboardMode", static_cast<int>(mode));
}

float Label3D::GetPixelSize() const {
    return GetPropertyValue<float>("pixelSize");
}

void Label3D::SetPixelSize(float size) {
    SetPropertyValue("pixelSize", size);
}

void Label3D::RegenerateTextMesh(RenderContext& ctx) {

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

    Color color = GetColor();

    MeshData textMeshData;
    float scale = fontSize / fontAtlas->fontSize;
    Vec2 cursor(0.0f, 0.0f);

    uint32_t vertexOffset = 0;
    for (size_t i = 0; i < text.length(); ++i) {
        char c = text[i];
        uint32_t codepoint = static_cast<uint32_t>(c);

        if (c == '\n') {
            cursor.x = 0.0f;
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

        Vertex v0, v1, v2, v3;
        v0.position = Vec3(glyphX, glyphY - glyphH, 0.0f);
        v0.normal = Vec3(0.0f, 0.0f, 1.0f);
        v0.texCoord = Vec2(glyph->uvMin.x, glyph->uvMax.y);
        v0.color = Vec4(color.r, color.g, color.b, color.a);

        v1.position = Vec3(glyphX + glyphW, glyphY - glyphH, 0.0f);
        v1.normal = Vec3(0.0f, 0.0f, 1.0f);
        v1.texCoord = Vec2(glyph->uvMax.x, glyph->uvMax.y);
        v1.color = Vec4(color.r, color.g, color.b, color.a);

        v2.position = Vec3(glyphX + glyphW, glyphY, 0.0f);
        v2.normal = Vec3(0.0f, 0.0f, 1.0f);
        v2.texCoord = Vec2(glyph->uvMax.x, glyph->uvMin.y);
        v2.color = Vec4(color.r, color.g, color.b, color.a);

        v3.position = Vec3(glyphX, glyphY, 0.0f);
        v3.normal = Vec3(0.0f, 0.0f, 1.0f);
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
    AABB pixelBounds = textMeshData.bounds;

    Vec3 pixelSize = pixelBounds.max - pixelBounds.min;
    Vec3 pixelCenter = (pixelBounds.min + pixelBounds.max) * 0.5f;

    if (pixelSize.x > 0.0f && pixelSize.y > 0.0f) {
        for (auto& vertex : textMeshData.vertices) {

            vertex.position.x -= pixelCenter.x;
            vertex.position.y -= pixelCenter.y;

            vertex.position.x /= pixelSize.x;
            vertex.position.y /= pixelSize.y;
        }

        textMeshData.calculateBounds();
    }

    m_TextMesh = ctx.getDevice()->createMesh(textMeshData);
    if (!m_TextMesh.isValid()) {

    } else {

        m_CachedMeshBounds = textMeshData.bounds;
    }

    m_CachedText = text;
    m_CachedFontSize = fontSize;
    m_MeshNeedsRegeneration = false;
}

void Label3D::buildDrawCommands(RenderContext& ctx) {

    if (!m_Owner) {
        return;
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return;
    }

    // A resolution change re-bakes the font atlas at a new oversampling density,
    // which repacks glyph UVs; regenerate the cached text mesh so it matches.
    if (ConsumeFontOversampleChanged()) {
        m_MeshNeedsRegeneration = true;
    }

    std::string fontPath = GetFontPath();
    float currentFontSize = GetFontSize();
    bool fontPathChanged = (fontPath != m_CurrentFontPath);
    bool fontSizeChanged = (currentFontSize != m_CurrentFontSize);

    if (fontPathChanged || fontSizeChanged) {
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

    if (text != m_CachedText || GetFontSize() != m_CachedFontSize) {
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

    Mat4 transform;
    BillboardMode billboardMode = GetBillboardMode();

    Vec2 size = CalculateRenderSize();

    if (billboardMode != BillboardMode::Disabled) {

        Mat4 billboardTransform = CalculateBillboardTransform(ctx.getViewMatrix());

        Mat4 scale = Mat4::Scale(Vec3(size.x, size.y, 1.0f));
        transform = billboardTransform * scale;
    } else {

        Vec3 pos = node3D->GetGlobalPosition();
        Quat rot = node3D->GetGlobalRotation();
        Vec3 scale = node3D->GetGlobalScale();

        glm::quat glmQuat = rot.ToGLM();
        glm::mat4 glmRot = glm::mat4_cast(glmQuat);
        Mat4 rotMatrix(glmRot);

        Mat4 scaleMatrix = Mat4::Scale(Vec3(size.x * scale.x, size.y * scale.y, scale.z));

        Mat4 translateMatrix = Mat4::Translate(pos);
        transform = translateMatrix * rotMatrix * scaleMatrix;
    }

    MaterialPropertyBlock overrides;
    overrides.setTexture("u_FontAtlas", fontAtlas->texture);
    overrides.setColor("u_TintColor", GetColor());

    MaterialHandle material = ctx.getDefaultText3DMaterial();

    ctx.drawMesh(m_TextMesh, material, transform, overrides, 0, false, true);
}

AABB Label3D::getWorldBounds() const {
    if (!m_Owner) {
        return AABB();
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return AABB();
    }

    Vec2 worldSize = CalculateRenderSize();
    Vec3 globalPos = node3D->GetGlobalPosition();
    Vec3 scale = node3D->GetGlobalScale();

    worldSize.x *= scale.x;
    worldSize.y *= scale.y;

    Vec3 halfSize(worldSize.x * 0.5f, worldSize.y * 0.5f, 0.1f);

    return AABB(
        globalPos - halfSize,
        globalPos + halfSize
    );
}

RenderLayer Label3D::getRenderLayer() const {

    return RenderLayer::Transparent;
}

math::OBB Label3D::getOrientedBounds() const {
    if (!m_Owner) {
        return math::OBB();
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return math::OBB();
    }

    Vec2 worldSize = CalculateRenderSize();
    Vec3 worldPos = node3D->GetGlobalPosition();
    Quat worldRot = node3D->GetGlobalRotation();
    Vec3 worldScale = node3D->GetGlobalScale();

    BillboardMode billboardMode = GetBillboardMode();
    if (billboardMode != BillboardMode::Disabled) {
        return math::OBB::FromAABB(getWorldBounds());
    }

    Vec3 halfExtents(
        worldSize.x * 0.5f * std::abs(worldScale.x),
        worldSize.y * 0.5f * std::abs(worldScale.y),
        0.05f * std::abs(worldScale.z)
    );

    return math::OBB(worldPos, halfExtents, worldRot);
}

bool Label3D::IntersectRay(const math::Ray& ray, float& outDistance) const {

    math::OBB obb = getOrientedBounds();

    Vec3 localRayOrigin = obb.rotation.Inverse() * (ray.origin - obb.center);
    Vec3 localRayDir = obb.rotation.Inverse() * ray.direction;

    AABB localAABB(Vec3(-obb.extents.x, -obb.extents.y, -obb.extents.z),
                   Vec3(obb.extents.x, obb.extents.y, obb.extents.z));

    math::Ray localRay(localRayOrigin, localRayDir);
    return localRay.IntersectAABB(localAABB, outDistance);
}

bool Label3D::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
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
