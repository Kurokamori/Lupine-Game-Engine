#include "lupine/components/NineSlicePanel.hpp"
#include "lupine/components/UIImageDraw.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/rendering/ViewportUtils.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/math/OBB.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

NineSlicePanel::NineSlicePanel()
    : UIControl("NineSlicePanel")
{
}

NineSlicePanel::NineSlicePanel(const std::string& name)
    : UIControl(name)
{
}

NineSlicePanel::~NineSlicePanel() {
}

void NineSlicePanel::DefineProperties() {
    DefineUIControlProperties(200.0f, 100.0f, "useUISpace", "Size");

    // Rendering
    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, -100, 100, 1, "Rendering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Rendering"));

    // Texture
    DefineProperty(PROPERTY_FILE_GROUP(texturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "Texture"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(modulate, Color, Color::White(), "Texture"));

    // Nine-Slice Margins
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(marginLeft, 10.0f, 0.0f, 1000.0f, 1.0f, "Margins"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(marginRight, 10.0f, 0.0f, 1000.0f, 1.0f, "Margins"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(marginTop, 10.0f, 0.0f, 1000.0f, 1.0f, "Margins"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(marginBottom, 10.0f, 0.0f, 1000.0f, 1.0f, "Margins"));

    // Nine-Slice Options
    DefineProperty(PROPERTY_ENUM_GROUP(nineSliceAxisH, 0, "NineSlice", Stretch, Tile));
    DefineProperty(PROPERTY_ENUM_GROUP(nineSliceAxisV, 0, "NineSlice", Stretch, Tile));
    DefineProperty(PROPERTY_DEFAULT_GROUP(nineSliceDrawCenter, Bool, true, "NineSlice"));
}

void NineSlicePanel::OnAwake() {
    std::string texPath = GetTexturePath();
    if (!texPath.empty()) {
        LoadTexture(texPath);
    }
}

void NineSlicePanel::OnReady() {
}

bool NineSlicePanel::OnGizmoScale(float scaleDelta, int axis, bool is3D) {
    // Resizing is delegated to the base, which writes whichever property actually drives
    // the axis: width/height when point-anchored, the offsets when anchor-stretched (where
    // width/height are never read at all), and nothing when a container owns the rect.
    const bool handled = UIControl::OnGizmoScale(scaleDelta, axis, is3D);
    return handled;
}

// ===== Property Accessors =====

Vec2 NineSlicePanel::GetContentMinSize() const {
    // The nine-slice corners must not be squished: minimum size is the sum of margins.
    return Vec2(GetMarginLeft() + GetMarginRight(), GetMarginTop() + GetMarginBottom());
}

int NineSlicePanel::GetLayer() const {
    return GetPropertyValue<int>("layer");
}

void NineSlicePanel::SetLayer(int layer) {
    SetPropertyValue<int>("layer", layer);
}

int NineSlicePanel::GetSortingOrder() const {
    return GetPropertyValue<int>("sortingOrder");
}

void NineSlicePanel::SetSortingOrder(int order) {
    SetPropertyValue<int>("sortingOrder", order);
}

std::string NineSlicePanel::GetTexturePath() const {
    return GetPropertyValue<std::string>("texturePath");
}

void NineSlicePanel::SetTexturePath(const std::string& path) {
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
    SetPropertyValue<std::string>("texturePath", resPath);
    LoadTexture(resPath);
}

const std::vector<UIControl::ThemeBinding>& NineSlicePanel::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = {
        { "modulate", "modulate", ThemeBinding::Kind::Color }
    };
    return kBindings;
}

Color NineSlicePanel::GetModulate() const {
    return ResolveThemedColor("modulate", "modulate");
}

void NineSlicePanel::SetModulate(const Color& color) {
    SetThemedProperty<Color>("modulate", color);
}

float NineSlicePanel::GetMarginLeft() const {
    return GetPropertyValue<float>("marginLeft");
}

void NineSlicePanel::SetMarginLeft(float margin) {
    SetPropertyValue<float>("marginLeft", margin);
}

float NineSlicePanel::GetMarginRight() const {
    return GetPropertyValue<float>("marginRight");
}

void NineSlicePanel::SetMarginRight(float margin) {
    SetPropertyValue<float>("marginRight", margin);
}

float NineSlicePanel::GetMarginTop() const {
    return GetPropertyValue<float>("marginTop");
}

void NineSlicePanel::SetMarginTop(float margin) {
    SetPropertyValue<float>("marginTop", margin);
}

float NineSlicePanel::GetMarginBottom() const {
    return GetPropertyValue<float>("marginBottom");
}

void NineSlicePanel::SetMarginBottom(float margin) {
    SetPropertyValue<float>("marginBottom", margin);
}

UINineSliceAxisMode NineSlicePanel::GetNineSliceAxisHorizontal() const {
    return UINineSliceAxisModeFromInt(GetPropertyValue<int>("nineSliceAxisH"));
}

void NineSlicePanel::SetNineSliceAxisHorizontal(UINineSliceAxisMode mode) {
    SetPropertyValue<int>("nineSliceAxisH", static_cast<int>(mode));
}

UINineSliceAxisMode NineSlicePanel::GetNineSliceAxisVertical() const {
    return UINineSliceAxisModeFromInt(GetPropertyValue<int>("nineSliceAxisV"));
}

void NineSlicePanel::SetNineSliceAxisVertical(UINineSliceAxisMode mode) {
    SetPropertyValue<int>("nineSliceAxisV", static_cast<int>(mode));
}

bool NineSlicePanel::GetNineSliceDrawCenter() const {
    return GetPropertyValue<bool>("nineSliceDrawCenter");
}

void NineSlicePanel::SetNineSliceDrawCenter(bool drawCenter) {
    SetPropertyValue<bool>("nineSliceDrawCenter", drawCenter);
}

// ===== Private Methods =====

void NineSlicePanel::LoadTexture(const std::string& path) {
    if (path.empty()) {
        m_TextureAsset.Reset();
        m_TextureHandle = TextureHandle();
        m_CurrentTexturePath = "";
        return;
    }

    m_TextureAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
    bool loaded = m_TextureAsset->LoadFromFile(path, true, asset::ImageColorSpace::sRGB);

    if (!loaded) {
        
        m_TextureAsset.Reset();
        m_TextureHandle = TextureHandle();
        m_CurrentTexturePath = "";
    } else {
        m_CurrentTexturePath = path;
    }
}

void NineSlicePanel::UploadTextureToGPU(RenderContext& ctx) {
    if (!m_TextureAsset || !m_TextureAsset->GetData()) {
        return;
    }

    if (m_TextureHandle.isValid()) {
        return; // Already uploaded
    }

    auto device = ctx.getDevice();
    if (!device) {
        return;
    }

    m_TextureHandle = lupine::CreateTexture2DFromImage(device, *m_TextureAsset, TextureFormat::RGBA8_UNORM);
}

void NineSlicePanel::RenderNineSlice(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {
    // Check if texture path changed
    std::string currentPath = GetTexturePath();
    if (currentPath != m_CurrentTexturePath) {
        // Destroy old texture
        if (m_TextureHandle.isValid()) {
            ctx.getDevice()->destroyTexture(m_TextureHandle);
            m_TextureHandle = TextureHandle();
        }

        m_TextureAsset.Reset();

        // Load new texture
        if (!currentPath.empty()) {
            LoadTexture(currentPath);
        }
    }

    // Upload texture to GPU if needed
    if (!m_TextureHandle.isValid() && m_TextureAsset.IsValid() && m_TextureAsset->IsLoaded()) {
        UploadTextureToGPU(ctx);
    }

    if (!m_TextureHandle.isValid()) {
        return; // No texture to render
    }

    int textureWidth = m_TextureAsset->GetWidth();
    int textureHeight = m_TextureAsset->GetHeight();
    if (textureWidth == 0 || textureHeight == 0) {
        return;
    }

    // Drive the shared nine-slice painter with this panel's per-side margins (in source
    // texture pixels) and axis/center options, so a single implementation backs every
    // nine-slice control (Button/Panel background images and this dedicated panel).
    UINineSlice nineSlice;
    nineSlice.marginLeft = GetMarginLeft();
    nineSlice.marginTop = GetMarginTop();
    nineSlice.marginRight = GetMarginRight();
    nineSlice.marginBottom = GetMarginBottom();
    nineSlice.axisHorizontal = GetNineSliceAxisHorizontal();
    nineSlice.axisVertical = GetNineSliceAxisVertical();
    nineSlice.drawCenter = GetNineSliceDrawCenter();

    DrawUIImage(ctx, position, size, rotation, m_TextureHandle,
                textureWidth, textureHeight, GetModulate(),
                Vec4(0.0f, 0.0f, 0.0f, 0.0f), UIImageStretchMode::NineSlice, nineSlice);
}

// ===== IRenderableComponent Implementation =====

void NineSlicePanel::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return;
    }

    const Rect __rect = GetResolvedRect();
    Vec2 position = __rect.GetCenter();
    Vec2 size = __rect.size;
    float rotation = node2D->GetGlobalRotation();

    RenderNineSlice(ctx, position, size, rotation);
}

AABB NineSlicePanel::getWorldBounds() const {
    if (!m_Owner) {
        return AABB();
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return AABB();
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 size = GetBoundsSize();
    Vec2 halfSize = size * 0.5f;

    return AABB(
        Vec3(position.x - halfSize.x, position.y - halfSize.y, -0.1f),
        Vec3(position.x + halfSize.x, position.y + halfSize.y, 0.1f)
    );
}

RenderLayer NineSlicePanel::getRenderLayer() const {
    Color modulate = GetModulate();
    if (modulate.a >= 1.0f) {
        return RenderLayer::Opaque;
    }
    return RenderLayer::Transparent;
}

SpatialType NineSlicePanel::getSpatialType() const {
    return GetUISpatialType();
}

OBB NineSlicePanel::getOrientedBounds() const {
    if (!m_Owner) {
        return OBB();
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return OBB();
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 size = GetBoundsSize();
    float rotation = node2D->GetGlobalRotation();

    Vec3 center(position.x, position.y, 0.0f);
    Vec3 halfExtents(size.x * 0.5f, size.y * 0.5f, 0.1f);
    Quat quatRotation = Quat::FromAxisAngle(Vec3::UnitZ(), rotation);

    return OBB(center, halfExtents, quatRotation);
}

bool NineSlicePanel::IntersectRay(const Ray& ray, float& outDistance) const {
    OBB obb = getOrientedBounds();

    Vec3 localRayOrigin = obb.rotation.Inverse() * (ray.origin - obb.center);
    Vec3 localRayDir = obb.rotation.Inverse() * ray.direction;

    AABB localAABB(
        Vec3(-obb.extents.x, -obb.extents.y, -obb.extents.z),
        Vec3(obb.extents.x, obb.extents.y, obb.extents.z)
    );

    Ray localRay(localRayOrigin, localRayDir);
    return localAABB.IntersectRay(localRay, outDistance);
}

bool NineSlicePanel::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
    std::string currentPath = GetTexturePath();
    if (currentPath.empty()) {
        return false;
    }

    // Resolve our path for comparison
    std::string resolvedCurrentPath;
    auto& assetDb = asset::AssetDatabase::GetInstance();
    if (assetDb.IsInitialized()) {
        resolvedCurrentPath = assetDb.ResolveAsset(currentPath);
    }

    // Check if this is our texture
    bool matches = (currentPath == changedPath) ||
                   (!resolvedCurrentPath.empty() && !resolvedChangedPath.empty() &&
                    resolvedCurrentPath == resolvedChangedPath);

    if (matches) {

        // Invalidate instance state - force reload on next render
        m_TextureHandle = TextureHandle();
        m_TextureAsset.Reset();
        m_CurrentTexturePath.clear();

        return true;
    }

    return false;
}

} // namespace components
} // namespace lupine

