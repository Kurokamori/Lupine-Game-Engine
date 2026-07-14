#include "lupine/components/Panel3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/logger/Logger.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

using core::Node2D;
using core::Node3D;

Panel3D::Panel3D()
    : Component("Panel3D")
    , m_StyleBox(nullptr)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
    , m_MeshNeedsRegeneration(true)
{

    m_StyleBox = std::make_shared<StyleBoxFlat>();
}

Panel3D::Panel3D(const std::string& name)
    : Component(name)
    , m_StyleBox(nullptr)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
    , m_MeshNeedsRegeneration(true)
{

    m_StyleBox = std::make_shared<StyleBoxFlat>();
}

Panel3D::~Panel3D() {

}

void Panel3D::DefineProperties() {

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(width, 5.0f, 0.0f, 10000.0f, 0.1f, "Size"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(height, 5.0f, 0.0f, 10000.0f, 0.1f, "Size"));

    DefineProperty(PROPERTY_ENUM_GROUP(billboardMode, 1, "Transform", Disabled, Enabled, YAxisOnly));

    DefineProperty(PROPERTY_DEFAULT_GROUP(doubleSided, Bool, true, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(castShadow, Bool, true, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(receiveShadow, Bool, true, "Rendering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, -100, 100, 1, "Rendering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Rendering"));

    DefineProperty(PROPERTY_FILE_GROUP(stylePath, std::string(""), "*.style", "Style"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(backgroundColor, Color, Color(0.8f, 0.8f, 0.8f, 1.0f), "Background"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(opacity, 1.0f, 0.0f, 1.0f, 0.01f, "Background"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(borderEnabled, Bool, false, "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidthLinked, Bool, true, "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidth, Vec4, Vec4(1.0f, 1.0f, 1.0f, 1.0f), "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color::Black(), "Border"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadiusLinked, Bool, true, "CornerRadius"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadius, Vec4, Vec4(0.0f, 0.0f, 0.0f, 0.0f), "CornerRadius"));

    DefineProperty(PROPERTY_INT_RANGE_GROUP(cornerDetail, 8, 1, 32, 1, "CornerDetail"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(antiAliasing, Bool, true, "CornerDetail"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(antiAliasingSize, 1.0f, 0.0f, 4.0f, 0.1f, "CornerDetail"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(shadowEnabled, Bool, false, "Shadow"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(shadowColor, Color, Color(0.0f, 0.0f, 0.0f, 0.6f), "Shadow"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(shadowSize, 4.0f, 0.0f, 100.0f, 0.5f, "Shadow"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(shadowOffset, Vec2, Vec2(0.0f, 0.0f), "Shadow"));

    DefineProperty(PROPERTY_FILE_GROUP(customShaderPath, std::string(""), "*.glsl,*.shader", "Material"));
}

void Panel3D::OnAwake() {
    EnsureStyleBox();

    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    m_CornerRadius.FromVec4(cornerRadiusVec);
    m_CornerRadius.SetLinked(cornerRadiusLinked);

    Vec4 borderWidthVec = GetPropertyValue<Vec4>("borderWidth");
    bool borderWidthLinked = GetPropertyValue<bool>("borderWidthLinked");
    m_BorderWidth.FromVec4(borderWidthVec);
    m_BorderWidth.SetLinked(borderWidthLinked);

    std::string stylePath = GetStylePath();
    if (!stylePath.empty()) {
        LoadStyleFromFile(stylePath);
    }
}

void Panel3D::OnReady() {
    m_MeshNeedsRegeneration = true;
}

void Panel3D::OnRender() {

}

bool Panel3D::OnGizmoScale(float scaleDelta, int axis, bool is3D) {

    if (is3D) {
        float currentWidth = GetWidth();
        float currentHeight = GetHeight();

        if (axis == 0) {

            SetWidth(std::max(0.1f, currentWidth + scaleDelta * currentWidth));
        } else if (axis == 1) {

            SetHeight(std::max(0.1f, currentHeight + scaleDelta * currentHeight));
        } else if (axis == -1) {

            SetWidth(std::max(0.1f, currentWidth + scaleDelta * currentWidth));
            SetHeight(std::max(0.1f, currentHeight + scaleDelta * currentHeight));
        }

        m_MeshNeedsRegeneration = true;

        return true;
    }

    return false;
}

float Panel3D::GetWidth() const {
    return GetPropertyValue<float>("width");
}

void Panel3D::SetWidth(float width) {
    SetPropertyValue<float>("width", width);
    m_MeshNeedsRegeneration = true;
}

float Panel3D::GetHeight() const {
    return GetPropertyValue<float>("height");
}

void Panel3D::SetHeight(float height) {
    SetPropertyValue<float>("height", height);
    m_MeshNeedsRegeneration = true;
}

Panel3DBillboardMode Panel3D::GetBillboardMode() const {
    int mode = GetPropertyValue<int>("billboardMode");
    return static_cast<Panel3DBillboardMode>(mode);
}

void Panel3D::SetBillboardMode(Panel3DBillboardMode mode) {
    SetPropertyValue<int>("billboardMode", static_cast<int>(mode));
}

bool Panel3D::GetDoubleSided() const {
    return GetPropertyValue<bool>("doubleSided");
}

void Panel3D::SetDoubleSided(bool doubleSided) {
    SetPropertyValue<bool>("doubleSided", doubleSided);
}

bool Panel3D::GetCastShadow() const {
    return GetPropertyValue<bool>("castShadow");
}

void Panel3D::SetCastShadow(bool castShadow) {
    SetPropertyValue<bool>("castShadow", castShadow);
}

bool Panel3D::GetReceiveShadow() const {
    return GetPropertyValue<bool>("receiveShadow");
}

void Panel3D::SetReceiveShadow(bool receiveShadow) {
    SetPropertyValue<bool>("receiveShadow", receiveShadow);
}

int Panel3D::GetLayer() const {
    return GetPropertyValue<int>("layer");
}

void Panel3D::SetLayer(int layer) {
    SetPropertyValue<int>("layer", layer);
}

int Panel3D::GetSortingOrder() const {
    return GetPropertyValue<int>("sortingOrder");
}

void Panel3D::SetSortingOrder(int order) {
    SetPropertyValue<int>("sortingOrder", order);
}

void Panel3D::SetStyleBox(std::shared_ptr<StyleBox> styleBox) {
    m_StyleBox = styleBox;
    m_MeshNeedsRegeneration = true;

    if (auto flatStyle = GetStyleBoxFlat()) {
        SetBackgroundColor(flatStyle->GetBackgroundColor());
        SetOpacity(flatStyle->GetOpacity());
        SetBorderWidthLinked(flatStyle->GetBorderWidthLinked());

    }
}

bool Panel3D::LoadStyleFromFile(const std::string& filepath) {
    auto styleBox = StyleBox::LoadFromFile(filepath);
    if (styleBox) {
        SetStyleBox(styleBox);
        SetStylePath(filepath);

        return true;
    }
    return false;
}

bool Panel3D::SaveStyleToFile(const std::string& filepath) const {
    if (m_StyleBox) {
        bool success = m_StyleBox->SaveToFile(filepath);
        if (success) {

        }
        return success;
    }
    return false;
}

const std::string& Panel3D::GetStylePath() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("stylePath");
    return cachedPath;
}

void Panel3D::SetStylePath(const std::string& path) {
    SetPropertyValue<std::string>("stylePath", path);
}

Color Panel3D::GetBackgroundColor() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("backgroundColor");
    if (prop) {
        return prop->GetValue<Color>();
    }
    return Color::White();
}

void Panel3D::SetBackgroundColor(const Color& color) {
    SetPropertyValue<Color>("backgroundColor", color);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetBackgroundColor(color);
    }
    m_MeshNeedsRegeneration = true;
}

float Panel3D::GetOpacity() const {
    return GetPropertyValue<float>("opacity");
}

void Panel3D::SetOpacity(float opacity) {
    SetPropertyValue<float>("opacity", opacity);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetOpacity(opacity);
    }
    m_MeshNeedsRegeneration = true;
}

bool Panel3D::GetBorderWidthLinked() const {
    return m_BorderWidth.IsLinked();
}

void Panel3D::SetBorderWidthLinked(bool linked) {
    m_BorderWidth.SetLinked(linked);
    SetPropertyValue<bool>("borderWidthLinked", linked);
}

float Panel3D::GetBorderWidthLeft() const {
    return m_BorderWidth.Get(3);
}

void Panel3D::SetBorderWidthLeft(float width) {
    m_BorderWidth.Set(3, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Panel3D::GetBorderWidthRight() const {
    return m_BorderWidth.Get(1);
}

void Panel3D::SetBorderWidthRight(float width) {
    m_BorderWidth.Set(1, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Panel3D::GetBorderWidthTop() const {
    return m_BorderWidth.Get(0);
}

void Panel3D::SetBorderWidthTop(float width) {
    m_BorderWidth.Set(0, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Panel3D::GetBorderWidthBottom() const {
    return m_BorderWidth.Get(2);
}

void Panel3D::SetBorderWidthBottom(float width) {
    m_BorderWidth.Set(2, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
    m_MeshNeedsRegeneration = true;
}

bool Panel3D::GetBorderEnabled() const {
    return GetPropertyValue<bool>("borderEnabled");
}

void Panel3D::SetBorderEnabled(bool enabled) {
    SetPropertyValue<bool>("borderEnabled", enabled);
    m_MeshNeedsRegeneration = true;
}

Color Panel3D::GetBorderColor() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("borderColor");
    if (prop) {
        return prop->GetValue<Color>();
    }
    return Color::Black();
}

void Panel3D::SetBorderColor(const Color& color) {
    SetPropertyValue<Color>("borderColor", color);
    m_MeshNeedsRegeneration = true;
}

bool Panel3D::GetCornerRadiusLinked() const {
    return m_CornerRadius.IsLinked();
}

void Panel3D::SetCornerRadiusLinked(bool linked) {
    m_CornerRadius.SetLinked(linked);
    SetPropertyValue<bool>("cornerRadiusLinked", linked);
}

float Panel3D::GetCornerRadiusTopLeft() const {
    return m_CornerRadius.Get(0);
}

void Panel3D::SetCornerRadiusTopLeft(float radius) {
    m_CornerRadius.Set(0, radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Panel3D::GetCornerRadiusTopRight() const {
    return m_CornerRadius.Get(1);
}

void Panel3D::SetCornerRadiusTopRight(float radius) {
    m_CornerRadius.Set(1, radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Panel3D::GetCornerRadiusBottomLeft() const {
    return m_CornerRadius.Get(3);
}

void Panel3D::SetCornerRadiusBottomLeft(float radius) {
    m_CornerRadius.Set(3, radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Panel3D::GetCornerRadiusBottomRight() const {
    return m_CornerRadius.Get(2);
}

void Panel3D::SetCornerRadiusBottomRight(float radius) {
    m_CornerRadius.Set(2, radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

int Panel3D::GetCornerDetail() const {
    return GetPropertyValue<int>("cornerDetail");
}

void Panel3D::SetCornerDetail(int detail) {
    SetPropertyValue<int>("cornerDetail", detail);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetCornerDetail(detail);
    }
    m_MeshNeedsRegeneration = true;
}

bool Panel3D::GetAntiAliasing() const {
    return GetPropertyValue<bool>("antiAliasing");
}

void Panel3D::SetAntiAliasing(bool aa) {
    SetPropertyValue<bool>("antiAliasing", aa);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetAntiAliasing(aa);
    }
    m_MeshNeedsRegeneration = true;
}

float Panel3D::GetAntiAliasingSize() const {
    return GetPropertyValue<float>("antiAliasingSize");
}

void Panel3D::SetAntiAliasingSize(float size) {
    SetPropertyValue<float>("antiAliasingSize", size);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetAntiAliasingSize(size);
    }
    m_MeshNeedsRegeneration = true;
}

bool Panel3D::GetShadowEnabled() const {
    return GetPropertyValue<bool>("shadowEnabled");
}

void Panel3D::SetShadowEnabled(bool enabled) {
    SetPropertyValue<bool>("shadowEnabled", enabled);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetShadowEnabled(enabled);
    }
    m_MeshNeedsRegeneration = true;
}

Color Panel3D::GetShadowColor() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("shadowColor");
    if (prop) {
        return prop->GetValue<Color>();
    }
    return Color(0.0f, 0.0f, 0.0f, 0.6f);
}

void Panel3D::SetShadowColor(const Color& color) {
    SetPropertyValue<Color>("shadowColor", color);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetShadowColor(color);
    }
    m_MeshNeedsRegeneration = true;
}

float Panel3D::GetShadowSize() const {
    return GetPropertyValue<float>("shadowSize");
}

void Panel3D::SetShadowSize(float size) {
    SetPropertyValue<float>("shadowSize", size);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetShadowSize(size);
    }
    m_MeshNeedsRegeneration = true;
}

const Vec2& Panel3D::GetShadowOffset() const {
    static Vec2 cachedOffset;
    static Vec2 defaultOffset = Vec2::Zero();
    const ComponentProperty* prop = m_CustomProperties.GetProperty("shadowOffset");
    if (prop) {
        cachedOffset = prop->GetValue<Vec2>();
        return cachedOffset;
    }
    return defaultOffset;
}

void Panel3D::SetShadowOffset(const Vec2& offset) {
    SetPropertyValue<Vec2>("shadowOffset", offset);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetShadowOffset(offset);
    }
    m_MeshNeedsRegeneration = true;
}

const std::string& Panel3D::GetCustomShaderPath() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("customShaderPath");
    return cachedPath;
}

void Panel3D::SetCustomShaderPath(const std::string& path) {
    SetPropertyValue<std::string>("customShaderPath", path);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetCustomShaderPath(path);
    }
}

Mat4 Panel3D::CalculateBillboardTransform(const Mat4& viewMatrix) const {
    Panel3DBillboardMode mode = GetBillboardMode();

    if (mode == Panel3DBillboardMode::Disabled) {
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

    if (mode == Panel3DBillboardMode::Enabled) {

        glm::mat4 glmView = viewMatrix.ToGLM();
        Vec3 right(glmView[0][0], glmView[1][0], glmView[2][0]);
        Vec3 up(glmView[0][1], glmView[1][1], glmView[2][1]);
        Vec3 forward = Cross(right, up).Normalized();

        right = right * scale.x;
        up = up * scale.y;
        forward = forward * scale.z;

        glm::mat4 billboard(1.0f);
        billboard[0] = glm::vec4(right.x, right.y, right.z, 0.0f);
        billboard[1] = glm::vec4(up.x, up.y, up.z, 0.0f);
        billboard[2] = glm::vec4(forward.x, forward.y, forward.z, 0.0f);
        billboard[3] = glm::vec4(position.x, position.y, position.z, 1.0f);

        return Mat4(billboard);
    } else if (mode == Panel3DBillboardMode::YAxisOnly) {

        glm::mat4 glmView = viewMatrix.ToGLM();
        glm::mat4 invView = glm::inverse(glmView);
        Vec3 cameraPos(invView[3][0], invView[3][1], invView[3][2]);

        Vec3 toCamera = (cameraPos - position);
        toCamera.y = 0.0f;
        toCamera = toCamera.Normalized();

        Vec3 up(0, 1, 0);
        Vec3 right = Cross(up, toCamera).Normalized();
        Vec3 forward = toCamera;

        right = right * scale.x;
        up = up * scale.y;
        forward = forward * scale.z;

        glm::mat4 billboard(1.0f);
        billboard[0] = glm::vec4(right.x, right.y, right.z, 0.0f);
        billboard[1] = glm::vec4(up.x, up.y, up.z, 0.0f);
        billboard[2] = glm::vec4(forward.x, forward.y, forward.z, 0.0f);
        billboard[3] = glm::vec4(position.x, position.y, position.z, 1.0f);

        return Mat4(billboard);
    }

    return Mat4::Identity();
}

void Panel3D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled()) {
        return;
    }

    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    m_CornerRadius.FromVec4(cornerRadiusVec);
    m_CornerRadius.SetLinked(cornerRadiusLinked);

    Vec4 borderWidthVec = GetPropertyValue<Vec4>("borderWidth");
    bool borderWidthLinked = GetPropertyValue<bool>("borderWidthLinked");
    m_BorderWidth.FromVec4(borderWidthVec);
    m_BorderWidth.SetLinked(borderWidthLinked);

    Node* owner = GetOwner();
    if (!owner) return;

    Node3D* node3D = dynamic_cast<Node3D*>(owner);
    if (!node3D) return;

    Vec2 size = Vec2(GetWidth(), GetHeight());

    Panel3DBillboardMode billboardMode = GetBillboardMode();

    Mat4 transform;

    if (billboardMode != Panel3DBillboardMode::Disabled) {

        transform = CalculateBillboardTransform(ctx.getViewMatrix());
    } else {

        transform = node3D->GetGlobalTransformMatrix();
    }

    bool doubleSided = GetDoubleSided();

    if (doubleSided) {

        RenderBorder(ctx, transform, size, true);
        RenderFill(ctx, transform, size, true);

        RenderFill(ctx, transform, size, false);
        RenderBorder(ctx, transform, size, false);
    } else {

        RenderBorder(ctx, transform, size, false);
        RenderFill(ctx, transform, size, false);
    }
}

void Panel3D::RenderFill(RenderContext& ctx, const Mat4& transform, const Vec2& size, bool isBackFace) {
    Color fillColor = GetBackgroundColor();
    // Debug: log fillColor right after getting it
    static int fillColorDebugCount = 0;
    if (fillColorDebugCount < 10) {
        
        fillColorDebugCount++;
    }
    fillColor.a *= GetOpacity();

    if (fillColor.a <= 0.0f) return;

    MeshHandle quadMesh = ctx.getOrCreateQuadMesh();
    if (!quadMesh.isValid()) {
        return;
    }

    float zOffset = isBackFace ? -0.001f : 0.001f;

    Mat4 scale = Mat4::Scale(Vec3(size.x, size.y, 1.0f));
    Mat4 finalTransform = transform * scale * Mat4::Translate(Vec3(0, 0, zOffset));

    Vec4 cornerRadius = m_CornerRadius.AsVec4();
    bool hasRoundedCorners = (cornerRadius.x > 0.0f || cornerRadius.y > 0.0f ||
                              cornerRadius.z > 0.0f || cornerRadius.w > 0.0f);

    const float pixelScale = 20.0f;
    Vec2 scaledSize = size * pixelScale;
    Vec4 scaledCornerRadius = cornerRadius * pixelScale;

    MaterialPropertyBlock overrides;
    overrides.setColor("u_Color", fillColor);
    overrides.setVec2("u_Size", scaledSize);
    overrides.setVec4("u_CornerRadius", scaledCornerRadius);
    overrides.setBool("u_UseTexture", false);

    MaterialHandle material;
    if (hasRoundedCorners) {
        material = ctx.getRoundedRect3DMaterial();
    } else {
        material = GetDoubleSided()
            ? ctx.getDefaultColoredDoubleSidedMaterial()
            : ctx.getDefaultColoredMaterial();
        overrides.setColor("u_TintColor", fillColor);
    }

    ctx.drawMesh(quadMesh, material, finalTransform, overrides, 0, GetCastShadow(), GetReceiveShadow());
}

void Panel3D::RenderBorder(RenderContext& ctx, const Mat4& transform, const Vec2& size, bool isBackFace) {
    if (!GetBorderEnabled()) {
        return;
    }

    float borderTop = m_BorderWidth.Get(0);
    float borderRight = m_BorderWidth.Get(1);
    float borderBottom = m_BorderWidth.Get(2);
    float borderLeft = m_BorderWidth.Get(3);

    if (borderTop <= 0.0f && borderRight <= 0.0f && borderBottom <= 0.0f && borderLeft <= 0.0f) {
        return;
    }

    Color borderColor = GetBorderColor();

    Vec4 cornerRadius = m_CornerRadius.AsVec4();
    bool hasRoundedCorners = (cornerRadius.x > 0.0f || cornerRadius.y > 0.0f ||
                              cornerRadius.z > 0.0f || cornerRadius.w > 0.0f);

    MeshHandle quadMesh = ctx.getOrCreateQuadMesh();
    if (!quadMesh.isValid()) {
        return;
    }

    if (hasRoundedCorners) {

        Vec2 outerSize(size.x + borderLeft + borderRight, size.y + borderTop + borderBottom);

        float zOffset = isBackFace ? 0.0f : 0.0f;

        Mat4 scale = Mat4::Scale(Vec3(outerSize.x, outerSize.y, 1.0f));
        Mat4 finalTransform = transform * scale * Mat4::Translate(Vec3(0, 0, zOffset));

        const float pixelScale = 20.0f;
        Vec2 scaledOuterSize = outerSize * pixelScale;

        Vec4 outerCornerRadius = Vec4(
            cornerRadius.x + (borderTop + borderLeft) * 0.5f,
            cornerRadius.y + (borderTop + borderRight) * 0.5f,
            cornerRadius.z + (borderBottom + borderRight) * 0.5f,
            cornerRadius.w + (borderBottom + borderLeft) * 0.5f
        );

        Vec4 scaledOuterCornerRadius = outerCornerRadius * pixelScale;
        Vec4 scaledBorderWidth = Vec4(borderTop, borderRight, borderBottom, borderLeft) * pixelScale;

        MaterialPropertyBlock overrides;
        overrides.setColor("u_Color", borderColor);
        overrides.setVec2("u_Size", scaledOuterSize);
        overrides.setVec4("u_CornerRadius", scaledOuterCornerRadius);
        overrides.setVec4("u_BorderWidth", scaledBorderWidth);
        overrides.setBool("u_EnableAntialiasing", false);

        MaterialHandle material = ctx.getRoundedRect3DBorderMaterial();
        ctx.drawMesh(quadMesh, material, finalTransform, overrides, 0, GetCastShadow(), GetReceiveShadow());
    } else {

        float zOffset = isBackFace ? 0.0f : 0.0f;

        MaterialPropertyBlock overrides;
        overrides.setColor("u_TintColor", borderColor);
        overrides.setBool("u_UseTexture", false);

        MaterialHandle material = GetDoubleSided()
            ? ctx.getDefaultColoredDoubleSidedMaterial()
            : ctx.getDefaultColoredMaterial();

        if (borderTop > 0.0f) {
            Mat4 topTransform = transform
                * Mat4::Translate(Vec3(0.0f, size.y * 0.5f + borderTop * 0.5f, zOffset))
                * Mat4::Scale(Vec3(size.x + borderLeft + borderRight, borderTop, 1.0f));
            ctx.drawMesh(quadMesh, material, topTransform, overrides, 0, GetCastShadow(), GetReceiveShadow());
        }

        if (borderBottom > 0.0f) {
            Mat4 bottomTransform = transform
                * Mat4::Translate(Vec3(0.0f, -size.y * 0.5f - borderBottom * 0.5f, zOffset))
                * Mat4::Scale(Vec3(size.x + borderLeft + borderRight, borderBottom, 1.0f));
            ctx.drawMesh(quadMesh, material, bottomTransform, overrides, 0, GetCastShadow(), GetReceiveShadow());
        }

        if (borderLeft > 0.0f) {
            Mat4 leftTransform = transform
                * Mat4::Translate(Vec3(-size.x * 0.5f - borderLeft * 0.5f, 0.0f, zOffset))
                * Mat4::Scale(Vec3(borderLeft, size.y, 1.0f));
            ctx.drawMesh(quadMesh, material, leftTransform, overrides, 0, GetCastShadow(), GetReceiveShadow());
        }

        if (borderRight > 0.0f) {
            Mat4 rightTransform = transform
                * Mat4::Translate(Vec3(size.x * 0.5f + borderRight * 0.5f, 0.0f, zOffset))
                * Mat4::Scale(Vec3(borderRight, size.y, 1.0f));
            ctx.drawMesh(quadMesh, material, rightTransform, overrides, 0, GetCastShadow(), GetReceiveShadow());
        }
    }
}

AABB Panel3D::getWorldBounds() const {
    if (!GetOwner()) {
        return AABB();
    }

    float width = GetWidth();
    float height = GetHeight();

    Node3D* node3d = dynamic_cast<Node3D*>(GetOwner());
    if (!node3d) {
        return AABB();
    }

    Vec3 globalPos = node3d->GetGlobalPosition();
    Vec3 globalScale = node3d->GetGlobalScale();

    Vec2 size(width * globalScale.x, height * globalScale.y);

    Panel3DBillboardMode billboardMode = GetBillboardMode();
    if (billboardMode != Panel3DBillboardMode::Disabled) {
        Vec3 halfSize(size.x * 0.5f, size.y * 0.5f, 0.1f);
        return AABB(
            globalPos - halfSize,
            globalPos + halfSize
        );
    }

    Quat rotation = node3d->GetGlobalRotation();
    Vec3 halfSize(size.x * 0.5f, size.y * 0.5f, 0.1f);

    Vec3 corners[8] = {
        rotation * Vec3(-halfSize.x, -halfSize.y, -halfSize.z),
        rotation * Vec3( halfSize.x, -halfSize.y, -halfSize.z),
        rotation * Vec3( halfSize.x,  halfSize.y, -halfSize.z),
        rotation * Vec3(-halfSize.x,  halfSize.y, -halfSize.z),
        rotation * Vec3(-halfSize.x, -halfSize.y,  halfSize.z),
        rotation * Vec3( halfSize.x, -halfSize.y,  halfSize.z),
        rotation * Vec3( halfSize.x,  halfSize.y,  halfSize.z),
        rotation * Vec3(-halfSize.x,  halfSize.y,  halfSize.z)
    };

    Vec3 min(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (int i = 0; i < 8; ++i) {
        Vec3 worldCorner = globalPos + corners[i];
        min.x = std::min(min.x, worldCorner.x);
        min.y = std::min(min.y, worldCorner.y);
        min.z = std::min(min.z, worldCorner.z);
        max.x = std::max(max.x, worldCorner.x);
        max.y = std::max(max.y, worldCorner.y);
        max.z = std::max(max.z, worldCorner.z);
    }

    return AABB(min, max);
}

math::OBB Panel3D::getOrientedBounds() const {
    if (!GetOwner()) {
        return math::OBB();
    }

    Node3D* node3d = dynamic_cast<Node3D*>(GetOwner());
    if (!node3d) {
        return math::OBB();
    }

    Vec3 position = node3d->GetGlobalPosition();
    Vec2 size(GetWidth(), GetHeight());
    Vec3 globalScale = node3d->GetGlobalScale();
    Quat rotation = node3d->GetGlobalRotation();

    Panel3DBillboardMode billboardMode = GetBillboardMode();
    if (billboardMode != Panel3DBillboardMode::Disabled) {
        return math::OBB::FromAABB(getWorldBounds());
    }

    size.x *= globalScale.x;
    size.y *= globalScale.y;

    Vec3 center = position;
    Vec3 extents = Vec3(size.x * 0.5f, size.y * 0.5f, 0.1f);

    return math::OBB(center, extents, rotation);
}

bool Panel3D::IntersectRay(const math::Ray& ray, float& outDistance) const {
    math::OBB obb = getOrientedBounds();

    Vec3 localRayOrigin = obb.rotation.Inverse() * (ray.origin - obb.center);
    Vec3 localRayDir = obb.rotation.Inverse() * ray.direction;

    AABB localAABB(
        Vec3(-obb.extents.x, -obb.extents.y, -obb.extents.z),
        Vec3(obb.extents.x, obb.extents.y, obb.extents.z)
    );

    math::Ray localRay(localRayOrigin, localRayDir);
    return localRay.IntersectAABB(localAABB, outDistance);
}

RenderLayer Panel3D::getRenderLayer() const {

    float opacity = GetOpacity();
    if (opacity < 1.0f) {
        return RenderLayer::Transparent;
    }
    return RenderLayer::Opaque;
}

void Panel3D::EnsureStyleBox() {
    if (!m_StyleBox) {
        m_StyleBox = std::make_shared<StyleBoxFlat>();
    }
}

std::shared_ptr<StyleBoxFlat> Panel3D::GetStyleBoxFlat() const {
    return std::dynamic_pointer_cast<StyleBoxFlat>(m_StyleBox);
}

}
}

