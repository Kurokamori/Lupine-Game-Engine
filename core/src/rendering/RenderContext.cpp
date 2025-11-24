#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderView.hpp"
#include "lupine/rendering/RenderCamera.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/MeshBuilder.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/math/MathCommon.hpp"
#include "lupine/math/Plane.hpp"

#include "lupine/logger/Logger.hpp"
#include <cmath>

namespace lupine {

RenderContext::RenderContext(RenderView* view, IGfxDevice* device)
    : m_view(view)
    , m_device(device)
    , m_viewMatrix(Mat4::Identity())
    , m_projectionMatrix(Mat4::Identity())
    , m_viewProjectionMatrix(Mat4::Identity())
{
}

void RenderContext::setDefaultMaterials(
    MaterialHandle coloredMaterial,
    MaterialHandle texturedMaterial,
    MaterialHandle wireframeMaterial,
    MaterialHandle lineMaterial,
    MaterialHandle textMaterial)
{
    m_coloredMaterial = coloredMaterial;
    m_texturedMaterial = texturedMaterial;
    m_texturedDoubleSidedMaterial = MaterialHandle();
    m_wireframeMaterial = wireframeMaterial;
    m_lineMaterial = lineMaterial;
    m_textMaterial = textMaterial;
}

RenderCamera* RenderContext::getCamera() const {
    return m_view ? m_view->getCamera() : nullptr;
}

void RenderContext::updateCameraMatrices() {
    if (!m_view || !m_view->getCamera()) {
        m_viewMatrix = Mat4::Identity();
        m_projectionMatrix = Mat4::Identity();
        m_viewProjectionMatrix = Mat4::Identity();
        return;
    }

    RenderCamera* camera = m_view->getCamera();
    float aspectRatio = m_view->getAspectRatio();

    m_viewMatrix = camera->getViewMatrix();
    m_projectionMatrix = camera->getProjectionMatrix(aspectRatio);
    m_viewProjectionMatrix = m_projectionMatrix * m_viewMatrix;

}

void RenderContext::drawMesh(
    MeshHandle mesh,
    MaterialHandle material,
    const Mat4& transform,
    uint32_t submeshIndex)
{
    DrawItem item;
    item.mesh = mesh;
    item.submeshIndex = submeshIndex;
    item.material = material;
    item.worldTransform = transform;
    item.perObject.worldTransform = transform;
    item.perObject.normalMatrix = transform.Inverse().Transposed();

    Vec3 objectPos = transform.TransformPoint(Vec3(0.0f, 0.0f, 0.0f));
    Vec3 cameraPos = m_viewMatrix.Inverse().TransformPoint(Vec3(0.0f, 0.0f, 0.0f));
    item.distanceToCamera = (objectPos - cameraPos).Length();

    addDrawItem(item);
}

void RenderContext::drawMesh(
    MeshHandle mesh,
    MaterialHandle material,
    const Mat4& transform,
    const MaterialPropertyBlock& overrides,
    uint32_t submeshIndex,
    bool castShadow,
    bool receiveShadow)
{
    DrawItem item;
    item.mesh = mesh;
    item.submeshIndex = submeshIndex;
    item.material = material;
    item.propertyOverrides = overrides;
    item.worldTransform = transform;
    item.perObject.worldTransform = transform;
    item.perObject.normalMatrix = transform.Inverse().Transposed();
    item.castShadow = castShadow;
    item.receiveShadow = receiveShadow;

    Vec3 objectPos = transform.TransformPoint(Vec3(0.0f, 0.0f, 0.0f));
    Vec3 cameraPos = m_viewMatrix.Inverse().TransformPoint(Vec3(0.0f, 0.0f, 0.0f));
    item.distanceToCamera = (objectPos - cameraPos).Length();

    addDrawItem(item);
}

void RenderContext::drawSprite(const SpriteDrawData& sprite) {
    if (!m_device) {

        return;
    }

    MeshHandle quadMesh = getOrCreateQuadMesh();
    if (!quadMesh.isValid()) {

        return;
    }

    Mat4 scale = Mat4::Scale(Vec3(sprite.size.x, sprite.size.y, 1.0f));
    Mat4 rotation = Mat4::Rotate(sprite.rotation, Vec3(0.0f, 0.0f, 1.0f));
    Mat4 translation = Mat4::Translate(Vec3(sprite.position.x, sprite.position.y, 0.0f));

    Vec2 pivotOffset = (sprite.pivot - Vec2(0.5f, 0.5f)) * sprite.size;
    Mat4 pivotTranslation = Mat4::Translate(Vec3(-pivotOffset.x, -pivotOffset.y, 0.0f));

    Mat4 transform = translation * rotation * pivotTranslation * scale;

    Vec3 corners[4] = {
        Vec3(-0.5f, -0.5f, 0.0f),
        Vec3( 0.5f, -0.5f, 0.0f),
        Vec3( 0.5f,  0.5f, 0.0f),
        Vec3(-0.5f,  0.5f, 0.0f)
    };

    MaterialPropertyBlock overrides;
    overrides.setTexture("u_Texture", sprite.texture);
    overrides.setColor("u_TintColor", sprite.tint);
    overrides.setVec4("u_UVRect", Vec4(sprite.uvMin.x, sprite.uvMin.y, sprite.uvMax.x, sprite.uvMax.y));
    overrides.setBool("u_UseTexture", true);

    drawMesh(quadMesh, m_texturedMaterial, transform, overrides);
}

void RenderContext::drawPrimitive(const PrimitiveDrawData& primitive) {
    if (!m_device) {
        return;
    }

    MeshHandle mesh;
    Mat4 transform = Mat4::Translate(primitive.position);
    bool needsCleanup = false;

    switch (primitive.type) {
        case PrimitiveType::Line: {

            MeshData lineData;

            Vertex v0, v1;
            v0.position = Vec3(0, 0, 0);
            v0.normal = Vec3(0, 0, 1);
            v0.texCoord = Vec2(0, 0);
            v0.color = Vec4(primitive.color.r, primitive.color.g, primitive.color.b, primitive.color.a);

            v1.position = Vec3(primitive.size.x, 0, 0);
            v1.normal = Vec3(0, 0, 1);
            v1.texCoord = Vec2(1, 0);
            v1.color = Vec4(primitive.color.r, primitive.color.g, primitive.color.b, primitive.color.a);

            lineData.vertices = {v0, v1};
            lineData.indices = {0, 1};
            lineData.calculateBounds();

            mesh = m_device->createMesh(lineData);
            needsCleanup = true;
            break;
        }
        case PrimitiveType::Triangle: {

            MeshData triangleData;

            Vertex v0, v1, v2;
            v0.position = Vec3(0, 0, 0);
            v0.normal = Vec3(0, 0, 1);
            v0.texCoord = Vec2(0.5f, 0);
            v0.color = Vec4(primitive.color.r, primitive.color.g, primitive.color.b, primitive.color.a);

            v1.position = Vec3(-0.5f, 1, 0);
            v1.normal = Vec3(0, 0, 1);
            v1.texCoord = Vec2(0, 1);
            v1.color = Vec4(primitive.color.r, primitive.color.g, primitive.color.b, primitive.color.a);

            v2.position = Vec3(0.5f, 1, 0);
            v2.normal = Vec3(0, 0, 1);
            v2.texCoord = Vec2(1, 1);
            v2.color = Vec4(primitive.color.r, primitive.color.g, primitive.color.b, primitive.color.a);

            triangleData.vertices = {v0, v1, v2};
            triangleData.indices = {0, 1, 2};
            triangleData.calculateBounds();

            mesh = m_device->createMesh(triangleData);
            transform = transform * Mat4::Scale(primitive.size);
            needsCleanup = true;
            break;
        }
        case PrimitiveType::Quad: {
            mesh = getOrCreateQuadMesh();
            transform = transform * Mat4::Scale(primitive.size);
            break;
        }
        case PrimitiveType::Circle: {
            mesh = getOrCreateCircleMesh();
            transform = transform * Mat4::Scale(Vec3(primitive.size.x, primitive.size.y, 1.0f));
            break;
        }
        case PrimitiveType::Box: {
            mesh = getOrCreateCubeMesh();
            transform = transform * Mat4::Scale(primitive.size);
            break;
        }
        case PrimitiveType::Sphere: {
            mesh = getOrCreateSphereMesh();
            transform = transform * Mat4::Scale(primitive.size);
            break;
        }
    }

    if (!mesh.isValid()) {
        return;
    }

    MaterialPropertyBlock overrides;
    overrides.setColor("u_Color", primitive.color);

    MaterialHandle material = primitive.wireframe ? m_wireframeMaterial : m_coloredMaterial;

    drawMesh(mesh, material, transform, overrides);

    if (needsCleanup) {
        m_device->destroyMesh(mesh);
    }
}

void RenderContext::drawText(const TextDrawData& text) {
    if (!m_device || !text.fontAtlas.isValid()) {

        return;
    }

    const FontAtlas* fontAtlas = m_device->getFontAtlas(text.fontAtlas);
    if (!fontAtlas) {

        return;
    }

    float scale = text.fontSize / fontAtlas->fontSize;

    MeshData textMeshData;
    Vec2 cursor = text.position;

    uint32_t vertexOffset = 0;
    for (size_t i = 0; i < text.text.length(); ++i) {
        char c = text.text[i];
        uint32_t codepoint = static_cast<uint32_t>(c);

        if (c == '\n') {
            cursor.x = text.position.x;
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

        Vec2 glyphPos;
        glyphPos.x = cursor.x + glyph->bearing.x * scale;
        glyphPos.y = cursor.y - glyph->bearing.y * scale;

        Vec2 glyphSize = glyph->size * scale;

        if (i == 0) {

        }

        Vertex v0, v1, v2, v3;

        float textZ = 0.1f;

        v0.position = Vec3(glyphPos.x, glyphPos.y - glyphSize.y, textZ);
        v0.normal = Vec3(0, 0, 1);
        v0.texCoord = Vec2(glyph->uvMin.x, glyph->uvMax.y);
        v0.color = Vec4(text.color.r, text.color.g, text.color.b, text.color.a);

        v1.position = Vec3(glyphPos.x + glyphSize.x, glyphPos.y - glyphSize.y, textZ);
        v1.normal = Vec3(0, 0, 1);
        v1.texCoord = Vec2(glyph->uvMax.x, glyph->uvMax.y);
        v1.color = Vec4(text.color.r, text.color.g, text.color.b, text.color.a);

        v2.position = Vec3(glyphPos.x + glyphSize.x, glyphPos.y, textZ);
        v2.normal = Vec3(0, 0, 1);
        v2.texCoord = Vec2(glyph->uvMax.x, glyph->uvMin.y);
        v2.color = Vec4(text.color.r, text.color.g, text.color.b, text.color.a);

        v3.position = Vec3(glyphPos.x, glyphPos.y, textZ);
        v3.normal = Vec3(0, 0, 1);
        v3.texCoord = Vec2(glyph->uvMin.x, glyph->uvMin.y);
        v3.color = Vec4(text.color.r, text.color.g, text.color.b, text.color.a);

        if (i == 0) {

        }

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

        return;
    }

    textMeshData.calculateBounds();

    MeshHandle textMesh = m_device->createMesh(textMeshData);
    if (!textMesh.isValid()) {

        return;
    }

    MaterialPropertyBlock overrides;
    overrides.setTexture("u_FontAtlas", fontAtlas->texture);
    overrides.setColor("u_TextColor", text.color);

    Mat4 transform = Mat4::Identity();
    drawMesh(textMesh, m_textMaterial, transform, overrides);

    m_device->destroyMesh(textMesh);
}

void RenderContext::drawQuad(
    const Vec3& position,
    const Vec2& size,
    const Color& color,
    TextureHandle texture)
{
    if (!m_device) {
        return;
    }

    MeshHandle quadMesh = getOrCreateQuadMesh();
    if (!quadMesh.isValid()) {
        return;
    }

    Mat4 transform = Mat4::Translate(position) * Mat4::Scale(Vec3(size.x, size.y, 1.0f));

    MaterialPropertyBlock overrides;
    overrides.setColor("u_Color", color);
    if (texture.isValid()) {
        overrides.setTexture("u_MainTexture", texture);
    }

    MaterialHandle material = texture.isValid() ? m_texturedMaterial : m_coloredMaterial;

    drawMesh(quadMesh, material, transform, overrides);
}

void RenderContext::drawRoundedRect(
    const Vec2& position,
    const Vec2& size,
    float cornerRadius,
    const Color& color,
    int blendMode)
{

    drawRoundedRect(position, size, Vec4(cornerRadius, cornerRadius, cornerRadius, cornerRadius), color, blendMode);
}

void RenderContext::drawRoundedRect(
    const Vec2& position,
    const Vec2& size,
    const Vec4& cornerRadius,
    const Color& color,
    int blendMode)
{
    if (!m_device) {
        return;
    }

    MaterialHandle material = m_roundedRectMaterial;
    switch (blendMode) {
        case 1: material = m_roundedRectAdditiveMaterial; break;
        case 2: material = m_roundedRectMultiplyMaterial; break;
        case 3: material = m_roundedRectOpaqueMaterial; break;
        default: break;
    }

    MeshHandle quadMesh = getOrCreateQuadMesh();

    Mat4 transform = Mat4::Identity();
    transform = Mat4::Translate(Vec3(position.x + size.x * 0.5f, position.y + size.y * 0.5f, 0.0f)) * Mat4::Scale(Vec3(size.x, size.y, 1.0f));

    MaterialPropertyBlock overrides;
    overrides.setColor("u_TintColor", color);
    overrides.setVec4("u_CornerRadius", cornerRadius);
    overrides.setVec2("u_Size", size);
    overrides.setBool("u_UseTexture", false);
    overrides.setVec2("u_UVMin", Vec2(0.0f, 0.0f));
    overrides.setVec2("u_UVMax", Vec2(1.0f, 1.0f));

    drawMesh(quadMesh, material, transform, overrides);
}

void RenderContext::drawRoundedRect(
    const Vec2& position,
    const Vec2& size,
    const Vec4& cornerRadius,
    const Color& color,
    TextureHandle texture,
    const Vec2& uvMin,
    const Vec2& uvMax,
    int blendMode)
{
    if (!m_device) {
        return;
    }

    MaterialHandle material = m_roundedRectMaterial;
    switch (blendMode) {
        case 1: material = m_roundedRectAdditiveMaterial; break;
        case 2: material = m_roundedRectMultiplyMaterial; break;
        case 3: material = m_roundedRectOpaqueMaterial; break;
        case 4: material = m_roundedRectOverlayMaterial; break;
        default: break;
    }

    MeshHandle quadMesh = getOrCreateQuadMesh();

    Mat4 transform = Mat4::Identity();
    transform = Mat4::Translate(Vec3(position.x + size.x * 0.5f, position.y + size.y * 0.5f, 0.0f)) * Mat4::Scale(Vec3(size.x, size.y, 1.0f));

    MaterialPropertyBlock overrides;
    overrides.setColor("u_TintColor", color);
    overrides.setVec4("u_CornerRadius", cornerRadius);
    overrides.setVec2("u_Size", size);
    overrides.setBool("u_UseTexture", true);
    overrides.setTexture("u_Texture", texture);
    overrides.setVec2("u_UVMin", uvMin);
    overrides.setVec2("u_UVMax", uvMax);

    drawMesh(quadMesh, material, transform, overrides);
}

void RenderContext::drawRoundedRect(
    const Vec2& position,
    const Vec2& size,
    const Vec4& cornerRadius,
    const Color& color,
    float rotation,
    int blendMode)
{
    if (!m_device) {
        return;
    }

    MaterialHandle material = m_roundedRectMaterial;
    switch (blendMode) {
        case 1: material = m_roundedRectAdditiveMaterial; break;
        case 2: material = m_roundedRectMultiplyMaterial; break;
        case 3: material = m_roundedRectOpaqueMaterial; break;
        default: break;
    }

    MeshHandle quadMesh = getOrCreateQuadMesh();

    Mat4 transform = Mat4::Identity();

    transform = Mat4::Translate(Vec3(position.x, position.y, 0.0f)) *
                Mat4::Rotate(rotation, Vec3::UnitZ()) *
                Mat4::Scale(Vec3(size.x, size.y, 1.0f));

    MaterialPropertyBlock overrides;
    overrides.setColor("u_TintColor", color);
    overrides.setVec4("u_CornerRadius", cornerRadius);
    overrides.setVec2("u_Size", size);
    overrides.setBool("u_UseTexture", false);
    overrides.setVec2("u_UVMin", Vec2(0.0f, 0.0f));
    overrides.setVec2("u_UVMax", Vec2(1.0f, 1.0f));

    drawMesh(quadMesh, material, transform, overrides);
}

void RenderContext::drawRoundedRect(
    const Vec2& position,
    const Vec2& size,
    const Vec4& cornerRadius,
    const Color& color,
    TextureHandle texture,
    float rotation,
    const Vec2& uvMin,
    const Vec2& uvMax,
    int blendMode)
{
    if (!m_device) {
        return;
    }

    MaterialHandle material = m_roundedRectMaterial;
    switch (blendMode) {
        case 1: material = m_roundedRectAdditiveMaterial; break;
        case 2: material = m_roundedRectMultiplyMaterial; break;
        case 3: material = m_roundedRectOpaqueMaterial; break;
        case 4: material = m_roundedRectOverlayMaterial; break;
        default: break;
    }

    MeshHandle quadMesh = getOrCreateQuadMesh();

    Mat4 transform = Mat4::Identity();

    transform = Mat4::Translate(Vec3(position.x, position.y, 0.0f)) *
                Mat4::Rotate(rotation, Vec3::UnitZ()) *
                Mat4::Scale(Vec3(size.x, size.y, 1.0f));

    MaterialPropertyBlock overrides;
    overrides.setColor("u_TintColor", color);
    overrides.setVec4("u_CornerRadius", cornerRadius);
    overrides.setVec2("u_Size", size);
    overrides.setBool("u_UseTexture", true);
    overrides.setTexture("u_Texture", texture);
    overrides.setVec2("u_UVMin", uvMin);
    overrides.setVec2("u_UVMax", uvMax);

    drawMesh(quadMesh, material, transform, overrides);
}

void RenderContext::drawRoundedRectBorder(
    const Vec2& outerPosition,
    const Vec2& outerSize,
    const Vec4& outerCornerRadius,
    const Vec4& borderWidth,
    const Color& color)
{

    drawRoundedRectBorder(outerPosition, outerSize, outerCornerRadius, borderWidth, color, color, color, color);
}

void RenderContext::drawRoundedRectBorder(
    const Vec2& outerPosition,
    const Vec2& outerSize,
    const Vec4& outerCornerRadius,
    const Vec4& borderWidth,
    const Color& color,
    float rotation)
{
    if (!m_device || std::abs(rotation) < 0.0001f) {

        drawRoundedRectBorder(outerPosition, outerSize, outerCornerRadius, borderWidth, color);
        return;
    }

    float topWidth = borderWidth.x;
    float rightWidth = borderWidth.y;
    float bottomWidth = borderWidth.z;
    float leftWidth = borderWidth.w;

    Vec2 center = Vec2(outerPosition.x, outerPosition.y);

    drawRoundedRect(center, outerSize, outerCornerRadius, color, rotation, 0);
}

void RenderContext::drawRoundedRectBorder(
    const Vec2& outerPosition,
    const Vec2& outerSize,
    const Vec4& outerCornerRadius,
    const Vec4& borderWidth,
    const Color& colorTop,
    const Color& colorRight,
    const Color& colorBottom,
    const Color& colorLeft)
{
    if (!m_device) {
        return;
    }

    float topWidth = borderWidth.x;
    float rightWidth = borderWidth.y;
    float bottomWidth = borderWidth.z;
    float leftWidth = borderWidth.w;

    bool uniformColor = (colorTop == colorRight && colorTop == colorBottom && colorTop == colorLeft);

    if (uniformColor) {

        drawRoundedRect(outerPosition, outerSize, outerCornerRadius, colorTop, 0);

    } else {

        float tlRadius = outerCornerRadius.x;
        float trRadius = outerCornerRadius.y;
        float brRadius = outerCornerRadius.z;
        float blRadius = outerCornerRadius.w;

        if (topWidth > 0.0f) {
            Vec2 topPos = outerPosition;
            Vec2 topSize = Vec2(outerSize.x, topWidth);
            Vec4 topCorners = Vec4(tlRadius, trRadius, 0.0f, 0.0f);
            drawRoundedRect(topPos, topSize, topCorners, colorTop, 0);
        }

        if (bottomWidth > 0.0f) {
            Vec2 bottomPos = Vec2(outerPosition.x, outerPosition.y + outerSize.y - bottomWidth);
            Vec2 bottomSize = Vec2(outerSize.x, bottomWidth);
            Vec4 bottomCorners = Vec4(0.0f, 0.0f, brRadius, blRadius);
            drawRoundedRect(bottomPos, bottomSize, bottomCorners, colorBottom, 0);
        }

        if (leftWidth > 0.0f) {
            Vec2 leftPos = Vec2(outerPosition.x, outerPosition.y + topWidth);
            Vec2 leftSize = Vec2(leftWidth, outerSize.y - topWidth - bottomWidth);
            Vec4 leftCorners = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            drawRoundedRect(leftPos, leftSize, leftCorners, colorLeft, 0);
        }

        if (rightWidth > 0.0f) {
            Vec2 rightPos = Vec2(outerPosition.x + outerSize.x - rightWidth, outerPosition.y + topWidth);
            Vec2 rightSize = Vec2(rightWidth, outerSize.y - topWidth - bottomWidth);
            Vec4 rightCorners = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            drawRoundedRect(rightPos, rightSize, rightCorners, colorRight, 0);
        }
    }
}

void RenderContext::drawLine(
    const Vec3& start,
    const Vec3& end,
    const Color& color,
    float thickness)
{
    if (!m_device) {
        return;
    }

    Vec3 direction = (end - start).Normalized();
    Vec3 perpendicular;

    if (std::abs(direction.z) < 0.99f) {
        perpendicular = direction.Cross(Vec3(0, 0, 1)).Normalized();
    } else {
        perpendicular = direction.Cross(Vec3(0, 1, 0)).Normalized();
    }

    Vec3 offset = perpendicular * (thickness * 0.5f);

    MeshData lineData;

    Vertex v0, v1, v2, v3;
    v0.position = start - offset;
    v0.normal = Vec3(0, 0, 1);
    v0.texCoord = Vec2(0, 0);
    v0.color = Vec4(color.r, color.g, color.b, color.a);

    v1.position = start + offset;
    v1.normal = Vec3(0, 0, 1);
    v1.texCoord = Vec2(1, 0);
    v1.color = Vec4(color.r, color.g, color.b, color.a);

    v2.position = end + offset;
    v2.normal = Vec3(0, 0, 1);
    v2.texCoord = Vec2(1, 1);
    v2.color = Vec4(color.r, color.g, color.b, color.a);

    v3.position = end - offset;
    v3.normal = Vec3(0, 0, 1);
    v3.texCoord = Vec2(0, 1);
    v3.color = Vec4(color.r, color.g, color.b, color.a);

    lineData.vertices = {v0, v1, v2, v3};
    lineData.indices = {0, 1, 2, 0, 2, 3};
    lineData.calculateBounds();

    MeshHandle mesh = m_device->createMesh(lineData);

    MaterialPropertyBlock overrides;
    overrides.setColor("u_Color", color);

    MaterialHandle material = (m_currentSpatialType == SpatialType::World2D || m_currentSpatialType == SpatialType::Canvas)
                              ? m_line2DMaterial : m_lineMaterial;
    drawMesh(mesh, material, Mat4::Identity(), overrides);

    m_device->destroyMesh(mesh);
}

void RenderContext::drawCircle(
    const Vec3& center,
    float radius,
    const Color& color,
    bool filled)
{
    if (!m_device) {
        return;
    }

    MeshHandle mesh;
    MaterialHandle material;

    if (filled) {

        mesh = getOrCreateCircleMesh();
        if (!mesh.isValid()) {
            return;
        }

        material = (m_currentSpatialType == SpatialType::World2D || m_currentSpatialType == SpatialType::Canvas)
                   ? m_colored2DMaterial : m_coloredMaterial;
    } else {

        MeshData wireframeData;
        const int segments = 32;

        for (int i = 0; i <= segments; ++i) {
            float angle = static_cast<float>(i) / segments * 2.0f * 3.14159265359f;
            float x = std::cos(angle);
            float y = std::sin(angle);

            Vertex v;
            v.position = Vec3(x, y, 0.0f);
            v.normal = Vec3(0, 0, 1);
            v.texCoord = Vec2(0, 0);
            v.color = Vec4(color.r, color.g, color.b, color.a);
            wireframeData.vertices.push_back(v);

            if (i > 0) {
                wireframeData.indices.push_back(i - 1);
                wireframeData.indices.push_back(i);
            }
        }

        wireframeData.calculateBounds();
        mesh = m_device->createMesh(wireframeData);

        material = (m_currentSpatialType == SpatialType::World2D || m_currentSpatialType == SpatialType::Canvas)
                   ? m_line2DMaterial : m_lineMaterial;
    }

    Mat4 transform = Mat4::Translate(center) * Mat4::Scale(Vec3(radius, radius, radius));

    MaterialPropertyBlock overrides;
    overrides.setColor("u_Color", color);

    drawMesh(mesh, material, transform, overrides);

    if (!filled && mesh.isValid()) {
        m_device->destroyMesh(mesh);
    }
}

void RenderContext::drawBox(
    const Vec3& center,
    const Vec3& size,
    const Color& color,
    bool wireframe)
{
    if (!m_device) {
        return;
    }

    MeshHandle cubeMesh = getOrCreateCubeMesh();
    if (!cubeMesh.isValid()) {
        return;
    }

    Mat4 transform = Mat4::Translate(center) * Mat4::Scale(size);

    MaterialPropertyBlock overrides;
    overrides.setColor("u_Color", color);

    MaterialHandle material = wireframe ? m_wireframeMaterial : m_coloredMaterial;

    drawMesh(cubeMesh, material, transform, overrides);
}

bool RenderContext::isVisible(const AABB& bounds) const {

    /*
    const Mat4& vp = m_viewProjection;
    const glm::mat4& m = vp.GetGLM();

    Plane left(
        Vec3(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0]),
        m[3][3] + m[3][0]
    );

    Plane right(
        Vec3(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0]),
        m[3][3] - m[3][0]
    );

    Plane bottom(
        Vec3(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1]),
        m[3][3] + m[3][1]
    );

    Plane top(
        Vec3(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1]),
        m[3][3] - m[3][1]
    );

    Plane near(
        Vec3(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2]),
        m[3][3] + m[3][2]
    );

    Plane far(
        Vec3(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2]),
        m[3][3] - m[3][2]
    );

    Plane planes[6] = { left, right, bottom, top, near, far };

    for (int i = 0; i < 6; ++i) {

        Vec3 positiveVertex(
            planes[i].normal.x > 0.0f ? bounds.max.x : bounds.min.x,
            planes[i].normal.y > 0.0f ? bounds.max.y : bounds.min.y,
            planes[i].normal.z > 0.0f ? bounds.max.z : bounds.min.z
        );

        if (planes[i].DistanceToPoint(positiveVertex) < 0.0f) {
            return false;
        }
    }

    return true;
    */

    const Mat4& m = m_viewProjectionMatrix;

    auto makePlane = [](float a, float b, float c, float d) -> Plane {
        Vec3 normal(a, b, c);
        float length = normal.Length();
        if (length > 0.0001f) {
            normal = normal / length;
            float distance = -d / length;
            return Plane(normal, distance);
        }
        return Plane(Vec3(0, 1, 0), 0.0f);
    };

    Plane left = makePlane(
        m[0][3] + m[0][0],
        m[1][3] + m[1][0],
        m[2][3] + m[2][0],
        m[3][3] + m[3][0]
    );

    Plane right = makePlane(
        m[0][3] - m[0][0],
        m[1][3] - m[1][0],
        m[2][3] - m[2][0],
        m[3][3] - m[3][0]
    );

    Plane bottom = makePlane(
        m[0][3] + m[0][1],
        m[1][3] + m[1][1],
        m[2][3] + m[2][1],
        m[3][3] + m[3][1]
    );

    Plane top = makePlane(
        m[0][3] - m[0][1],
        m[1][3] - m[1][1],
        m[2][3] - m[2][1],
        m[3][3] - m[3][1]
    );

    Plane nearPlane = makePlane(
        m[0][3] + m[0][2],
        m[1][3] + m[1][2],
        m[2][3] + m[2][2],
        m[3][3] + m[3][2]
    );

    Plane farPlane = makePlane(
        m[0][3] - m[0][2],
        m[1][3] - m[1][2],
        m[2][3] - m[2][2],
        m[3][3] - m[3][2]
    );

    Plane planes[6] = { left, right, bottom, top, nearPlane, farPlane };

    for (int i = 0; i < 6; ++i) {

        Vec3 positiveVertex(
            planes[i].normal.x > 0.0f ? bounds.max.x : bounds.min.x,
            planes[i].normal.y > 0.0f ? bounds.max.y : bounds.min.y,
            planes[i].normal.z > 0.0f ? bounds.max.z : bounds.min.z
        );

        if (planes[i].DistanceToPoint(positiveVertex) < 0.0f) {
            return false;
        }
    }

    return true;
}

void RenderContext::clear() {
    m_drawItems.clear();
}

void RenderContext::addDrawItem(const DrawItem& item) {
    DrawItem modifiedItem = item;
    modifiedItem.spatialType = m_currentSpatialType;

    // For 2D and Canvas rendering, use Z-index as sort key for proper layering
    if (m_currentSpatialType == SpatialType::World2D || m_currentSpatialType == SpatialType::Canvas) {
        modifiedItem.sortKey = static_cast<float>(m_currentZIndex);
    }

    m_drawItems.push_back(modifiedItem);
}

MeshHandle RenderContext::getOrCreateQuadMesh() {
    if (!m_cachedQuadMesh.isValid() && m_device) {
        MeshData quadData = MeshBuilder::createQuad(1.0f, 1.0f);

        m_cachedQuadMesh = m_device->createMesh(quadData);

    }
    return m_cachedQuadMesh;
}

MeshHandle RenderContext::getOrCreateCubeMesh() {
    if (!m_cachedCubeMesh.isValid() && m_device) {
        MeshData cubeData = MeshBuilder::createCube(1.0f);
        m_cachedCubeMesh = m_device->createMesh(cubeData);
    }
    return m_cachedCubeMesh;
}

MeshHandle RenderContext::getOrCreateSphereMesh() {
    if (!m_cachedSphereMesh.isValid() && m_device) {
        MeshData sphereData = MeshBuilder::createSphere(1.0f, 16, 16);
        m_cachedSphereMesh = m_device->createMesh(sphereData);
    }
    return m_cachedSphereMesh;
}

MeshHandle RenderContext::getOrCreateCircleMesh() {
    if (!m_cachedCircleMesh.isValid() && m_device) {
        MeshData circleData = MeshBuilder::createCircle(1.0f, 32);
        m_cachedCircleMesh = m_device->createMesh(circleData);
    }
    return m_cachedCircleMesh;
}

}

