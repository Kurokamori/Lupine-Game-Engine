#include "lupine/components/OmniLight3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/debug/DebugDraw.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

OmniLight3D::OmniLight3D()
    : Component("OmniLight3D")
{
}

OmniLight3D::OmniLight3D(const std::string& name)
    : Component(name)
{
}

OmniLight3D::~OmniLight3D() {
}

void OmniLight3D::DefineProperties() {

    DefineProperty(PROPERTY_DEFAULT(color, Color, Color::White()));

    DefineProperty(PROPERTY_FLOAT_RANGE(intensity, 1.0f, 0.0f, 100.0f, 0.1f));

    DefineProperty(PROPERTY_FLOAT_RANGE(range, 10.0f, 0.1f, 1000.0f, 0.5f));

    DefineProperty(PROPERTY_FLOAT_RANGE(attenuation, 2.0f, 0.0f, 4.0f, 0.1f));

    DefineProperty(PROPERTY_DEFAULT(negative, Bool, false));

    DefineProperty(PROPERTY_DEFAULT(castShadows, Bool, false));
    DefineProperty(PROPERTY_FLOAT_RANGE(shadowOpacity, 1.0f, 0.0f, 1.0f, 0.01f));
    DefineProperty(PROPERTY_FLOAT_RANGE(shadowBlur, 1.0f, 0.0f, 10.0f, 0.1f));
    DefineProperty(PROPERTY_FLOAT_RANGE(shadowBias, 0.001f, 0.0f, 0.1f, 0.0001f));
    DefineProperty(PROPERTY_INT_RANGE(shadowResolution, 1024, 256, 4096, 256));
}

void OmniLight3D::OnAwake() {
}

void OmniLight3D::OnReady() {
}

void OmniLight3D::OnRender() {

    DrawDebugVisualization();
}

Color OmniLight3D::GetColor() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("color");
    if (prop) {
        return prop->GetValue<Color>();
    }
    return Color::White();
}

void OmniLight3D::SetColor(const Color& color) {
    SetPropertyValue<Color>("color", color);
}

float OmniLight3D::GetIntensity() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("intensity");
    return prop ? prop->GetValue<float>() : 1.0f;
}

void OmniLight3D::SetIntensity(float intensity) {
    SetPropertyValue<float>("intensity", intensity);
}

float OmniLight3D::GetRange() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("range");
    return prop ? prop->GetValue<float>() : 10.0f;
}

void OmniLight3D::SetRange(float range) {
    SetPropertyValue<float>("range", range);
}

float OmniLight3D::GetAttenuation() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("attenuation");
    return prop ? prop->GetValue<float>() : 2.0f;
}

void OmniLight3D::SetAttenuation(float attenuation) {
    SetPropertyValue<float>("attenuation", attenuation);
}

bool OmniLight3D::IsNegative() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("negative");
    return prop ? prop->GetValue<bool>() : false;
}

void OmniLight3D::SetNegative(bool negative) {
    SetPropertyValue<bool>("negative", negative);
}

bool OmniLight3D::CastsShadows() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("castShadows");
    return prop ? prop->GetValue<bool>() : true;
}

void OmniLight3D::SetCastsShadows(bool castShadows) {
    SetPropertyValue<bool>("castShadows", castShadows);
}

float OmniLight3D::GetShadowOpacity() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("shadowOpacity");
    return prop ? prop->GetValue<float>() : 1.0f;
}

void OmniLight3D::SetShadowOpacity(float opacity) {
    SetPropertyValue<float>("shadowOpacity", opacity);
}

float OmniLight3D::GetShadowBlur() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("shadowBlur");
    return prop ? prop->GetValue<float>() : 1.0f;
}

void OmniLight3D::SetShadowBlur(float blur) {
    SetPropertyValue<float>("shadowBlur", blur);
}

float OmniLight3D::GetShadowBias() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("shadowBias");
    return prop ? prop->GetValue<float>() : 0.001f;
}

void OmniLight3D::SetShadowBias(float bias) {
    SetPropertyValue<float>("shadowBias", bias);
}

int OmniLight3D::GetShadowResolution() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("shadowResolution");
    return prop ? prop->GetValue<int>() : 1024;
}

void OmniLight3D::SetShadowResolution(int resolution) {
    SetPropertyValue<int>("shadowResolution", resolution);
}

LightDescriptor OmniLight3D::ToLightDescriptor() const {
    LightDescriptor light;
    light.type = LightType::Point;

    Node* owner = GetOwner();
    if (owner) {
        Node3D* node3D = dynamic_cast<Node3D*>(owner);
        if (node3D) {
            light.position = node3D->GetGlobalPosition();
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

    light.castShadows = CastsShadows();
    light.shadowBias = GetShadowBias();
    light.shadowBlur = GetShadowBlur();
    light.shadowOpacity = GetShadowOpacity();
    light.shadowResolution = GetShadowResolution();

    return light;
}

void OmniLight3D::DrawDebugVisualization() {

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
    float range = GetRange();
    bool negative = IsNegative();

    Color debugColor = negative ? Color(0.2f, 0.8f, 1.0f, 1.0f) : Color(1.0f, 0.6f, 0.2f, 1.0f);

    float bulbSize = 0.15f;
    DebugDraw::Sphere(position, bulbSize, debugColor);

    float visualRange = std::min(range, 5.0f);
    DebugDraw::Sphere(position, visualRange, debugColor);

    DebugDraw::Line(position + Vec3(-bulbSize, 0, 0), position + Vec3(bulbSize, 0, 0), debugColor);
    DebugDraw::Line(position + Vec3(0, -bulbSize, 0), position + Vec3(0, bulbSize, 0), debugColor);
    DebugDraw::Line(position + Vec3(0, 0, -bulbSize), position + Vec3(0, 0, bulbSize), debugColor);

    DebugDraw::Axes(position, 0.2f);
}

}
}

