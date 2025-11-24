#include "lupine/components/SpotLight3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/debug/DebugDraw.hpp"
#include "lupine/logger/Logger.hpp"
#include <cmath>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

SpotLight3D::SpotLight3D()
    : Component("SpotLight3D")
{
}

SpotLight3D::SpotLight3D(const std::string& name)
    : Component(name)
{
}

SpotLight3D::~SpotLight3D() {
}

void SpotLight3D::DefineProperties() {

    DefineProperty(PROPERTY_DEFAULT(color, Color, Color::White()));

    DefineProperty(PROPERTY_FLOAT_RANGE(intensity, 1.0f, 0.0f, 100.0f, 0.1f));

    DefineProperty(PROPERTY_FLOAT_RANGE(range, 10.0f, 0.1f, 1000.0f, 0.5f));

    DefineProperty(PROPERTY_FLOAT_RANGE(attenuation, 2.0f, 0.0f, 4.0f, 0.1f));

    DefineProperty(PROPERTY_FLOAT_RANGE(innerConeAngle, 30.0f, 0.0f, 90.0f, 1.0f));
    DefineProperty(PROPERTY_FLOAT_RANGE(outerConeAngle, 45.0f, 0.0f, 90.0f, 1.0f));

    DefineProperty(PROPERTY_DEFAULT(negative, Bool, false));

    DefineProperty(PROPERTY_DEFAULT(castShadows, Bool, true));
    DefineProperty(PROPERTY_FLOAT_RANGE(shadowOpacity, 1.0f, 0.0f, 1.0f, 0.01f));
    DefineProperty(PROPERTY_FLOAT_RANGE(shadowBlur, 1.0f, 0.0f, 10.0f, 0.1f));
    DefineProperty(PROPERTY_FLOAT_RANGE(shadowBias, 0.001f, 0.0f, 0.1f, 0.0001f));
    DefineProperty(PROPERTY_INT_RANGE(shadowResolution, 1024, 256, 4096, 256));
}

void SpotLight3D::OnAwake() {
}

void SpotLight3D::OnReady() {
}

void SpotLight3D::OnRender() {
    DrawDebugVisualization();
}

Color SpotLight3D::GetColor() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("color");
    if (prop) {
        return prop->GetValue<Color>();
    }
    return Color::White();
}

void SpotLight3D::SetColor(const Color& color) {
    SetPropertyValue<Color>("color", color);
}

float SpotLight3D::GetIntensity() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("intensity");
    return prop ? prop->GetValue<float>() : 1.0f;
}

void SpotLight3D::SetIntensity(float intensity) {
    SetPropertyValue<float>("intensity", intensity);
}

float SpotLight3D::GetRange() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("range");
    return prop ? prop->GetValue<float>() : 10.0f;
}

void SpotLight3D::SetRange(float range) {
    SetPropertyValue<float>("range", range);
}

float SpotLight3D::GetAttenuation() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("attenuation");
    return prop ? prop->GetValue<float>() : 2.0f;
}

void SpotLight3D::SetAttenuation(float attenuation) {
    SetPropertyValue<float>("attenuation", attenuation);
}

float SpotLight3D::GetInnerConeAngle() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("innerConeAngle");
    return prop ? prop->GetValue<float>() : 30.0f;
}

void SpotLight3D::SetInnerConeAngle(float angle) {
    SetPropertyValue<float>("innerConeAngle", angle);
}

float SpotLight3D::GetOuterConeAngle() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("outerConeAngle");
    return prop ? prop->GetValue<float>() : 45.0f;
}

void SpotLight3D::SetOuterConeAngle(float angle) {
    SetPropertyValue<float>("outerConeAngle", angle);
}

bool SpotLight3D::IsNegative() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("negative");
    return prop ? prop->GetValue<bool>() : false;
}

void SpotLight3D::SetNegative(bool negative) {
    SetPropertyValue<bool>("negative", negative);
}

bool SpotLight3D::CastsShadows() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("castShadows");
    return prop ? prop->GetValue<bool>() : true;
}

void SpotLight3D::SetCastsShadows(bool castShadows) {
    SetPropertyValue<bool>("castShadows", castShadows);
}

float SpotLight3D::GetShadowOpacity() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("shadowOpacity");
    return prop ? prop->GetValue<float>() : 1.0f;
}

void SpotLight3D::SetShadowOpacity(float opacity) {
    SetPropertyValue<float>("shadowOpacity", opacity);
}

float SpotLight3D::GetShadowBlur() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("shadowBlur");
    return prop ? prop->GetValue<float>() : 1.0f;
}

void SpotLight3D::SetShadowBlur(float blur) {
    SetPropertyValue<float>("shadowBlur", blur);
}

float SpotLight3D::GetShadowBias() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("shadowBias");
    return prop ? prop->GetValue<float>() : 0.001f;
}

void SpotLight3D::SetShadowBias(float bias) {
    SetPropertyValue<float>("shadowBias", bias);
}

int SpotLight3D::GetShadowResolution() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("shadowResolution");
    return prop ? prop->GetValue<int>() : 1024;
}

void SpotLight3D::SetShadowResolution(int resolution) {
    SetPropertyValue<int>("shadowResolution", resolution);
}

LightDescriptor SpotLight3D::ToLightDescriptor() const {
    LightDescriptor light;
    light.type = LightType::Spot;

    Node* owner = GetOwner();
    if (owner) {
        Node3D* node3D = dynamic_cast<Node3D*>(owner);
        if (node3D) {
            light.position = node3D->GetGlobalPosition();

            Quat globalRotation = node3D->GetGlobalRotation();
            light.direction = globalRotation.GetForward();
            light.up = globalRotation.GetUp();
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

    light.range = GetRange();
    light.attenuation = GetAttenuation();

    light.innerConeAngle = GetInnerConeAngle() * DEG_TO_RAD;
    light.outerConeAngle = GetOuterConeAngle() * DEG_TO_RAD;

    light.castShadows = CastsShadows();
    light.shadowBias = GetShadowBias();
    light.shadowBlur = GetShadowBlur();
    light.shadowOpacity = GetShadowOpacity();
    light.shadowResolution = GetShadowResolution();

    return light;
}

void SpotLight3D::DrawDebugVisualization() {

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
    float range = GetRange();
    float outerAngle = GetOuterConeAngle();
    float innerAngle = GetInnerConeAngle();
    bool negative = IsNegative();

    Color debugColor = negative ? Color(1.0f, 0.2f, 0.8f, 1.0f) : Color(0.3f, 1.0f, 0.3f, 1.0f);
    Color innerConeColor = debugColor * 0.6f;

    Vec3 direction = rotation.GetForward();

    float bulbSize = 0.1f;
    DebugDraw::Sphere(position, bulbSize, debugColor);

    float visualRange = std::min(range, 5.0f);
    float outerConeRadius = visualRange * std::tan(outerAngle);
    Vec3 coneEnd = position + direction * visualRange;

    Vec3 up = std::abs(direction.y) < 0.9f ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(1.0f, 0.0f, 0.0f);
    Vec3 right = math::Cross(up, direction).Normalized();
    up = math::Cross(direction, right).Normalized();

    int segments = 12;
    for (int i = 0; i < segments; ++i) {
        float angle = (float)i * (3.14159f * 2.0f / (float)segments);
        float nextAngle = (float)(i + 1) * (3.14159f * 2.0f / (float)segments);

        Vec3 offset1 = (right * std::cos(angle) + up * std::sin(angle)) * outerConeRadius;
        Vec3 offset2 = (right * std::cos(nextAngle) + up * std::sin(nextAngle)) * outerConeRadius;

        Vec3 point1 = coneEnd + offset1;
        Vec3 point2 = coneEnd + offset2;

        DebugDraw::Line(position, point1, debugColor);

        DebugDraw::Line(point1, point2, debugColor);
    }

    if (innerAngle < outerAngle - 0.01f) {
        float innerConeRadius = visualRange * std::tan(innerAngle);
        for (int i = 0; i < segments; i += 2) {
            float angle = (float)i * (3.14159f * 2.0f / (float)segments);
            Vec3 offset = (right * std::cos(angle) + up * std::sin(angle)) * innerConeRadius;
            Vec3 point = coneEnd + offset;
            DebugDraw::Line(position, point, innerConeColor);
        }
    }

    DebugDraw::Arrow(position, position + direction * (visualRange * 0.5f), debugColor, 0.15f);

    DebugDraw::Axes(position, 0.2f);
}

}
}

