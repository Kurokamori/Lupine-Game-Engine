#include "lupine/components/CameraEffects.hpp"
#include "lupine/rendering/CameraEffectShaders.hpp"
#include "lupine/core/PropertyDescriptor.hpp"
#include "lupine/math/Color.hpp"
#include <cmath>
#include <utility>

namespace lupine {
namespace components {

// ===========================================================================
// CameraEffectColorGrade
// ===========================================================================

void CameraEffectColorGrade::DefineProperties() {
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(contrast, 1.0f, 0.0f, 2.0f, 0.01f, "Color Grade"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(saturation, 1.0f, 0.0f, 2.0f, 0.01f, "Color Grade"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(brightness, 0.0f, -1.0f, 1.0f, 0.01f, "Color Grade"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(temperature, 0.0f, -1.0f, 1.0f, 0.01f, "Color Grade"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(tint, 0.0f, -1.0f, 1.0f, 0.01f, "Color Grade"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(colorFilter, Color, math::Color(1.0f, 1.0f, 1.0f, 0.0f), "Color Grade"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(colorLift, Color, math::Color(0.0f, 0.0f, 0.0f, 1.0f), "Color Grade"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(colorGamma, Color, math::Color(1.0f, 1.0f, 1.0f, 1.0f), "Color Grade"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(colorGain, Color, math::Color(1.0f, 1.0f, 1.0f, 1.0f), "Color Grade"));
}

void CameraEffectColorGrade::BuildCompose(CameraComposeContribution& out) const {
    out.id = "colorgrade";
    out.uniformDecls =
        "    uniform float u_fx%S%_Contrast = 1.0;\n"
        "    uniform float u_fx%S%_Saturation = 1.0;\n"
        "    uniform float u_fx%S%_Brightness = 0.0;\n"
        "    uniform float u_fx%S%_Temperature = 0.0;\n"
        "    uniform float u_fx%S%_Tint = 0.0;\n"
        "    uniform vec4 u_fx%S%_ColorFilter = vec4(1.0, 1.0, 1.0, 0.0);\n"
        "    uniform vec3 u_fx%S%_Lift = vec3(0.0, 0.0, 0.0);\n"
        "    uniform vec3 u_fx%S%_Gamma = vec3(1.0, 1.0, 1.0);\n"
        "    uniform vec3 u_fx%S%_Gain = vec3(1.0, 1.0, 1.0);\n";
    out.applyFunction = R"GLSL(
    vec3 fx%S%_apply(vec3 color, vec2 uv) {
        vec3 warm = vec3(1.0 + u_fx%S%_Temperature * 0.2, 1.0, 1.0 - u_fx%S%_Temperature * 0.2);
        vec3 grn = vec3(1.0, 1.0 + u_fx%S%_Tint * 0.2, 1.0);
        color = color * warm * grn;
        color = mix(color, color * u_fx%S%_ColorFilter.rgb, u_fx%S%_ColorFilter.a);
        color = (color - vec3(0.5, 0.5, 0.5)) * u_fx%S%_Contrast + vec3(0.5, 0.5, 0.5);
        color = color + vec3(u_fx%S%_Brightness, u_fx%S%_Brightness, u_fx%S%_Brightness);
        float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
        color = mix(vec3(luma, luma, luma), color, u_fx%S%_Saturation);
        color = color * u_fx%S%_Gain + u_fx%S%_Lift * (vec3(1.0, 1.0, 1.0) - color);
        color = max(color, vec3(0.0, 0.0, 0.0));
        color = pow(color, vec3(1.0, 1.0, 1.0) / max(u_fx%S%_Gamma, vec3(0.001, 0.001, 0.001)));
        return color;
    }
)GLSL";

    const math::Color filter = GetPropertyValue<math::Color>("colorFilter");
    const math::Color lift = GetPropertyValue<math::Color>("colorLift");
    const math::Color gamma = GetPropertyValue<math::Color>("colorGamma");
    const math::Color gain = GetPropertyValue<math::Color>("colorGain");

    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_Contrast", GetPropertyValue<float>("contrast")));
    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_Saturation", GetPropertyValue<float>("saturation")));
    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_Brightness", GetPropertyValue<float>("brightness")));
    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_Temperature", GetPropertyValue<float>("temperature")));
    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_Tint", GetPropertyValue<float>("tint")));
    out.uniforms.push_back(CameraEffectUniform::Vec4("u_fx%S%_ColorFilter", math::Vec4(filter.r, filter.g, filter.b, filter.a)));
    out.uniforms.push_back(CameraEffectUniform::Vec3("u_fx%S%_Lift", math::Vec3(lift.r, lift.g, lift.b)));
    out.uniforms.push_back(CameraEffectUniform::Vec3("u_fx%S%_Gamma", math::Vec3(gamma.r, gamma.g, gamma.b)));
    out.uniforms.push_back(CameraEffectUniform::Vec3("u_fx%S%_Gain", math::Vec3(gain.r, gain.g, gain.b)));
}

// ===========================================================================
// CameraEffectTonemap
// ===========================================================================

void CameraEffectTonemap::DefineProperties() {
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(exposure, 1.0f, 0.0f, 8.0f, 0.01f, "Tonemap"));
    DefineProperty(PROPERTY_ENUM_GROUP(mode, 3, "Tonemap", Linear, Reinhard, ReinhardExtended, ACES, Filmic, AGX));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(whitePoint, 4.0f, 0.1f, 16.0f, 0.1f, "Tonemap"));
}

void CameraEffectTonemap::BuildCompose(CameraComposeContribution& out) const {
    out.id = "tonemap";
    out.uniformDecls =
        "    uniform float u_fx%S%_Exposure = 1.0;\n"
        "    uniform float u_fx%S%_Mode = 3.0;\n"
        "    uniform float u_fx%S%_WhitePoint = 4.0;\n";
    out.applyFunction = R"GLSL(
    vec3 fx%S%_reinhard(vec3 c) { return c / (c + vec3(1.0, 1.0, 1.0)); }
    vec3 fx%S%_reinhardExt(vec3 c, float w) {
        vec3 num = c * (1.0 + c / vec3(w * w, w * w, w * w));
        return num / (1.0 + c);
    }
    vec3 fx%S%_aces(vec3 x) {
        vec3 num = x * (2.51 * x + vec3(0.03, 0.03, 0.03));
        vec3 den = x * (2.43 * x + vec3(0.59, 0.59, 0.59)) + vec3(0.14, 0.14, 0.14);
        return clamp(num / den, 0.0, 1.0);
    }
    vec3 fx%S%_filmic(vec3 x) {
        vec3 X = max(vec3(0.0, 0.0, 0.0), x - vec3(0.004, 0.004, 0.004));
        return (X * (6.2 * X + vec3(0.5, 0.5, 0.5))) / (X * (6.2 * X + vec3(1.7, 1.7, 1.7)) + vec3(0.06, 0.06, 0.06));
    }
    vec3 fx%S%_agx(vec3 c) {
        c = max(c, vec3(0.0, 0.0, 0.0));
        return clamp(c / (c + vec3(0.155, 0.155, 0.155)) * 1.019, 0.0, 1.0);
    }
    vec3 fx%S%_apply(vec3 color, vec2 uv) {
        color = color * u_fx%S%_Exposure;
        float m = u_fx%S%_Mode;
        if (m < 0.5) { return clamp(color, 0.0, 1.0); }
        else if (m < 1.5) { return fx%S%_reinhard(color); }
        else if (m < 2.5) { return fx%S%_reinhardExt(color, u_fx%S%_WhitePoint); }
        else if (m < 3.5) { return fx%S%_aces(color); }
        else if (m < 4.5) { return fx%S%_filmic(color); }
        return fx%S%_agx(color);
    }
)GLSL";

    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_Exposure", GetPropertyValue<float>("exposure")));
    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_Mode", static_cast<float>(GetPropertyValue<int>("mode"))));
    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_WhitePoint", GetPropertyValue<float>("whitePoint")));
}

// ===========================================================================
// CameraEffectVignette
// ===========================================================================

void CameraEffectVignette::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(vignetteColor, Color, math::Color(0.0f, 0.0f, 0.0f, 1.0f), "Vignette"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(intensity, 0.4f, 0.0f, 2.0f, 0.01f, "Vignette"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(smoothness, 0.5f, 0.0f, 1.0f, 0.01f, "Vignette"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(roundness, 1.0f, 0.0f, 1.0f, 0.01f, "Vignette"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(centerX, 0.5f, 0.0f, 1.0f, 0.01f, "Vignette"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(centerY, 0.5f, 0.0f, 1.0f, 0.01f, "Vignette"));
}

void CameraEffectVignette::BuildCompose(CameraComposeContribution& out) const {
    out.id = "vignette";
    out.uniformDecls =
        "    uniform vec4 u_fx%S%_Color = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "    uniform float u_fx%S%_Intensity = 0.4;\n"
        "    uniform float u_fx%S%_Smoothness = 0.5;\n"
        "    uniform float u_fx%S%_Roundness = 1.0;\n"
        "    uniform vec2 u_fx%S%_Center = vec2(0.5, 0.5);\n";
    out.applyFunction = R"GLSL(
    vec3 fx%S%_apply(vec3 color, vec2 uv) {
        vec2 d = uv - u_fx%S%_Center;
        d.x = d.x * mix(1.0, u_Resolution.x / max(u_Resolution.y, 1.0), u_fx%S%_Roundness);
        float dist = length(d) * 1.41421356;
        float vig = 1.0 - smoothstep(u_fx%S%_Smoothness * 0.5, 1.0, dist) * u_fx%S%_Intensity;
        vig = clamp(vig, 0.0, 1.0);
        return mix(u_fx%S%_Color.rgb, color, vig);
    }
)GLSL";

    const math::Color color = GetPropertyValue<math::Color>("vignetteColor");
    out.uniforms.push_back(CameraEffectUniform::Vec4("u_fx%S%_Color", math::Vec4(color.r, color.g, color.b, color.a)));
    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_Intensity", GetPropertyValue<float>("intensity")));
    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_Smoothness", GetPropertyValue<float>("smoothness")));
    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_Roundness", GetPropertyValue<float>("roundness")));
    out.uniforms.push_back(CameraEffectUniform::Vec2("u_fx%S%_Center",
        math::Vec2(GetPropertyValue<float>("centerX"), GetPropertyValue<float>("centerY"))));
}

// ===========================================================================
// CameraEffectFilmGrain
// ===========================================================================

void CameraEffectFilmGrain::DefineProperties() {
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(intensity, 0.08f, 0.0f, 1.0f, 0.01f, "Film Grain"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(size, 1.0f, 0.5f, 8.0f, 0.1f, "Film Grain"));
}

void CameraEffectFilmGrain::BuildCompose(CameraComposeContribution& out) const {
    out.id = "grain";
    out.uniformDecls =
        "    uniform float u_fx%S%_Intensity = 0.08;\n"
        "    uniform float u_fx%S%_Size = 1.0;\n";
    out.applyFunction = R"GLSL(
    vec3 fx%S%_hash(vec2 p) {
        float n = sin(dot(p, vec2(41.0, 289.0)));
        return fract(vec3(262144.0, 32768.0, 4096.0) * n);
    }
    vec3 fx%S%_apply(vec3 color, vec2 uv) {
        vec2 gp = floor(uv * u_Resolution / max(u_fx%S%_Size, 0.5));
        float g = fx%S%_hash(gp + vec2(u_Time, u_Time * 1.7)).x;
        return color + (g - 0.5) * u_fx%S%_Intensity;
    }
)GLSL";

    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_Intensity", GetPropertyValue<float>("intensity")));
    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_Size", GetPropertyValue<float>("size")));
}

// ===========================================================================
// CameraEffectColorInvert
// ===========================================================================

void CameraEffectColorInvert::DefineProperties() {
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(strength, 1.0f, 0.0f, 1.0f, 0.01f, "Color Invert"));
}

void CameraEffectColorInvert::BuildCompose(CameraComposeContribution& out) const {
    out.id = "invert";
    out.uniformDecls = "    uniform float u_fx%S%_Strength = 1.0;\n";
    out.applyFunction = R"GLSL(
    vec3 fx%S%_apply(vec3 color, vec2 uv) {
        return mix(color, vec3(1.0, 1.0, 1.0) - color, u_fx%S%_Strength);
    }
)GLSL";
    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_Strength", GetPropertyValue<float>("strength")));
}

// ===========================================================================
// CameraEffectPosterize
// ===========================================================================

void CameraEffectPosterize::DefineProperties() {
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(levels, 8.0f, 2.0f, 64.0f, 1.0f, "Posterize"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(strength, 1.0f, 0.0f, 1.0f, 0.01f, "Posterize"));
}

void CameraEffectPosterize::BuildCompose(CameraComposeContribution& out) const {
    out.id = "posterize";
    out.uniformDecls =
        "    uniform float u_fx%S%_Levels = 8.0;\n"
        "    uniform float u_fx%S%_Strength = 1.0;\n";
    out.applyFunction = R"GLSL(
    vec3 fx%S%_apply(vec3 color, vec2 uv) {
        float l = max(u_fx%S%_Levels, 2.0);
        vec3 q = floor(clamp(color, 0.0, 1.0) * l) / max(l - 1.0, 1.0);
        q = clamp(q, 0.0, 1.0);
        return mix(color, q, u_fx%S%_Strength);
    }
)GLSL";
    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_Levels", GetPropertyValue<float>("levels")));
    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_Strength", GetPropertyValue<float>("strength")));
}

// ===========================================================================
// CameraEffectHueShift
// ===========================================================================

void CameraEffectHueShift::DefineProperties() {
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(hueDegrees, 0.0f, -180.0f, 180.0f, 1.0f, "Hue Shift"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(saturation, 1.0f, 0.0f, 2.0f, 0.01f, "Hue Shift"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(value, 1.0f, 0.0f, 2.0f, 0.01f, "Hue Shift"));
}

void CameraEffectHueShift::BuildCompose(CameraComposeContribution& out) const {
    out.id = "hueshift";
    out.uniformDecls =
        "    uniform float u_fx%S%_Hue = 0.0;\n"
        "    uniform float u_fx%S%_SatMul = 1.0;\n"
        "    uniform float u_fx%S%_ValMul = 1.0;\n";
    out.applyFunction = R"GLSL(
    vec3 fx%S%_apply(vec3 color, vec2 uv) {
        float Y = dot(color, vec3(0.299, 0.587, 0.114));
        float I = dot(color, vec3(0.595716, -0.274453, -0.321263));
        float Q = dot(color, vec3(0.211456, -0.522591, 0.311135));
        float h = u_fx%S%_Hue * 6.2831853;
        float c = cos(h);
        float s = sin(h);
        float I2 = I * c - Q * s;
        float Q2 = I * s + Q * c;
        float R = Y + 0.9563 * I2 + 0.6210 * Q2;
        float G = Y - 0.2721 * I2 - 0.6474 * Q2;
        float B = Y - 1.1070 * I2 + 1.7046 * Q2;
        vec3 rgb = vec3(R, G, B);
        float luma = dot(rgb, vec3(0.299, 0.587, 0.114));
        rgb = mix(vec3(luma, luma, luma), rgb, u_fx%S%_SatMul);
        return rgb * u_fx%S%_ValMul;
    }
)GLSL";

    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_Hue", GetPropertyValue<float>("hueDegrees") / 360.0f));
    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_SatMul", GetPropertyValue<float>("saturation")));
    out.uniforms.push_back(CameraEffectUniform::Float("u_fx%S%_ValMul", GetPropertyValue<float>("value")));
}

// ===========================================================================
// CameraEffectBlur
// ===========================================================================

void CameraEffectBlur::DefineProperties() {
    DefineProperty(PROPERTY_ENUM_GROUP(mode, 0, "Blur", Box, Directional));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(radius, 4.0f, 0.0f, 64.0f, 0.5f, "Blur"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(samples, 8, 1, 64, 1, "Blur"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(direction, 0.0f, -180.0f, 180.0f, 1.0f, "Blur"));
}

void CameraEffectBlur::BuildPasses(const CameraEffectContext& ctx, std::vector<CameraEffectPass>& out) const {
    (void)ctx;
    const std::string extraDecls =
        "    uniform float u_Radius = 4.0;\n"
        "    uniform float u_Samples = 8.0;\n"
        "    uniform vec2 u_Dir = vec2(1.0, 0.0);\n";
    const std::string fragmentBody = R"GLSL(
    void main() {
        float K = u_Samples;
        vec2 stepv = u_Dir * u_TexelSize * (u_Radius / max(K, 1.0));
        vec4 sum = SAMPLE(u_SceneTex, v_TexCoord) * (K + 1.0);
        float wsum = K + 1.0;
        for (int i = 1; i < 65; i = i + 1) {
            if (float(i) > K) { break; }
            float w = (K + 1.0 - float(i));
            vec2 o = stepv * float(i);
            sum = sum + SAMPLE(u_SceneTex, v_TexCoord + o) * w;
            sum = sum + SAMPLE(u_SceneTex, v_TexCoord - o) * w;
            wsum = wsum + 2.0 * w;
        }
        FragColor = sum / max(wsum, 0.0001);
    }
)GLSL";

    const std::string source =
        CameraEffectShaders::BuildStandaloneShader("CameraBlur", extraDecls, fragmentBody);
    const float radius = GetPropertyValue<float>("radius");
    const float samples = static_cast<float>(GetPropertyValue<int>("samples"));
    const int mode = GetPropertyValue<int>("mode");

    if (mode == 1) {
        const float ang = GetPropertyValue<float>("direction") * 0.01745329252f;
        CameraEffectPass pass;
        pass.cacheKey = "blur";
        pass.lshSource = source;
        pass.uniforms.push_back(CameraEffectUniform::Float("u_Radius", radius));
        pass.uniforms.push_back(CameraEffectUniform::Float("u_Samples", samples));
        pass.uniforms.push_back(CameraEffectUniform::Vec2("u_Dir", math::Vec2(std::cos(ang), std::sin(ang))));
        out.push_back(std::move(pass));
        return;
    }

    CameraEffectPass horizontal;
    horizontal.cacheKey = "blur";
    horizontal.lshSource = source;
    horizontal.uniforms.push_back(CameraEffectUniform::Float("u_Radius", radius));
    horizontal.uniforms.push_back(CameraEffectUniform::Float("u_Samples", samples));
    horizontal.uniforms.push_back(CameraEffectUniform::Vec2("u_Dir", math::Vec2(1.0f, 0.0f)));
    out.push_back(std::move(horizontal));

    CameraEffectPass vertical;
    vertical.cacheKey = "blur";
    vertical.lshSource = source;
    vertical.uniforms.push_back(CameraEffectUniform::Float("u_Radius", radius));
    vertical.uniforms.push_back(CameraEffectUniform::Float("u_Samples", samples));
    vertical.uniforms.push_back(CameraEffectUniform::Vec2("u_Dir", math::Vec2(0.0f, 1.0f)));
    out.push_back(std::move(vertical));
}

// ===========================================================================
// CameraEffectGlow
// ===========================================================================

void CameraEffectGlow::DefineProperties() {
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(threshold, 0.7f, 0.0f, 4.0f, 0.01f, "Glow"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(intensity, 1.0f, 0.0f, 4.0f, 0.01f, "Glow"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(radius, 16.0f, 1.0f, 128.0f, 1.0f, "Glow"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(samples, 32, 4, 64, 1, "Glow"));
}

void CameraEffectGlow::BuildPasses(const CameraEffectContext& ctx, std::vector<CameraEffectPass>& out) const {
    (void)ctx;
    const std::string extraDecls =
        "    uniform float u_Threshold = 0.7;\n"
        "    uniform float u_Intensity = 1.0;\n"
        "    uniform float u_GlowRadius = 16.0;\n"
        "    uniform float u_Samples = 32.0;\n";
    const std::string fragmentBody = R"GLSL(
    void main() {
        vec4 base4 = SAMPLE(u_SceneTex, v_TexCoord);
        vec3 base = base4.rgb;
        float K = u_Samples;
        vec3 glow = vec3(0.0, 0.0, 0.0);
        float wsum = 0.0001;
        for (int i = 0; i < 64; i = i + 1) {
            if (float(i) >= K) { break; }
            float fi = float(i);
            float ang = fi * 2.39996323;
            float t = (fi + 0.5) / max(K, 1.0);
            float rad = sqrt(t) * u_GlowRadius;
            vec2 o = vec2(cos(ang), sin(ang)) * u_TexelSize * rad;
            vec3 c = SAMPLE(u_SceneTex, v_TexCoord + o).rgb;
            float br = max(c.r, max(c.g, c.b));
            float contrib = max(br - u_Threshold, 0.0);
            vec3 bright = c * (contrib / max(br, 0.0001));
            float w = 1.0 - sqrt(t);
            glow = glow + bright * w;
            wsum = wsum + w;
        }
        glow = glow / wsum;
        FragColor = vec4(base + glow * u_Intensity, base4.a);
    }
)GLSL";

    CameraEffectPass pass;
    pass.cacheKey = "glow";
    pass.lshSource = CameraEffectShaders::BuildStandaloneShader("CameraGlow", extraDecls, fragmentBody);
    pass.uniforms.push_back(CameraEffectUniform::Float("u_Threshold", GetPropertyValue<float>("threshold")));
    pass.uniforms.push_back(CameraEffectUniform::Float("u_Intensity", GetPropertyValue<float>("intensity")));
    pass.uniforms.push_back(CameraEffectUniform::Float("u_GlowRadius", GetPropertyValue<float>("radius")));
    pass.uniforms.push_back(CameraEffectUniform::Float("u_Samples", static_cast<float>(GetPropertyValue<int>("samples"))));
    out.push_back(std::move(pass));
}

// ===========================================================================
// CameraEffectOutline
// ===========================================================================

void CameraEffectOutline::DefineProperties() {
    DefineProperty(PROPERTY_ENUM_GROUP(mode, 0, "Outline", Color, Depth));
    DefineProperty(PROPERTY_DEFAULT_GROUP(outlineColor, Color, math::Color(0.0f, 0.0f, 0.0f, 1.0f), "Outline"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(thickness, 1.0f, 0.5f, 8.0f, 0.1f, "Outline"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(threshold, 0.2f, 0.001f, 4.0f, 0.001f, "Outline"));
}

void CameraEffectOutline::BuildPasses(const CameraEffectContext& ctx, std::vector<CameraEffectPass>& out) const {
    const math::Color color = GetPropertyValue<math::Color>("outlineColor");
    const float thickness = GetPropertyValue<float>("thickness");
    const float threshold = GetPropertyValue<float>("threshold");
    const int mode = GetPropertyValue<int>("mode");
    const bool useDepth = (mode == 1) && ctx.cameraPerspective;

    CameraEffectPass pass;
    pass.uniforms.push_back(CameraEffectUniform::Vec4("u_Color", math::Vec4(color.r, color.g, color.b, color.a)));
    pass.uniforms.push_back(CameraEffectUniform::Float("u_Thickness", thickness));
    pass.uniforms.push_back(CameraEffectUniform::Float("u_Threshold", threshold));

    if (useDepth) {
        const std::string extraDecls =
            "    uniform sampler2D u_DepthTex;\n"
            "    uniform vec4 u_Color = vec4(0.0, 0.0, 0.0, 1.0);\n"
            "    uniform float u_Thickness = 1.0;\n"
            "    uniform float u_Threshold = 0.2;\n"
            "    uniform float u_Near = 0.1;\n"
            "    uniform float u_Far = 1000.0;\n"
            "    uniform float u_DepthZeroToOne = 1.0;\n";
        const std::string fragmentBody = R"GLSL(
    float linEye(vec2 uv) {
        float d = SAMPLE(u_DepthTex, uv).r;
        float ndc = mix(d * 2.0 - 1.0, d, u_DepthZeroToOne);
        float denom = (u_Far + u_Near - ndc * (u_Far - u_Near));
        return (2.0 * u_Near * u_Far) / max(denom, 0.0001);
    }
    void main() {
        vec2 t = u_TexelSize * u_Thickness;
        float c = linEye(v_TexCoord);
        float l = linEye(v_TexCoord - vec2(t.x, 0.0));
        float r = linEye(v_TexCoord + vec2(t.x, 0.0));
        float u = linEye(v_TexCoord + vec2(0.0, t.y));
        float dn = linEye(v_TexCoord - vec2(0.0, t.y));
        float edge = abs(c - l) + abs(c - r) + abs(c - u) + abs(c - dn);
        float e = smoothstep(u_Threshold, u_Threshold * 2.0, edge);
        vec4 base4 = SAMPLE(u_SceneTex, v_TexCoord);
        float oa = e * u_Color.a;
        FragColor = vec4(mix(base4.rgb, u_Color.rgb, oa), max(base4.a, oa));
    }
)GLSL";
        pass.cacheKey = "outline_depth";
        pass.lshSource = CameraEffectShaders::BuildStandaloneShader("CameraOutlineDepth", extraDecls, fragmentBody);
        pass.needsDepth = true;
        pass.uniforms.push_back(CameraEffectUniform::Float("u_Near", ctx.cameraNear));
        pass.uniforms.push_back(CameraEffectUniform::Float("u_Far", ctx.cameraFar));
    } else {
        const std::string extraDecls =
            "    uniform vec4 u_Color = vec4(0.0, 0.0, 0.0, 1.0);\n"
            "    uniform float u_Thickness = 1.0;\n"
            "    uniform float u_Threshold = 0.2;\n";
        const std::string fragmentBody = R"GLSL(
    float lum(vec2 uv) { return dot(SAMPLE(u_SceneTex, uv).rgb, vec3(0.299, 0.587, 0.114)); }
    void main() {
        vec2 t = u_TexelSize * u_Thickness;
        float tl = lum(v_TexCoord + vec2(-t.x, t.y));
        float tc = lum(v_TexCoord + vec2(0.0, t.y));
        float tr = lum(v_TexCoord + vec2(t.x, t.y));
        float ml = lum(v_TexCoord + vec2(-t.x, 0.0));
        float mr = lum(v_TexCoord + vec2(t.x, 0.0));
        float bl = lum(v_TexCoord + vec2(-t.x, -t.y));
        float bc = lum(v_TexCoord + vec2(0.0, -t.y));
        float br = lum(v_TexCoord + vec2(t.x, -t.y));
        float gx = -tl - 2.0 * ml - bl + tr + 2.0 * mr + br;
        float gy = tl + 2.0 * tc + tr - bl - 2.0 * bc - br;
        float edge = sqrt(gx * gx + gy * gy);
        float e = smoothstep(u_Threshold, u_Threshold * 2.0, edge);
        vec4 base4 = SAMPLE(u_SceneTex, v_TexCoord);
        float oa = e * u_Color.a;
        FragColor = vec4(mix(base4.rgb, u_Color.rgb, oa), max(base4.a, oa));
    }
)GLSL";
        pass.cacheKey = "outline_color";
        pass.lshSource = CameraEffectShaders::BuildStandaloneShader("CameraOutlineColor", extraDecls, fragmentBody);
        pass.needsDepth = false;
    }
    out.push_back(std::move(pass));
}

// ===========================================================================
// CameraEffectPixelate
// ===========================================================================

void CameraEffectPixelate::DefineProperties() {
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pixelSize, 4.0f, 1.0f, 128.0f, 1.0f, "Pixelate"));
}

void CameraEffectPixelate::BuildPasses(const CameraEffectContext& ctx, std::vector<CameraEffectPass>& out) const {
    (void)ctx;
    const std::string extraDecls = "    uniform float u_PixelSize = 4.0;\n";
    const std::string fragmentBody = R"GLSL(
    void main() {
        float ps = max(u_PixelSize, 1.0);
        vec2 cell = floor(v_TexCoord * u_Resolution / ps) * ps + (ps * 0.5);
        vec2 uv = cell / u_Resolution;
        FragColor = SAMPLE(u_SceneTex, uv);
    }
)GLSL";

    CameraEffectPass pass;
    pass.cacheKey = "pixelate";
    pass.lshSource = CameraEffectShaders::BuildStandaloneShader("CameraPixelate", extraDecls, fragmentBody);
    pass.pointSampleScene = true;
    pass.uniforms.push_back(CameraEffectUniform::Float("u_PixelSize", GetPropertyValue<float>("pixelSize")));
    out.push_back(std::move(pass));
}

// ===========================================================================
// CameraEffectSharpen
// ===========================================================================

void CameraEffectSharpen::DefineProperties() {
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(amount, 0.5f, 0.0f, 4.0f, 0.01f, "Sharpen"));
}

void CameraEffectSharpen::BuildPasses(const CameraEffectContext& ctx, std::vector<CameraEffectPass>& out) const {
    (void)ctx;
    const std::string extraDecls = "    uniform float u_Amount = 0.5;\n";
    const std::string fragmentBody = R"GLSL(
    void main() {
        vec2 t = u_TexelSize;
        vec4 c4 = SAMPLE(u_SceneTex, v_TexCoord);
        vec3 c = c4.rgb;
        vec3 n = SAMPLE(u_SceneTex, v_TexCoord + vec2(t.x, 0.0)).rgb
               + SAMPLE(u_SceneTex, v_TexCoord - vec2(t.x, 0.0)).rgb
               + SAMPLE(u_SceneTex, v_TexCoord + vec2(0.0, t.y)).rgb
               + SAMPLE(u_SceneTex, v_TexCoord - vec2(0.0, t.y)).rgb;
        vec3 result = c + (c * 4.0 - n) * u_Amount;
        FragColor = vec4(result, c4.a);
    }
)GLSL";

    CameraEffectPass pass;
    pass.cacheKey = "sharpen";
    pass.lshSource = CameraEffectShaders::BuildStandaloneShader("CameraSharpen", extraDecls, fragmentBody);
    pass.uniforms.push_back(CameraEffectUniform::Float("u_Amount", GetPropertyValue<float>("amount")));
    out.push_back(std::move(pass));
}

// ===========================================================================
// CameraEffectChromaticAberration
// ===========================================================================

void CameraEffectChromaticAberration::DefineProperties() {
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(amount, 0.004f, 0.0f, 0.08f, 0.0005f, "Chromatic Aberration"));
}

void CameraEffectChromaticAberration::BuildPasses(const CameraEffectContext& ctx, std::vector<CameraEffectPass>& out) const {
    (void)ctx;
    const std::string extraDecls = "    uniform float u_Amount = 0.004;\n";
    const std::string fragmentBody = R"GLSL(
    void main() {
        vec2 uv = v_TexCoord;
        vec2 dir = uv - vec2(0.5, 0.5);
        float amt = u_Amount;
        vec4 center = SAMPLE(u_SceneTex, uv);
        float r = SAMPLE(u_SceneTex, uv - dir * amt).r;
        float b = SAMPLE(u_SceneTex, uv + dir * amt).b;
        FragColor = vec4(r, center.g, b, center.a);
    }
)GLSL";

    CameraEffectPass pass;
    pass.cacheKey = "ca";
    pass.lshSource = CameraEffectShaders::BuildStandaloneShader("CameraChromaticAberration", extraDecls, fragmentBody);
    pass.uniforms.push_back(CameraEffectUniform::Float("u_Amount", GetPropertyValue<float>("amount")));
    out.push_back(std::move(pass));
}

} // namespace components
} // namespace lupine
