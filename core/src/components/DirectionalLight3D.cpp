#include "lupine/components/DirectionalLight3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/debug/DebugDraw.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

DirectionalLight3D::DirectionalLight3D()
    : Component("DirectionalLight3D")
{
}

DirectionalLight3D::DirectionalLight3D(const std::string& name)
    : Component(name)
{
}

DirectionalLight3D::~DirectionalLight3D() {
}

void DirectionalLight3D::DefineProperties() {

    DefineProperty(PROPERTY_DEFAULT(color, Color, Color::White()));

    DefineProperty(PROPERTY_FLOAT_RANGE(intensity, 1.0f, 0.0f, 10.0f, 0.1f));

    DefineProperty(PROPERTY_DEFAULT(negative, Bool, false));

    DefineProperty(PROPERTY_DEFAULT(castShadows, Bool, true));
    DefineProperty(PROPERTY_FLOAT_RANGE(shadowOpacity, 1.0f, 0.0f, 1.0f, 0.01f));
    DefineProperty(PROPERTY_FLOAT_RANGE(shadowBlur, 1.0f, 0.0f, 10.0f, 0.1f));
    DefineProperty(PROPERTY_FLOAT_RANGE(shadowBias, 0.0005f, 0.0f, 0.01f, 0.00005f));
    DefineProperty(PROPERTY_INT_RANGE(shadowResolution, 2048, 256, 8192, 256));
    DefineProperty(PROPERTY_INT_RANGE(shadowCascades, 4, 1, 8, 1));
}

void DirectionalLight3D::OnAwake() {
}

void DirectionalLight3D::OnReady() {
}

void DirectionalLight3D::OnRender() {
    DrawDebugVisualization();
}

Color DirectionalLight3D::GetColor() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("color");
    if (prop) {
        return prop->GetValue<Color>();
    }
    return Color::White();
}

void DirectionalLight3D::SetColor(const Color& color) {
    SetPropertyValue<Color>("color", color);
}

float DirectionalLight3D::GetIntensity() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("intensity");
    return prop ? prop->GetValue<float>() : 1.0f;
}

void DirectionalLight3D::SetIntensity(float intensity) {
    SetPropertyValue<float>("intensity", intensity);
}

bool DirectionalLight3D::IsNegative() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("negative");
    return prop ? prop->GetValue<bool>() : false;
}

void DirectionalLight3D::SetNegative(bool negative) {
    SetPropertyValue<bool>("negative", negative);
}

bool DirectionalLight3D::CastsShadows() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("castShadows");
    return prop ? prop->GetValue<bool>() : true;
}

void DirectionalLight3D::SetCastsShadows(bool castShadows) {
    SetPropertyValue<bool>("castShadows", castShadows);
}

float DirectionalLight3D::GetShadowOpacity() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("shadowOpacity");
    return prop ? prop->GetValue<float>() : 1.0f;
}

void DirectionalLight3D::SetShadowOpacity(float opacity) {
    SetPropertyValue<float>("shadowOpacity", opacity);
}

float DirectionalLight3D::GetShadowBlur() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("shadowBlur");
    return prop ? prop->GetValue<float>() : 1.0f;
}

void DirectionalLight3D::SetShadowBlur(float blur) {
    SetPropertyValue<float>("shadowBlur", blur);
}

float DirectionalLight3D::GetShadowBias() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("shadowBias");
    return prop ? prop->GetValue<float>() : 0.001f;
}

void DirectionalLight3D::SetShadowBias(float bias) {
    SetPropertyValue<float>("shadowBias", bias);
}

int DirectionalLight3D::GetShadowResolution() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("shadowResolution");
    return prop ? prop->GetValue<int>() : 2048;
}

void DirectionalLight3D::SetShadowResolution(int resolution) {
    SetPropertyValue<int>("shadowResolution", resolution);
}

int DirectionalLight3D::GetShadowCascades() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("shadowCascades");
    return prop ? prop->GetValue<int>() : 4;
}

void DirectionalLight3D::SetShadowCascades(int cascades) {
    SetPropertyValue<int>("shadowCascades", cascades);
}

LightDescriptor DirectionalLight3D::ToLightDescriptor() const {
    LightDescriptor light;
    light.type = LightType::Directional;

    Node* owner = GetOwner();
    if (owner) {
        Node3D* node3D = dynamic_cast<Node3D*>(owner);
        if (node3D) {

            Quat globalRotation = node3D->GetGlobalRotation();
            light.direction = globalRotation.GetForward();
        }
    }

    Color color = GetColor();
    float intensity = GetIntensity();
    bool negative = IsNegative();

    if (negative) {
        intensity = -intensity;
    }

    light.color = color;
    light.intensity = intensity;

    light.castShadows = CastsShadows();
    light.shadowBias = GetShadowBias();
    light.shadowBlur = GetShadowBlur();
    light.shadowOpacity = GetShadowOpacity();
    light.shadowResolution = GetShadowResolution();
    light.shadowCascades = static_cast<uint32_t>(GetShadowCascades());
    light.cascadeSplitLambda = 0.5f;

    light.range = 0.0f;

    return light;
}

void DirectionalLight3D::DrawDebugVisualization() {

    if (!DebugDraw::IsAvailable()) {
        return;
    }

    if (DebugDraw::GetCurrentCameraType() != CameraType::Camera3D) {
        return;
    }

    Node* owner = GetOwner();
    if (!owner) {
        return;
    }

    Node3D* node3D = dynamic_cast<Node3D*>(owner);
    if (!node3D) {
        return;
    }

    Vec3 position = node3D->GetGlobalPosition();
    Quat rotation = node3D->GetGlobalRotation();
    Color color = GetColor();
    bool negative = IsNegative();

    Color debugColor = negative ? Color(0.8f, 0.2f, 1.0f, 1.0f) : Color(1.0f, 1.0f, 0.3f, 1.0f);

    Vec3 direction = rotation.GetForward();

    float iconSize = 0.5f;

    Vec3 up = std::abs(direction.y) < 0.9f ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(1.0f, 0.0f, 0.0f);
    Vec3 right = math::Cross(up, direction).Normalized();
    up = math::Cross(direction, right).Normalized();

    DebugDraw::Circle(position, direction, iconSize, debugColor, 16);

    for (int i = 0; i < 8; ++i) {
        float angle = (float)i * (3.14159f * 2.0f / 8.0f);
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        Vec3 rayDir = right * cosA + up * sinA;
        Vec3 rayStart = position + rayDir * iconSize;
        Vec3 rayEnd = position + rayDir * (iconSize * 1.5f);

        DebugDraw::Line(rayStart, rayEnd, debugColor);
    }

    Vec3 arrowEnd = position + direction * 2.5f;
    DebugDraw::Arrow(position, arrowEnd, debugColor, 0.4f);

    DebugDraw::Axes(position, 0.4f);
}

}
}

