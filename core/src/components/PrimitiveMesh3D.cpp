#include "lupine/components/PrimitiveMesh3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
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

const Color& PrimitiveMesh3D::GetColor() const {
    static Color cachedColor;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("color");
    if (prop) {
        cachedColor = prop->GetValue<Color>();
        return cachedColor;
    }
    static Color defaultColor = Color::White();
    return defaultColor;
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

    if (!meshData.vertices.empty()) {
        const auto& v0 = meshData.vertices[0];

    }

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

    if (useMaterialOverride && ctx.getDefaultPBRMaterial().isValid()) {

        material = ctx.getDefaultPBRMaterial();

        if (m_TexturesNeedUpload) {
            UploadMaterialTextures(ctx);
        }

        overrides.setColor("u_AlbedoColor", GetAlbedoColor());
        overrides.setColor("u_EmissiveColor", GetEmissiveColor());
        overrides.setColor("u_TintColor", Color::White());

        Vec4 params1(GetMetallic(), GetRoughness(), GetNormalScale(), GetEmissiveStrength());
        overrides.setVec4("u_MaterialParams1", params1);

        Vec4 params2(0.5f, 1.0f, 1.0f, 0.0f);
        overrides.setVec4("u_MaterialParams2", params2);

        Vec4 textureFlags(
            m_AlbedoTextureHandle.isValid() ? 1.0f : 0.0f,
            m_MetallicRoughnessTextureHandle.isValid() ? 1.0f : 0.0f,
            m_NormalTextureHandle.isValid() ? 1.0f : 0.0f,
            m_EmissiveTextureHandle.isValid() ? 1.0f : 0.0f
        );
        overrides.setVec4("u_TextureFlags", textureFlags);

        if (m_AlbedoTextureHandle.isValid()) {
            overrides.setTexture("u_AlbedoTexture", m_AlbedoTextureHandle);
        }
        if (m_MetallicRoughnessTextureHandle.isValid()) {
            overrides.setTexture("u_MetallicRoughnessTexture", m_MetallicRoughnessTextureHandle);
        }
        if (m_NormalTextureHandle.isValid()) {
            overrides.setTexture("u_NormalTexture", m_NormalTextureHandle);
        }
        if (m_EmissiveTextureHandle.isValid()) {
            overrides.setTexture("u_EmissiveTexture", m_EmissiveTextureHandle);
        }
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

const Color& PrimitiveMesh3D::GetAlbedoColor() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("albedoColor");
    static Color cachedColor;
    if (prop) {
        cachedColor = prop->GetValue<Color>();
        return cachedColor;
    }

    static Color defaultColor = Color::White();
    return defaultColor;
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

const Color& PrimitiveMesh3D::GetEmissiveColor() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("emissiveColor");
    static Color cachedColor;
    if (prop) {
        cachedColor = prop->GetValue<Color>();
    }
    return cachedColor;
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
        TextureDesc desc;
        desc.width = m_AlbedoTextureAsset->GetWidth();
        desc.height = m_AlbedoTextureAsset->GetHeight();
        desc.format = TextureFormat::RGBA8_SRGB;
        desc.usage = TextureUsage::Sampled;
        desc.initialData = m_AlbedoTextureAsset->GetData();
        m_AlbedoTextureHandle = device->createTexture(desc);

    }

    if (m_MetallicRoughnessTextureAsset && m_MetallicRoughnessTextureAsset->IsLoaded() && !m_MetallicRoughnessTextureHandle.isValid()) {
        TextureDesc desc;
        desc.width = m_MetallicRoughnessTextureAsset->GetWidth();
        desc.height = m_MetallicRoughnessTextureAsset->GetHeight();
        desc.format = TextureFormat::RGBA8_UNORM;
        desc.usage = TextureUsage::Sampled;
        desc.initialData = m_MetallicRoughnessTextureAsset->GetData();
        m_MetallicRoughnessTextureHandle = device->createTexture(desc);

    }

    if (m_NormalTextureAsset && m_NormalTextureAsset->IsLoaded() && !m_NormalTextureHandle.isValid()) {
        TextureDesc desc;
        desc.width = m_NormalTextureAsset->GetWidth();
        desc.height = m_NormalTextureAsset->GetHeight();
        desc.format = TextureFormat::RGBA8_UNORM;
        desc.usage = TextureUsage::Sampled;
        desc.initialData = m_NormalTextureAsset->GetData();
        m_NormalTextureHandle = device->createTexture(desc);

    }

    if (m_EmissiveTextureAsset && m_EmissiveTextureAsset->IsLoaded() && !m_EmissiveTextureHandle.isValid()) {
        TextureDesc desc;
        desc.width = m_EmissiveTextureAsset->GetWidth();
        desc.height = m_EmissiveTextureAsset->GetHeight();
        desc.format = TextureFormat::RGBA8_SRGB;
        desc.usage = TextureUsage::Sampled;
        desc.initialData = m_EmissiveTextureAsset->GetData();
        m_EmissiveTextureHandle = device->createTexture(desc);

    }

    m_TexturesNeedUpload = false;
}

}
}

