/**
 * @file LightComponentTests.cpp
 * @brief Headless console tests for the 3D lighting and environment components.
 *
 * DirectionalLight3D, OmniLight3D, SpotLight3D and WorldEnvironment all store
 * their configuration in the Component custom-property system. Every value a
 * project edits in the inspector flows through the typed setter/getter pairs and
 * is persisted via Component serialization. These tests construct each component,
 * define its properties and verify default values plus setter/getter round-trips
 * for the complete property surface (color, intensity, shadows, range,
 * attenuation, cone angles, fog, ambient lighting, skybox settings).
 *
 * GPU-facing entry points (ToLightDescriptor relies on an owning Node3D, and the
 * WorldEnvironment skybox upload paths need a live IGfxDevice) are intentionally
 * NOT exercised here: these tests cover the data/config layer that must be correct
 * independently of any render device.
 */

#include "lupine/engine/Engine.hpp"
#include "lupine/components/DirectionalLight3D.hpp"
#include "lupine/components/OmniLight3D.hpp"
#include "lupine/components/SpotLight3D.hpp"
#include "lupine/components/WorldEnvironment.hpp"
#include "lupine/math/Color.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <cmath>

using namespace lupine;
using namespace lupine::core;

namespace {

bool FloatEq(float a, float b, float tol = 1e-4f) {
    return std::fabs(a - b) < tol;
}

bool ColorEq(const math::Color& a, const math::Color& b, float tol = 1e-4f) {
    return FloatEq(a.r, b.r, tol) && FloatEq(a.g, b.g, tol) &&
           FloatEq(a.b, b.b, tol) && FloatEq(a.a, b.a, tol);
}

bool TestDirectionalLightDefaults() {
    TEST_SECTION("DirectionalLight3D: Default Values");

    std::shared_ptr<components::DirectionalLight3D> light =
        std::make_shared<components::DirectionalLight3D>();
    light->DefineProperties();

    TEST_ASSERT(light->GetTypeName() == "DirectionalLight3D", "Type name is DirectionalLight3D");
    TEST_ASSERT(ColorEq(light->GetColor(), math::Color::White()), "Default color is white");
    TEST_ASSERT(FloatEq(light->GetIntensity(), 1.0f), "Default intensity is 1.0");
    TEST_ASSERT(!light->IsNegative(), "Default negative mode is off");
    TEST_ASSERT(light->CastsShadows(), "Directional light casts shadows by default");
    TEST_ASSERT(FloatEq(light->GetShadowOpacity(), 1.0f), "Default shadow opacity is 1.0");
    TEST_ASSERT(FloatEq(light->GetShadowBlur(), 1.0f), "Default shadow blur is 1.0");
    TEST_ASSERT(FloatEq(light->GetShadowBias(), 0.0005f), "Default shadow bias is 0.0005");
    TEST_ASSERT(light->GetShadowResolution() == 2048, "Default shadow resolution is 2048");
    TEST_ASSERT(light->GetShadowCascades() == 4, "Default shadow cascades is 4");

    return true;
}

bool TestDirectionalLightRoundTrip() {
    TEST_SECTION("DirectionalLight3D: Setter/Getter Round-Trips");

    std::shared_ptr<components::DirectionalLight3D> light =
        std::make_shared<components::DirectionalLight3D>();
    light->DefineProperties();

    math::Color color(0.25f, 0.5f, 0.75f, 0.9f);
    light->SetColor(color);
    TEST_ASSERT(ColorEq(light->GetColor(), color), "Color round-trips");
    TEST_ASSERT(FloatEq(light->GetColor().r, 0.25f), "Color red channel preserved");
    TEST_ASSERT(FloatEq(light->GetColor().g, 0.5f), "Color green channel preserved");
    TEST_ASSERT(FloatEq(light->GetColor().b, 0.75f), "Color blue channel preserved");
    TEST_ASSERT(FloatEq(light->GetColor().a, 0.9f), "Color alpha channel preserved");

    light->SetIntensity(5.5f);
    TEST_ASSERT(FloatEq(light->GetIntensity(), 5.5f), "Intensity round-trips");
    light->SetIntensity(0.0f);
    TEST_ASSERT(FloatEq(light->GetIntensity(), 0.0f), "Minimum intensity round-trips");
    light->SetIntensity(10.0f);
    TEST_ASSERT(FloatEq(light->GetIntensity(), 10.0f), "Maximum intensity round-trips");

    light->SetNegative(true);
    TEST_ASSERT(light->IsNegative(), "Negative mode enables");
    light->SetNegative(false);
    TEST_ASSERT(!light->IsNegative(), "Negative mode disables");

    light->SetCastsShadows(false);
    TEST_ASSERT(!light->CastsShadows(), "Shadow casting disables");
    light->SetCastsShadows(true);
    TEST_ASSERT(light->CastsShadows(), "Shadow casting enables");

    light->SetShadowOpacity(0.35f);
    TEST_ASSERT(FloatEq(light->GetShadowOpacity(), 0.35f), "Shadow opacity round-trips");
    light->SetShadowOpacity(0.0f);
    TEST_ASSERT(FloatEq(light->GetShadowOpacity(), 0.0f), "Minimum shadow opacity round-trips");

    light->SetShadowBlur(4.2f);
    TEST_ASSERT(FloatEq(light->GetShadowBlur(), 4.2f), "Shadow blur round-trips");

    light->SetShadowBias(0.0042f);
    TEST_ASSERT(FloatEq(light->GetShadowBias(), 0.0042f), "Shadow bias round-trips");

    light->SetShadowResolution(4096);
    TEST_ASSERT(light->GetShadowResolution() == 4096, "Shadow resolution round-trips");
    light->SetShadowResolution(256);
    TEST_ASSERT(light->GetShadowResolution() == 256, "Minimum shadow resolution round-trips");

    light->SetShadowCascades(8);
    TEST_ASSERT(light->GetShadowCascades() == 8, "Shadow cascades round-trips");
    light->SetShadowCascades(1);
    TEST_ASSERT(light->GetShadowCascades() == 1, "Minimum shadow cascades round-trips");

    return true;
}

bool TestOmniLightDefaults() {
    TEST_SECTION("OmniLight3D: Default Values");

    std::shared_ptr<components::OmniLight3D> light =
        std::make_shared<components::OmniLight3D>();
    light->DefineProperties();

    TEST_ASSERT(light->GetTypeName() == "OmniLight3D", "Type name is OmniLight3D");
    TEST_ASSERT(ColorEq(light->GetColor(), math::Color::White()), "Default color is white");
    TEST_ASSERT(FloatEq(light->GetIntensity(), 1.0f), "Default intensity is 1.0");
    TEST_ASSERT(FloatEq(light->GetRange(), 10.0f), "Default range is 10.0");
    TEST_ASSERT(FloatEq(light->GetAttenuation(), 2.0f), "Default attenuation is 2.0");
    TEST_ASSERT(!light->IsNegative(), "Default negative mode is off");
    TEST_ASSERT(!light->CastsShadows(), "Omni light does not cast shadows by default");
    TEST_ASSERT(FloatEq(light->GetShadowOpacity(), 1.0f), "Default shadow opacity is 1.0");
    TEST_ASSERT(FloatEq(light->GetShadowBlur(), 1.0f), "Default shadow blur is 1.0");
    TEST_ASSERT(FloatEq(light->GetShadowBias(), 0.001f), "Default shadow bias is 0.001");
    TEST_ASSERT(light->GetShadowResolution() == 1024, "Default shadow resolution is 1024");

    return true;
}

bool TestOmniLightRoundTrip() {
    TEST_SECTION("OmniLight3D: Setter/Getter Round-Trips");

    std::shared_ptr<components::OmniLight3D> light =
        std::make_shared<components::OmniLight3D>();
    light->DefineProperties();

    math::Color color(0.9f, 0.1f, 0.2f, 1.0f);
    light->SetColor(color);
    TEST_ASSERT(ColorEq(light->GetColor(), color), "Color round-trips");
    TEST_ASSERT(FloatEq(light->GetColor().r, 0.9f), "Color red channel preserved");
    TEST_ASSERT(FloatEq(light->GetColor().g, 0.1f), "Color green channel preserved");
    TEST_ASSERT(FloatEq(light->GetColor().b, 0.2f), "Color blue channel preserved");

    light->SetIntensity(42.0f);
    TEST_ASSERT(FloatEq(light->GetIntensity(), 42.0f), "Intensity round-trips");
    light->SetIntensity(100.0f);
    TEST_ASSERT(FloatEq(light->GetIntensity(), 100.0f), "Maximum intensity round-trips");

    light->SetRange(250.0f);
    TEST_ASSERT(FloatEq(light->GetRange(), 250.0f), "Range round-trips");
    light->SetRange(0.1f);
    TEST_ASSERT(FloatEq(light->GetRange(), 0.1f), "Minimum range round-trips");
    light->SetRange(1000.0f);
    TEST_ASSERT(FloatEq(light->GetRange(), 1000.0f), "Maximum range round-trips");

    light->SetAttenuation(3.5f);
    TEST_ASSERT(FloatEq(light->GetAttenuation(), 3.5f), "Attenuation round-trips");
    light->SetAttenuation(0.0f);
    TEST_ASSERT(FloatEq(light->GetAttenuation(), 0.0f), "Minimum attenuation round-trips");
    light->SetAttenuation(4.0f);
    TEST_ASSERT(FloatEq(light->GetAttenuation(), 4.0f), "Maximum attenuation round-trips");

    light->SetNegative(true);
    TEST_ASSERT(light->IsNegative(), "Negative mode enables");
    light->SetNegative(false);
    TEST_ASSERT(!light->IsNegative(), "Negative mode disables");

    light->SetCastsShadows(true);
    TEST_ASSERT(light->CastsShadows(), "Shadow casting enables");
    light->SetCastsShadows(false);
    TEST_ASSERT(!light->CastsShadows(), "Shadow casting disables");

    light->SetShadowOpacity(0.6f);
    TEST_ASSERT(FloatEq(light->GetShadowOpacity(), 0.6f), "Shadow opacity round-trips");

    light->SetShadowBlur(7.5f);
    TEST_ASSERT(FloatEq(light->GetShadowBlur(), 7.5f), "Shadow blur round-trips");

    light->SetShadowBias(0.05f);
    TEST_ASSERT(FloatEq(light->GetShadowBias(), 0.05f), "Shadow bias round-trips");

    light->SetShadowResolution(2048);
    TEST_ASSERT(light->GetShadowResolution() == 2048, "Shadow resolution round-trips");
    light->SetShadowResolution(4096);
    TEST_ASSERT(light->GetShadowResolution() == 4096, "Maximum shadow resolution round-trips");

    return true;
}

bool TestSpotLightDefaults() {
    TEST_SECTION("SpotLight3D: Default Values");

    std::shared_ptr<components::SpotLight3D> light =
        std::make_shared<components::SpotLight3D>();
    light->DefineProperties();

    TEST_ASSERT(light->GetTypeName() == "SpotLight3D", "Type name is SpotLight3D");
    TEST_ASSERT(ColorEq(light->GetColor(), math::Color::White()), "Default color is white");
    TEST_ASSERT(FloatEq(light->GetIntensity(), 1.0f), "Default intensity is 1.0");
    TEST_ASSERT(FloatEq(light->GetRange(), 10.0f), "Default range is 10.0");
    TEST_ASSERT(FloatEq(light->GetAttenuation(), 2.0f), "Default attenuation is 2.0");
    TEST_ASSERT(FloatEq(light->GetInnerConeAngle(), 30.0f), "Default inner cone angle is 30");
    TEST_ASSERT(FloatEq(light->GetOuterConeAngle(), 45.0f), "Default outer cone angle is 45");
    TEST_ASSERT(!light->IsNegative(), "Default negative mode is off");
    TEST_ASSERT(light->CastsShadows(), "Spot light casts shadows by default");
    TEST_ASSERT(FloatEq(light->GetShadowOpacity(), 1.0f), "Default shadow opacity is 1.0");
    TEST_ASSERT(FloatEq(light->GetShadowBlur(), 1.0f), "Default shadow blur is 1.0");
    TEST_ASSERT(FloatEq(light->GetShadowBias(), 0.001f), "Default shadow bias is 0.001");
    TEST_ASSERT(light->GetShadowResolution() == 1024, "Default shadow resolution is 1024");

    return true;
}

bool TestSpotLightRoundTrip() {
    TEST_SECTION("SpotLight3D: Setter/Getter Round-Trips");

    std::shared_ptr<components::SpotLight3D> light =
        std::make_shared<components::SpotLight3D>();
    light->DefineProperties();

    math::Color color(0.3f, 0.6f, 0.4f, 0.5f);
    light->SetColor(color);
    TEST_ASSERT(ColorEq(light->GetColor(), color), "Color round-trips");
    TEST_ASSERT(FloatEq(light->GetColor().a, 0.5f), "Color alpha channel preserved");

    light->SetIntensity(12.0f);
    TEST_ASSERT(FloatEq(light->GetIntensity(), 12.0f), "Intensity round-trips");

    light->SetRange(75.0f);
    TEST_ASSERT(FloatEq(light->GetRange(), 75.0f), "Range round-trips");

    light->SetAttenuation(1.5f);
    TEST_ASSERT(FloatEq(light->GetAttenuation(), 1.5f), "Attenuation round-trips");

    light->SetInnerConeAngle(15.0f);
    TEST_ASSERT(FloatEq(light->GetInnerConeAngle(), 15.0f), "Inner cone angle round-trips");
    light->SetInnerConeAngle(0.0f);
    TEST_ASSERT(FloatEq(light->GetInnerConeAngle(), 0.0f), "Minimum inner cone angle round-trips");

    light->SetOuterConeAngle(60.0f);
    TEST_ASSERT(FloatEq(light->GetOuterConeAngle(), 60.0f), "Outer cone angle round-trips");
    light->SetOuterConeAngle(90.0f);
    TEST_ASSERT(FloatEq(light->GetOuterConeAngle(), 90.0f), "Maximum outer cone angle round-trips");

    light->SetNegative(true);
    TEST_ASSERT(light->IsNegative(), "Negative mode enables");
    light->SetNegative(false);
    TEST_ASSERT(!light->IsNegative(), "Negative mode disables");

    light->SetCastsShadows(false);
    TEST_ASSERT(!light->CastsShadows(), "Shadow casting disables");
    light->SetCastsShadows(true);
    TEST_ASSERT(light->CastsShadows(), "Shadow casting enables");

    light->SetShadowOpacity(0.8f);
    TEST_ASSERT(FloatEq(light->GetShadowOpacity(), 0.8f), "Shadow opacity round-trips");

    light->SetShadowBlur(2.0f);
    TEST_ASSERT(FloatEq(light->GetShadowBlur(), 2.0f), "Shadow blur round-trips");

    light->SetShadowBias(0.02f);
    TEST_ASSERT(FloatEq(light->GetShadowBias(), 0.02f), "Shadow bias round-trips");

    light->SetShadowResolution(256);
    TEST_ASSERT(light->GetShadowResolution() == 256, "Minimum shadow resolution round-trips");
    light->SetShadowResolution(4096);
    TEST_ASSERT(light->GetShadowResolution() == 4096, "Maximum shadow resolution round-trips");

    return true;
}

bool TestWorldEnvironmentDefaults() {
    TEST_SECTION("WorldEnvironment: Default Values");

    std::shared_ptr<components::WorldEnvironment> env =
        std::make_shared<components::WorldEnvironment>();
    env->DefineProperties();

    TEST_ASSERT(env->GetTypeName() == "WorldEnvironment", "Type name is WorldEnvironment");

    TEST_ASSERT(env->GetSkyboxType() == 0, "Default skybox type is None (0)");
    TEST_ASSERT(ColorEq(env->GetSkyboxColor(), math::Color(0.5f, 0.7f, 1.0f, 1.0f)),
                "Default skybox color matches");
    TEST_ASSERT(ColorEq(env->GetSkyTopColor(), math::Color(0.1f, 0.3f, 0.8f, 1.0f)),
                "Default sky top color matches");
    TEST_ASSERT(ColorEq(env->GetSkyHorizonColor(), math::Color(0.6f, 0.7f, 0.9f, 1.0f)),
                "Default sky horizon color matches");
    TEST_ASSERT(ColorEq(env->GetSkyBottomColor(), math::Color(0.8f, 0.8f, 0.8f, 1.0f)),
                "Default sky bottom color matches");

    TEST_ASSERT(env->GetCubemapPosX().empty(), "Default cubemap +X path is empty");
    TEST_ASSERT(env->GetCubemapNegX().empty(), "Default cubemap -X path is empty");
    TEST_ASSERT(env->GetCubemapPosY().empty(), "Default cubemap +Y path is empty");
    TEST_ASSERT(env->GetCubemapNegY().empty(), "Default cubemap -Y path is empty");
    TEST_ASSERT(env->GetCubemapPosZ().empty(), "Default cubemap +Z path is empty");
    TEST_ASSERT(env->GetCubemapNegZ().empty(), "Default cubemap -Z path is empty");
    TEST_ASSERT(env->GetPanoramicTexture().empty(), "Default panoramic texture path is empty");

    TEST_ASSERT(!env->GetFogEnabled(), "Fog is disabled by default");
    TEST_ASSERT(ColorEq(env->GetFogColor(), math::Color(0.5f, 0.5f, 0.5f, 1.0f)),
                "Default fog color matches");
    TEST_ASSERT(FloatEq(env->GetFogDensity(), 0.01f), "Default fog density is 0.01");
    TEST_ASSERT(FloatEq(env->GetFogStart(), 10.0f), "Default fog start is 10.0");
    TEST_ASSERT(FloatEq(env->GetFogEnd(), 100.0f), "Default fog end is 100.0");
    TEST_ASSERT(env->GetFogMode() == 0, "Default fog mode is Linear (0)");

    TEST_ASSERT(env->GetAmbientLightEnabled(), "Ambient light is enabled by default");
    TEST_ASSERT(ColorEq(env->GetAmbientLightColor(), math::Color(1.0f, 1.0f, 1.0f, 1.0f)),
                "Default ambient light color is white");
    TEST_ASSERT(FloatEq(env->GetAmbientLightIntensity(), 0.2f),
                "Default ambient light intensity is 0.2");

    TEST_ASSERT(!env->GetVolumetricFogEnabled(), "Volumetric fog is disabled by default");

    return true;
}

bool TestWorldEnvironmentSkyboxRoundTrip() {
    TEST_SECTION("WorldEnvironment: Skybox Round-Trips");

    std::shared_ptr<components::WorldEnvironment> env =
        std::make_shared<components::WorldEnvironment>();
    env->DefineProperties();

    env->SetSkyboxType(static_cast<int>(components::WorldEnvironment::SkyboxType::Procedural));
    TEST_ASSERT(env->GetSkyboxType() == 2, "Skybox type round-trips to Procedural");
    env->SetSkyboxType(static_cast<int>(components::WorldEnvironment::SkyboxType::Cubemap));
    TEST_ASSERT(env->GetSkyboxType() == 3, "Skybox type round-trips to Cubemap");
    env->SetSkyboxType(static_cast<int>(components::WorldEnvironment::SkyboxType::Panoramic));
    TEST_ASSERT(env->GetSkyboxType() == 4, "Skybox type round-trips to Panoramic");

    math::Color skyboxColor(0.2f, 0.4f, 0.6f, 1.0f);
    env->SetSkyboxColor(skyboxColor);
    TEST_ASSERT(ColorEq(env->GetSkyboxColor(), skyboxColor), "Skybox color round-trips");

    math::Color topColor(0.05f, 0.15f, 0.45f, 1.0f);
    env->SetSkyTopColor(topColor);
    TEST_ASSERT(ColorEq(env->GetSkyTopColor(), topColor), "Sky top color round-trips");

    math::Color horizonColor(0.55f, 0.65f, 0.85f, 1.0f);
    env->SetSkyHorizonColor(horizonColor);
    TEST_ASSERT(ColorEq(env->GetSkyHorizonColor(), horizonColor), "Sky horizon color round-trips");

    math::Color bottomColor(0.7f, 0.7f, 0.7f, 1.0f);
    env->SetSkyBottomColor(bottomColor);
    TEST_ASSERT(ColorEq(env->GetSkyBottomColor(), bottomColor), "Sky bottom color round-trips");

    env->SetCubemapPosX("res://sky/px.png");
    env->SetCubemapNegX("res://sky/nx.png");
    env->SetCubemapPosY("res://sky/py.png");
    env->SetCubemapNegY("res://sky/ny.png");
    env->SetCubemapPosZ("res://sky/pz.png");
    env->SetCubemapNegZ("res://sky/nz.png");
    TEST_ASSERT(env->GetCubemapPosX() == "res://sky/px.png", "Cubemap +X path round-trips");
    TEST_ASSERT(env->GetCubemapNegX() == "res://sky/nx.png", "Cubemap -X path round-trips");
    TEST_ASSERT(env->GetCubemapPosY() == "res://sky/py.png", "Cubemap +Y path round-trips");
    TEST_ASSERT(env->GetCubemapNegY() == "res://sky/ny.png", "Cubemap -Y path round-trips");
    TEST_ASSERT(env->GetCubemapPosZ() == "res://sky/pz.png", "Cubemap +Z path round-trips");
    TEST_ASSERT(env->GetCubemapNegZ() == "res://sky/nz.png", "Cubemap -Z path round-trips");

    env->SetPanoramicTexture("res://sky/pano.hdr");
    TEST_ASSERT(env->GetPanoramicTexture() == "res://sky/pano.hdr",
                "Panoramic texture path round-trips");

    return true;
}

bool TestWorldEnvironmentFogAndAmbientRoundTrip() {
    TEST_SECTION("WorldEnvironment: Fog / Ambient / Volumetric Round-Trips");

    std::shared_ptr<components::WorldEnvironment> env =
        std::make_shared<components::WorldEnvironment>();
    env->DefineProperties();

    env->SetFogEnabled(true);
    TEST_ASSERT(env->GetFogEnabled(), "Fog enables");
    env->SetFogEnabled(false);
    TEST_ASSERT(!env->GetFogEnabled(), "Fog disables");

    math::Color fogColor(0.3f, 0.3f, 0.35f, 1.0f);
    env->SetFogColor(fogColor);
    TEST_ASSERT(ColorEq(env->GetFogColor(), fogColor), "Fog color round-trips");

    env->SetFogDensity(0.25f);
    TEST_ASSERT(FloatEq(env->GetFogDensity(), 0.25f), "Fog density round-trips");
    env->SetFogDensity(0.0f);
    TEST_ASSERT(FloatEq(env->GetFogDensity(), 0.0f), "Minimum fog density round-trips");
    env->SetFogDensity(1.0f);
    TEST_ASSERT(FloatEq(env->GetFogDensity(), 1.0f), "Maximum fog density round-trips");

    env->SetFogStart(25.0f);
    TEST_ASSERT(FloatEq(env->GetFogStart(), 25.0f), "Fog start round-trips");

    env->SetFogEnd(500.0f);
    TEST_ASSERT(FloatEq(env->GetFogEnd(), 500.0f), "Fog end round-trips");

    env->SetFogMode(static_cast<int>(components::WorldEnvironment::FogMode::Exponential));
    TEST_ASSERT(env->GetFogMode() == 1, "Fog mode round-trips to Exponential");
    env->SetFogMode(static_cast<int>(components::WorldEnvironment::FogMode::ExponentialSquared));
    TEST_ASSERT(env->GetFogMode() == 2, "Fog mode round-trips to ExponentialSquared");
    env->SetFogMode(static_cast<int>(components::WorldEnvironment::FogMode::Linear));
    TEST_ASSERT(env->GetFogMode() == 0, "Fog mode round-trips to Linear");

    env->SetAmbientLightEnabled(false);
    TEST_ASSERT(!env->GetAmbientLightEnabled(), "Ambient light disables");
    env->SetAmbientLightEnabled(true);
    TEST_ASSERT(env->GetAmbientLightEnabled(), "Ambient light enables");

    math::Color ambientColor(0.4f, 0.45f, 0.5f, 1.0f);
    env->SetAmbientLightColor(ambientColor);
    TEST_ASSERT(ColorEq(env->GetAmbientLightColor(), ambientColor), "Ambient light color round-trips");

    env->SetAmbientLightIntensity(0.75f);
    TEST_ASSERT(FloatEq(env->GetAmbientLightIntensity(), 0.75f),
                "Ambient light intensity round-trips");
    env->SetAmbientLightIntensity(10.0f);
    TEST_ASSERT(FloatEq(env->GetAmbientLightIntensity(), 10.0f),
                "Maximum ambient light intensity round-trips");

    env->SetVolumetricFogEnabled(true);
    TEST_ASSERT(env->GetVolumetricFogEnabled(), "Volumetric fog enables");
    env->SetVolumetricFogEnabled(false);
    TEST_ASSERT(!env->GetVolumetricFogEnabled(), "Volumetric fog disables");

    return true;
}

bool TestWorldEnvironmentSerialization() {
    TEST_SECTION("WorldEnvironment: Serialization Round-Trip");

    std::shared_ptr<components::WorldEnvironment> env =
        std::make_shared<components::WorldEnvironment>();
    env->DefineProperties();

    env->SetSkyboxType(static_cast<int>(components::WorldEnvironment::SkyboxType::Procedural));
    env->SetSkyboxColor(math::Color(0.11f, 0.22f, 0.33f, 1.0f));
    env->SetFogEnabled(true);
    env->SetFogDensity(0.05f);
    env->SetFogMode(static_cast<int>(components::WorldEnvironment::FogMode::Exponential));
    env->SetAmbientLightIntensity(0.5f);
    env->SetVolumetricFogEnabled(true);

    nlohmann::json json = env->Serialize();
    TEST_ASSERT(json.contains("type") && json["type"] == "WorldEnvironment",
                "Serialized JSON records the component type");
    TEST_ASSERT(json.contains("properties") && json["properties"].is_object(),
                "Serialized JSON contains a properties object");

    std::shared_ptr<components::WorldEnvironment> restored =
        std::make_shared<components::WorldEnvironment>();
    restored->Deserialize(json);

    TEST_ASSERT(restored->GetSkyboxType() == 2, "Deserialized skybox type preserved");
    TEST_ASSERT(ColorEq(restored->GetSkyboxColor(), math::Color(0.11f, 0.22f, 0.33f, 1.0f)),
                "Deserialized skybox color preserved");
    TEST_ASSERT(restored->GetFogEnabled(), "Deserialized fog-enabled flag preserved");
    TEST_ASSERT(FloatEq(restored->GetFogDensity(), 0.05f), "Deserialized fog density preserved");
    TEST_ASSERT(restored->GetFogMode() == 1, "Deserialized fog mode preserved");
    TEST_ASSERT(FloatEq(restored->GetAmbientLightIntensity(), 0.5f),
                "Deserialized ambient light intensity preserved");
    TEST_ASSERT(restored->GetVolumetricFogEnabled(), "Deserialized volumetric fog flag preserved");

    return true;
}

bool TestLightSerialization() {
    TEST_SECTION("Lights: Serialization Round-Trip");

    std::shared_ptr<components::SpotLight3D> spot =
        std::make_shared<components::SpotLight3D>();
    spot->DefineProperties();
    spot->SetColor(math::Color(0.7f, 0.2f, 0.1f, 1.0f));
    spot->SetIntensity(8.0f);
    spot->SetRange(33.0f);
    spot->SetInnerConeAngle(20.0f);
    spot->SetOuterConeAngle(50.0f);
    spot->SetCastsShadows(false);

    nlohmann::json json = spot->Serialize();
    TEST_ASSERT(json.contains("type") && json["type"] == "SpotLight3D",
                "Spot light serialized type is correct");

    std::shared_ptr<components::SpotLight3D> restored =
        std::make_shared<components::SpotLight3D>();
    restored->Deserialize(json);

    TEST_ASSERT(ColorEq(restored->GetColor(), math::Color(0.7f, 0.2f, 0.1f, 1.0f)),
                "Deserialized spot color preserved");
    TEST_ASSERT(FloatEq(restored->GetIntensity(), 8.0f), "Deserialized spot intensity preserved");
    TEST_ASSERT(FloatEq(restored->GetRange(), 33.0f), "Deserialized spot range preserved");
    TEST_ASSERT(FloatEq(restored->GetInnerConeAngle(), 20.0f),
                "Deserialized spot inner cone angle preserved");
    TEST_ASSERT(FloatEq(restored->GetOuterConeAngle(), 50.0f),
                "Deserialized spot outer cone angle preserved");
    TEST_ASSERT(!restored->CastsShadows(), "Deserialized spot shadow flag preserved");

    return true;
}

} // namespace

void RunLightComponentTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "LIGHT COMPONENT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("LightComponents");

    engine::InitializeEngine();

    bool allPassed = true;
    allPassed &= TestDirectionalLightDefaults();
    allPassed &= TestDirectionalLightRoundTrip();
    allPassed &= TestOmniLightDefaults();
    allPassed &= TestOmniLightRoundTrip();
    allPassed &= TestSpotLightDefaults();
    allPassed &= TestSpotLightRoundTrip();
    allPassed &= TestWorldEnvironmentDefaults();
    allPassed &= TestWorldEnvironmentSkyboxRoundTrip();
    allPassed &= TestWorldEnvironmentFogAndAmbientRoundTrip();
    allPassed &= TestWorldEnvironmentSerialization();
    allPassed &= TestLightSerialization();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL LIGHT COMPONENT TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
