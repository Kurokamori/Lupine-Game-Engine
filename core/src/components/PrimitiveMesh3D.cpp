#include "lupine/components/PrimitiveMesh3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/rendering/MeshBuilder.hpp"
#include "lupine/rendering/Material.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

PrimitiveMesh3D::PrimitiveMesh3D()
    : Component("PrimitiveMesh3D")
    , m_MeshHandle()
    , m_MeshNeedsRegeneration(true)
    , m_TexturesNeedUpload(false)
    , m_CachedShape(static_cast<PrimitiveShape>(-1))
    , m_CachedSize(-1.0f)
    , m_CachedHeight(-1.0f)
    , m_CachedDetail(-1)
    , m_CachedMinorRadius(-1.0f)
{
}

PrimitiveMesh3D::PrimitiveMesh3D(const std::string& name)
    : Component(name)
    , m_MeshHandle()
    , m_MeshNeedsRegeneration(true)
    , m_TexturesNeedUpload(false)
    , m_CachedShape(static_cast<PrimitiveShape>(-1))
    , m_CachedSize(-1.0f)
    , m_CachedHeight(-1.0f)
    , m_CachedDetail(-1)
    , m_CachedMinorRadius(-1.0f)
{
}

PrimitiveMesh3D::~PrimitiveMesh3D() {

}

void PrimitiveMesh3D::DefineProperties() {

    DefineProperty(PROPERTY_ENUM_GROUP(shape, 0, "Geometry", Cube, Sphere, Cylinder, Cone, Pyramid, Torus, Capsule));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(size, 1.0f, 0.01f, 100.0f, 0.1f, "Geometry"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(height, 2.0f, 0.01f, 100.0f, 0.1f, "Geometry"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(detail, 32, 3, 128, 1, "Geometry"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(minorRadius, 0.2f, 0.01f, 10.0f, 0.05f, "Geometry"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(color, Color, Color::White(), "Geometry"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(castShadow, Bool, true, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(receiveShadow, Bool, true, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(doubleSided, Bool, false, "Rendering"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(materialOverrideEnabled, Bool, false, "Material Override"));
    // Shader type: PBR=0, Toon=1, Unlit=2, Standard3D=3, Custom=4
    DefineProperty(PROPERTY_DEFAULT_GROUP(shaderType, String, std::string("PBR"), "Material Override"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(albedoColor, Color, Color::White(), "Material Override"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(albedoTexture, Int, 0, "Material Override"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(metallic, 0.0f, 0.0f, 1.0f, 0.01f, "Material Override"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(roughness, 0.5f, 0.0f, 1.0f, 0.01f, "Material Override"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(metallicRoughnessTexture, Int, 0, "Material Override"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalTexture, Int, 0, "Material Override"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(normalScale, 1.0f, 0.0f, 2.0f, 0.1f, "Material Override"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(emissiveColor, Color, Color::Black(), "Material Override"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(emissiveTexture, Int, 0, "Material Override"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(emissiveStrength, 1.0f, 0.0f, 10.0f, 0.1f, "Material Override"));

    // Toon shader parameters
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(shadowBands, 3.0f, 1.0f, 10.0f, 1.0f, "Material Override"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(shadowThreshold, 0.5f, 0.0f, 1.0f, 0.01f, "Material Override"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(shadowSoftness, 0.02f, 0.0f, 0.5f, 0.01f, "Material Override"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(specularBands, 2.0f, 1.0f, 10.0f, 1.0f, "Material Override"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(specularPower, 32.0f, 1.0f, 128.0f, 1.0f, "Material Override"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(rimIntensity, 0.0f, 0.0f, 2.0f, 0.1f, "Material Override"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(rimPower, 3.0f, 0.5f, 10.0f, 0.5f, "Material Override"));

    // Custom .lsh shader (translated at runtime, honors #render_mode; takes precedence
    // over shaderType when set).
    DefineProperty(PROPERTY_FILE_GROUP(customLshShaderPath, std::string(""), "*.lsh", "Material Override"));
}

void PrimitiveMesh3D::OnAwake() {

    m_MeshNeedsRegeneration = true;
}

void PrimitiveMesh3D::OnReady() {

}

void PrimitiveMesh3D::OnRender() {

}

PrimitiveShape PrimitiveMesh3D::GetShape() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("shape");
    if (prop) {
        int shapeIndex = prop->GetValue<int>();
        return static_cast<PrimitiveShape>(shapeIndex);
    }
    return PrimitiveShape::Cube;
}

void PrimitiveMesh3D::SetShape(PrimitiveShape shape) {
    SetPropertyValue<int>("shape", static_cast<int>(shape));
    m_MeshNeedsRegeneration = true;
}

Color PrimitiveMesh3D::GetColor() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("color");
    if (prop) {
        return prop->GetValue<Color>();
    }
    return Color::White();
}

void PrimitiveMesh3D::SetColor(const Color& color) {
    SetPropertyValue<Color>("color", color);
}

float PrimitiveMesh3D::GetSize() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("size");
    if (prop) {
        return prop->GetValue<float>();
    }
    return 1.0f;
}

void PrimitiveMesh3D::SetSize(float size) {
    SetPropertyValue<float>("size", size);
    m_MeshNeedsRegeneration = true;
}

float PrimitiveMesh3D::GetHeight() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("height");
    if (prop) {
        return prop->GetValue<float>();
    }
    return 2.0f;
}

void PrimitiveMesh3D::SetHeight(float height) {
    SetPropertyValue<float>("height", height);
    m_MeshNeedsRegeneration = true;
}

int PrimitiveMesh3D::GetDetail() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("detail");
    if (prop) {
        return prop->GetValue<int>();
    }
    return 32;
}

void PrimitiveMesh3D::SetDetail(int detail) {
    SetPropertyValue<int>("detail", detail);
    m_MeshNeedsRegeneration = true;
}

float PrimitiveMesh3D::GetMinorRadius() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("minorRadius");
    if (prop) {
        return prop->GetValue<float>();
    }
    return 0.2f;
}

void PrimitiveMesh3D::SetMinorRadius(float radius) {
    SetPropertyValue<float>("minorRadius", radius);
    m_MeshNeedsRegeneration = true;
}

bool PrimitiveMesh3D::GetCastShadow() const {
    return GetPropertyValue<bool>("castShadow");
}

void PrimitiveMesh3D::SetCastShadow(bool castShadow) {
    SetPropertyValue<bool>("castShadow", castShadow);
}

bool PrimitiveMesh3D::GetReceiveShadow() const {
    return GetPropertyValue<bool>("receiveShadow");
}

void PrimitiveMesh3D::SetReceiveShadow(bool receiveShadow) {
    SetPropertyValue<bool>("receiveShadow", receiveShadow);
}

bool PrimitiveMesh3D::GetDoubleSided() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("doubleSided");
    if (prop) {
        return prop->GetValue<bool>();
    }
    return false;
}

void PrimitiveMesh3D::SetDoubleSided(bool doubleSided) {
    SetPropertyValue<bool>("doubleSided", doubleSided);

}

bool PrimitiveMesh3D::HasShapeParametersChanged() const {
    return m_CachedShape != GetShape() ||
           m_CachedSize != GetSize() ||
           m_CachedHeight != GetHeight() ||
           m_CachedDetail != GetDetail() ||
           m_CachedMinorRadius != GetMinorRadius();
}

void PrimitiveMesh3D::UpdateCachedParameters() const {
    m_CachedShape = GetShape();
    m_CachedSize = GetSize();
    m_CachedHeight = GetHeight();
    m_CachedDetail = GetDetail();
    m_CachedMinorRadius = GetMinorRadius();
}

MeshData PrimitiveMesh3D::GenerateShapeMesh() const {
    PrimitiveShape shape = GetShape();
    float size = GetSize();
    float height = GetHeight();
    int detail = GetDetail();
    float minorRadius = GetMinorRadius();

    detail = std::max(3, std::min(128, detail));

    switch (shape) {
        case PrimitiveShape::Cube:
            return MeshBuilder::createCube(size);

        case PrimitiveShape::Sphere:
            return MeshBuilder::createSphere(size, detail, detail / 2);

        case PrimitiveShape::Cylinder:
            return MeshBuilder::createCylinder(size, height, detail);

        case PrimitiveShape::Cone:
            return MeshBuilder::createCone(size, height, detail);

        case PrimitiveShape::Pyramid:
            return MeshBuilder::createPyramid(size, height);

        case PrimitiveShape::Torus:
            return MeshBuilder::createTorus(size, minorRadius, detail, detail / 2);

        case PrimitiveShape::Capsule:
            return MeshBuilder::createCapsule(size, height, detail, detail / 2);

        default:
            return MeshBuilder::createCube(size);
    }
}

void PrimitiveMesh3D::RegenerateMesh(RenderContext& ctx) {
    if (!m_MeshNeedsRegeneration) {
        return;
    }

    if (m_MeshHandle.isValid() && ctx.getDevice()) {
        ctx.getDevice()->destroyMesh(m_MeshHandle);
        m_MeshHandle = MeshHandle();
    }

    MeshData meshData = GenerateShapeMesh();

    Vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    for (Vertex& vertex : meshData.vertices) {
        vertex.color = white;
    }

    if (ctx.getDevice()) {
        m_MeshHandle = ctx.getDevice()->createMesh(meshData);
    }

    UpdateCachedParameters();

    m_MeshNeedsRegeneration = false;
}

void PrimitiveMesh3D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return;
    }

    if (HasShapeParametersChanged()) {
        m_MeshNeedsRegeneration = true;
    }

    if (m_MeshNeedsRegeneration || !m_MeshHandle.isValid()) {
        RegenerateMesh(ctx);
    }

    if (!m_MeshHandle.isValid()) {
        return;
    }

    Mat4 transform = node3D->GetGlobalTransformMatrix();

    bool useMaterialOverride = HasMaterialOverride();

    MaterialHandle material;
    MaterialPropertyBlock overrides;
    ShaderType shaderType = GetShaderType();

    if (useMaterialOverride) {
        // A custom .lsh shader takes precedence: translated at runtime with the 3D-mesh
        // vertex layout; its optional #render_mode drives blend/cull/depth.
        const std::string lshPath = GetCustomLshShaderPath();
        if (!lshPath.empty()) {
            material = ctx.getOrCreateLshMaterial(lshPath, 3 /*opaque default*/,
                                                  LshMaterialLayout::Mesh3D);
        }

        // Select material based on shader type using the material registry
        // Handle custom shaders
        if (!material.isValid() && shaderType == ShaderType::Custom) {
            std::string vertPath = GetCustomVertShaderPath();
            std::string fragPath = GetCustomFragShaderPath();
            if (!vertPath.empty() || !fragPath.empty()) {
                material = ctx.getOrCreateCustomMaterial(vertPath, fragPath, false);
            }
        }

        // If not custom or custom failed, use registry
        if (!material.isValid()) {
            material = ctx.getMaterial(shaderType, false);  // false = not skeletal
        }

        if (!material.isValid()) {
            material = GetDoubleSided()
                ? ctx.getDefaultColoredDoubleSidedMaterial()
                : ctx.getDefaultColoredMaterial();
            overrides.setColor("u_TintColor", GetColor());
            ctx.drawMesh(m_MeshHandle, material, transform, overrides, 0, GetCastShadow(), GetReceiveShadow());
            return;
        }

        if (m_TexturesNeedUpload) {
            UploadMaterialTextures(ctx);
        }

        overrides.setColor("u_AlbedoColor", GetAlbedoColor());
        overrides.setColor("u_EmissiveColor", GetEmissiveColor());
        overrides.setColor("u_TintColor", Color::White());

        // Set ALL material uniforms - shaders use what they need and ignore the rest
        // This approach supports custom shaders without requiring per-shader-type code paths

        // PBR uniforms: u_MaterialParams1 = metallic, roughness, normalScale, emissiveStrength
        overrides.setVec4("u_MaterialParams1", Vec4(GetMetallic(), GetRoughness(), GetNormalScale(), GetEmissiveStrength()));
        // PBR uniforms: u_MaterialParams2 = alphaCutoff, aoStrength, heightScale, unused
        overrides.setVec4("u_MaterialParams2", Vec4(0.5f, 1.0f, 1.0f, 0.0f));

        // Toon uniforms: u_ToonMaterialParams = shadowBands, specularBands, normalScale, emissiveStrength
        overrides.setVec4("u_ToonMaterialParams", Vec4(GetShadowBands(), GetSpecularBands(), GetNormalScale(), GetEmissiveStrength()));
        // Toon uniforms: u_ToonMaterialParams2 = alphaCutoff, rimPower, rimIntensity, specularPower
        overrides.setVec4("u_ToonMaterialParams2", Vec4(0.5f, GetRimPower(), GetRimIntensity(), GetSpecularPower()));
        // Toon uniforms: u_ToonParams = shadowThreshold, shadowSoftness, specularThreshold, specularSoftness
        overrides.setVec4("u_ToonParams", Vec4(GetShadowThreshold(), GetShadowSoftness(), GetShadowThreshold(), GetShadowSoftness()));

        // Apply generic shader parameters from shaderParams map
        // This allows custom shaders to receive their parameters without C++ code changes
        for (const auto& [uniformName, value] : m_ShaderParams) {
            std::visit([&overrides, &uniformName](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, float>) {
                    overrides.setFloat(uniformName, arg);
                } else if constexpr (std::is_same_v<T, int>) {
                    overrides.setInt(uniformName, arg);
                } else if constexpr (std::is_same_v<T, bool>) {
                    overrides.setBool(uniformName, arg);
                } else if constexpr (std::is_same_v<T, Vec2>) {
                    overrides.setVec2(uniformName, arg);
                } else if constexpr (std::is_same_v<T, Vec3>) {
                    overrides.setVec3(uniformName, arg);
                } else if constexpr (std::is_same_v<T, Vec4>) {
                    overrides.setVec4(uniformName, arg);
                } else if constexpr (std::is_same_v<T, Color>) {
                    overrides.setColor(uniformName, arg);
                } else if constexpr (std::is_same_v<T, TextureHandle>) {
                    overrides.setTexture(uniformName, arg);
                }
            }, value);
        }

        Vec4 textureFlags(
            m_AlbedoTextureHandle.isValid() ? 1.0f : 0.0f,
            m_MetallicRoughnessTextureHandle.isValid() ? 1.0f : 0.0f,
            m_NormalTextureHandle.isValid() ? 1.0f : 0.0f,
            m_EmissiveTextureHandle.isValid() ? 1.0f : 0.0f
        );
        overrides.setVec4("u_TextureFlags", textureFlags);

        // Always bind textures (valid or not) to prevent stale texture bleeding
        overrides.setTexture("u_AlbedoTexture", m_AlbedoTextureHandle);
        overrides.setTexture("u_MetallicRoughnessTexture", m_MetallicRoughnessTextureHandle);
        overrides.setTexture("u_NormalTexture", m_NormalTextureHandle);
        overrides.setTexture("u_EmissiveTexture", m_EmissiveTextureHandle);
    } else {
        material = GetDoubleSided()
            ? ctx.getDefaultColoredDoubleSidedMaterial()
            : ctx.getDefaultColoredMaterial();

        overrides.setColor("u_TintColor", GetColor());
    }

    ctx.drawMesh(m_MeshHandle, material, transform, overrides, 0, GetCastShadow(), GetReceiveShadow());
}

AABB PrimitiveMesh3D::getWorldBounds() const {
    if (!m_Owner) {
        return AABB();
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return AABB();
    }

    PrimitiveShape shape = GetShape();
    float size = GetSize();
    float height = GetHeight();

    Vec3 halfExtents;
    switch (shape) {
        case PrimitiveShape::Cube:
        case PrimitiveShape::Pyramid:
            halfExtents = Vec3(size * 0.5f, height * 0.5f, size * 0.5f);
            break;

        case PrimitiveShape::Sphere:
            halfExtents = Vec3(size, size, size);
            break;

        case PrimitiveShape::Cylinder:
        case PrimitiveShape::Cone:
        case PrimitiveShape::Capsule:
            halfExtents = Vec3(size, height * 0.5f, size);
            break;

        case PrimitiveShape::Torus: {
            float majorRadius = size;
            float minorRadius = GetMinorRadius();
            float outerRadius = majorRadius + minorRadius;
            halfExtents = Vec3(outerRadius, minorRadius, outerRadius);
            break;
        }

        default:
            halfExtents = Vec3(size, size, size);
            break;
    }

    AABB localBounds(Vec3(-halfExtents.x, -halfExtents.y, -halfExtents.z),
                     Vec3(halfExtents.x, halfExtents.y, halfExtents.z));

    Mat4 worldTransform = node3D->GetGlobalTransformMatrix();
    return localBounds.Transform(worldTransform);
}

math::OBB PrimitiveMesh3D::getOrientedBounds() const {
    if (!m_Owner) {
        return math::OBB();
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return math::OBB();
    }

    PrimitiveShape shape = GetShape();
    float size = GetSize();
    float height = GetHeight();

    Vec3 halfExtents;
    switch (shape) {
        case PrimitiveShape::Cube:
        case PrimitiveShape::Pyramid:
            halfExtents = Vec3(size * 0.5f, height * 0.5f, size * 0.5f);
            break;

        case PrimitiveShape::Sphere:
            halfExtents = Vec3(size, size, size);
            break;

        case PrimitiveShape::Cylinder:
        case PrimitiveShape::Cone:
        case PrimitiveShape::Capsule:
            halfExtents = Vec3(size, height * 0.5f, size);
            break;

        case PrimitiveShape::Torus: {
            float majorRadius = size;
            float minorRadius = GetMinorRadius();
            float outerRadius = majorRadius + minorRadius;
            halfExtents = Vec3(outerRadius, minorRadius, outerRadius);
            break;
        }

        default:
            halfExtents = Vec3(size, size, size);
            break;
    }

    Vec3 worldPos = node3D->GetGlobalPosition();
    Quat worldRot = node3D->GetGlobalRotation();
    Vec3 worldScale = node3D->GetGlobalScale();

    halfExtents.x *= std::abs(worldScale.x);
    halfExtents.y *= std::abs(worldScale.y);
    halfExtents.z *= std::abs(worldScale.z);

    return math::OBB(worldPos, halfExtents, worldRot);
}

bool PrimitiveMesh3D::IntersectRay(const math::Ray& ray, float& outDistance) const {
    if (!m_Owner) {
        return false;
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return false;
    }

    Mat4 worldTransform = node3D->GetGlobalTransformMatrix();
    Mat4 invWorldTransform = worldTransform.Inverse();

    Vec4 localOrigin4 = invWorldTransform * Vec4(ray.origin.x, ray.origin.y, ray.origin.z, 1.0f);
    Vec3 localOrigin(localOrigin4.x / localOrigin4.w, localOrigin4.y / localOrigin4.w, localOrigin4.z / localOrigin4.w);

    Vec4 localDir4 = invWorldTransform * Vec4(ray.direction.x, ray.direction.y, ray.direction.z, 0.0f);
    Vec3 localDir(localDir4.x, localDir4.y, localDir4.z);
    localDir = localDir.Normalized();

    math::Ray localRay(localOrigin, localDir);

    MeshData meshData = GenerateShapeMesh();

    float closestHit = std::numeric_limits<float>::infinity();
    bool hitAny = false;

    const auto& vertices = meshData.vertices;
    const auto& indices = meshData.indices;

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const Vec3& v0 = vertices[indices[i]].position;
        const Vec3& v1 = vertices[indices[i + 1]].position;
        const Vec3& v2 = vertices[indices[i + 2]].position;

        float hitDist = 0.0f;
        if (localRay.IntersectTriangle(v0, v1, v2, hitDist)) {
            if (hitDist < closestHit) {
                closestHit = hitDist;
                hitAny = true;
            }
        }
    }

    if (hitAny) {

        Vec3 localHitPoint = localRay.GetPoint(closestHit);
        Vec4 worldHitPoint4 = worldTransform * Vec4(localHitPoint.x, localHitPoint.y, localHitPoint.z, 1.0f);
        Vec3 worldHitPoint(worldHitPoint4.x / worldHitPoint4.w,
                          worldHitPoint4.y / worldHitPoint4.w,
                          worldHitPoint4.z / worldHitPoint4.w);

        outDistance = (worldHitPoint - ray.origin).Length();
        return true;
    }

    return false;
}

RenderLayer PrimitiveMesh3D::getRenderLayer() const {

    return RenderLayer::Opaque;
}

bool PrimitiveMesh3D::HasMaterialOverride() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("materialOverrideEnabled");
    return prop ? prop->GetValue<bool>() : false;
}

void PrimitiveMesh3D::SetMaterialOverrideEnabled(bool enabled) {
    SetPropertyValue<bool>("materialOverrideEnabled", enabled);
    if (enabled && !m_MaterialOverride.has_value()) {

        m_MaterialOverride = PBRMaterialProperties();
        m_MaterialOverride->albedoColor = GetAlbedoColor();
        m_MaterialOverride->metallic = GetMetallic();
        m_MaterialOverride->roughness = GetRoughness();
        m_MaterialOverride->normalScale = GetNormalScale();
        m_MaterialOverride->emissiveColor = GetEmissiveColor();
        m_MaterialOverride->emissiveStrength = GetEmissiveStrength();

    } else if (!enabled) {
        m_MaterialOverride.reset();
    }
}

const PBRMaterialProperties& PrimitiveMesh3D::GetMaterialProperties() const {
    static PBRMaterialProperties defaultProps;
    return m_MaterialOverride.has_value() ? m_MaterialOverride.value() : defaultProps;
}

void PrimitiveMesh3D::SetAlbedoColor(const Color& color) {
    SetPropertyValue<Color>("albedoColor", color);
    if (m_MaterialOverride.has_value()) {
        m_MaterialOverride->albedoColor = color;
    }
}

Color PrimitiveMesh3D::GetAlbedoColor() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("albedoColor");
    if (prop) {
        return prop->GetValue<Color>();
    }
    return Color::White();
}

void PrimitiveMesh3D::SetAlbedoTexture(TextureHandle texture) {
    SetPropertyValue<int>("albedoTexture", static_cast<int>(texture.id));
    if (m_MaterialOverride.has_value()) {
        m_MaterialOverride->albedoTexture = texture;
    }
}

TextureHandle PrimitiveMesh3D::GetAlbedoTexture() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("albedoTexture");
    return prop ? TextureHandle(static_cast<uint32_t>(prop->GetValue<int>())) : TextureHandle();
}

void PrimitiveMesh3D::SetMetallic(float metallic) {
    SetPropertyValue<float>("metallic", metallic);
    if (m_MaterialOverride.has_value()) {
        m_MaterialOverride->metallic = metallic;
    }
}

float PrimitiveMesh3D::GetMetallic() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("metallic");
    return prop ? prop->GetValue<float>() : 0.0f;
}

void PrimitiveMesh3D::SetRoughness(float roughness) {
    SetPropertyValue<float>("roughness", roughness);
    if (m_MaterialOverride.has_value()) {
        m_MaterialOverride->roughness = roughness;
    }
}

float PrimitiveMesh3D::GetRoughness() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("roughness");
    return prop ? prop->GetValue<float>() : 0.5f;
}

void PrimitiveMesh3D::SetMetallicRoughnessTexture(TextureHandle texture) {
    SetPropertyValue<int>("metallicRoughnessTexture", static_cast<int>(texture.id));
    if (m_MaterialOverride.has_value()) {
        m_MaterialOverride->metallicRoughnessTexture = texture;
    }
}

TextureHandle PrimitiveMesh3D::GetMetallicRoughnessTexture() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("metallicRoughnessTexture");
    return prop ? TextureHandle(static_cast<uint32_t>(prop->GetValue<int>())) : TextureHandle();
}

void PrimitiveMesh3D::SetNormalTexture(TextureHandle texture) {
    SetPropertyValue<int>("normalTexture", static_cast<int>(texture.id));
    if (m_MaterialOverride.has_value()) {
        m_MaterialOverride->normalTexture = texture;
    }
}

TextureHandle PrimitiveMesh3D::GetNormalTexture() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("normalTexture");
    return prop ? TextureHandle(static_cast<uint32_t>(prop->GetValue<int>())) : TextureHandle();
}

void PrimitiveMesh3D::SetNormalScale(float scale) {
    SetPropertyValue<float>("normalScale", scale);
    if (m_MaterialOverride.has_value()) {
        m_MaterialOverride->normalScale = scale;
    }
}

float PrimitiveMesh3D::GetNormalScale() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("normalScale");
    return prop ? prop->GetValue<float>() : 1.0f;
}

void PrimitiveMesh3D::SetEmissiveColor(const Color& color) {
    SetPropertyValue<Color>("emissiveColor", color);
    if (m_MaterialOverride.has_value()) {
        m_MaterialOverride->emissiveColor = color;
    }
}

Color PrimitiveMesh3D::GetEmissiveColor() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("emissiveColor");
    if (prop) {
        return prop->GetValue<Color>();
    }
    return Color();
}

void PrimitiveMesh3D::SetEmissiveTexture(TextureHandle texture) {
    SetPropertyValue<int>("emissiveTexture", static_cast<int>(texture.id));
    if (m_MaterialOverride.has_value()) {
        m_MaterialOverride->emissiveTexture = texture;
    }
}

TextureHandle PrimitiveMesh3D::GetEmissiveTexture() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("emissiveTexture");
    return prop ? TextureHandle(static_cast<uint32_t>(prop->GetValue<int>())) : TextureHandle();
}

void PrimitiveMesh3D::SetEmissiveStrength(float strength) {
    SetPropertyValue<float>("emissiveStrength", strength);
    if (m_MaterialOverride.has_value()) {
        m_MaterialOverride->emissiveStrength = strength;
    }
}

float PrimitiveMesh3D::GetEmissiveStrength() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("emissiveStrength");
    return prop ? prop->GetValue<float>() : 1.0f;
}

// ===== Shader Selection =====

ShaderType PrimitiveMesh3D::GetShaderType() const {
    std::string shaderName = GetPropertyValue<std::string>("shaderType");
    if (shaderName == "Toon") return ShaderType::Toon;
    if (shaderName == "Stylized") return ShaderType::Stylized;
    if (shaderName == "Transparent") return ShaderType::Transparent;
    if (shaderName == "Glow") return ShaderType::Glow;
    if (shaderName == "Unlit") return ShaderType::Unlit;
    if (shaderName == "Standard3D") return ShaderType::Standard3D;
    if (shaderName == "Custom") return ShaderType::Custom;
    return ShaderType::PBR;
}

void PrimitiveMesh3D::SetShaderType(ShaderType type) {
    std::string shaderName;
    switch (type) {
        case ShaderType::Toon: shaderName = "Toon"; break;
        case ShaderType::Stylized: shaderName = "Stylized"; break;
        case ShaderType::Transparent: shaderName = "Transparent"; break;
        case ShaderType::Glow: shaderName = "Glow"; break;
        case ShaderType::Unlit: shaderName = "Unlit"; break;
        case ShaderType::Standard3D: shaderName = "Standard3D"; break;
        case ShaderType::Custom: shaderName = "Custom"; break;
        default: shaderName = "PBR"; break;
    }
    SetPropertyValue<std::string>("shaderType", shaderName);
}

std::string PrimitiveMesh3D::GetCustomVertShaderPath() const {
    return GetPropertyValue<std::string>("customVertShaderPath");
}

void PrimitiveMesh3D::SetCustomVertShaderPath(const std::string& path) {
    SetPropertyValue<std::string>("customVertShaderPath", path);
}

std::string PrimitiveMesh3D::GetCustomFragShaderPath() const {
    return GetPropertyValue<std::string>("customFragShaderPath");
}

void PrimitiveMesh3D::SetCustomFragShaderPath(const std::string& path) {
    SetPropertyValue<std::string>("customFragShaderPath", path);
}

std::string PrimitiveMesh3D::GetCustomLshShaderPath() const {
    return GetPropertyValue<std::string>("customLshShaderPath");
}

void PrimitiveMesh3D::SetCustomLshShaderPath(const std::string& path) {
    SetPropertyValue<std::string>("customLshShaderPath", path);
}

float PrimitiveMesh3D::GetShadowBands() const {
    return GetPropertyValue<float>("shadowBands");
}

void PrimitiveMesh3D::SetShadowBands(float bands) {
    SetPropertyValue<float>("shadowBands", bands);
}

float PrimitiveMesh3D::GetShadowThreshold() const {
    return GetPropertyValue<float>("shadowThreshold");
}

void PrimitiveMesh3D::SetShadowThreshold(float threshold) {
    SetPropertyValue<float>("shadowThreshold", threshold);
}

float PrimitiveMesh3D::GetShadowSoftness() const {
    return GetPropertyValue<float>("shadowSoftness");
}

void PrimitiveMesh3D::SetShadowSoftness(float softness) {
    SetPropertyValue<float>("shadowSoftness", softness);
}

float PrimitiveMesh3D::GetSpecularBands() const {
    return GetPropertyValue<float>("specularBands");
}

void PrimitiveMesh3D::SetSpecularBands(float bands) {
    SetPropertyValue<float>("specularBands", bands);
}

float PrimitiveMesh3D::GetSpecularPower() const {
    return GetPropertyValue<float>("specularPower");
}

void PrimitiveMesh3D::SetSpecularPower(float power) {
    SetPropertyValue<float>("specularPower", power);
}

float PrimitiveMesh3D::GetRimIntensity() const {
    return GetPropertyValue<float>("rimIntensity");
}

void PrimitiveMesh3D::SetRimIntensity(float intensity) {
    SetPropertyValue<float>("rimIntensity", intensity);
}

float PrimitiveMesh3D::GetRimPower() const {
    return GetPropertyValue<float>("rimPower");
}

void PrimitiveMesh3D::SetRimPower(float power) {
    SetPropertyValue<float>("rimPower", power);
}

bool PrimitiveMesh3D::LoadTexture(const std::string& filepath, asset::AssetRef<asset::ImageAsset>& outAsset) {
    if (filepath.empty()) {

        return false;
    }

    outAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());

    bool loaded = outAsset->LoadFromFile(filepath, true, asset::ImageColorSpace::sRGB);

    if (!loaded) {

        outAsset.Reset();
        return false;
    }

    m_TexturesNeedUpload = true;

    return true;
}

void PrimitiveMesh3D::UploadMaterialTextures(RenderContext& ctx) {
    IGfxDevice* device = ctx.getDevice();
    if (!device) {
        return;
    }

    if (m_AlbedoTextureAsset && m_AlbedoTextureAsset->IsLoaded() && !m_AlbedoTextureHandle.isValid()) {
        m_AlbedoTextureHandle = lupine::CreateTexture2DFromImage(device, *m_AlbedoTextureAsset, TextureFormat::RGBA8_SRGB);

    }

    if (m_MetallicRoughnessTextureAsset && m_MetallicRoughnessTextureAsset->IsLoaded() && !m_MetallicRoughnessTextureHandle.isValid()) {
        m_MetallicRoughnessTextureHandle = lupine::CreateTexture2DFromImage(device, *m_MetallicRoughnessTextureAsset, TextureFormat::RGBA8_UNORM);

    }

    if (m_NormalTextureAsset && m_NormalTextureAsset->IsLoaded() && !m_NormalTextureHandle.isValid()) {
        m_NormalTextureHandle = lupine::CreateTexture2DFromImage(device, *m_NormalTextureAsset, TextureFormat::RGBA8_UNORM);

    }

    if (m_EmissiveTextureAsset && m_EmissiveTextureAsset->IsLoaded() && !m_EmissiveTextureHandle.isValid()) {
        m_EmissiveTextureHandle = lupine::CreateTexture2DFromImage(device, *m_EmissiveTextureAsset, TextureFormat::RGBA8_SRGB);

    }

    m_TexturesNeedUpload = false;
}

}
}

