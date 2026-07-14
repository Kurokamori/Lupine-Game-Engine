
#version 330 core

in vec3 v_WorldPos;
in vec3 v_ViewPos;
in vec3 v_Normal;
in vec2 v_TexCoord;
in vec4 v_Color;
in vec3 v_ViewDir;

uniform mat4 u_ViewProjection;
uniform mat4 u_View;
uniform mat4 u_Model;
uniform mat4 u_NormalMatrix;
uniform vec3 u_CameraPosition;
uniform vec4 u_TintColor;
uniform vec4 u_AlbedoColor;
uniform vec4 u_EmissiveColor;
uniform vec4 u_TextureFlags;
uniform vec4 u_MaterialParams1;
uniform vec4 u_ToonMaterialParams;
uniform vec4 u_ToonMaterialParams2;
uniform vec4 u_ToonParams;
uniform int u_ReceiveShadow;
uniform sampler2D u_AlbedoTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_EmissiveTexture;

struct Light {
    vec4 positionOrDirection;
    vec4 direction;
    vec4 color;
    vec4 params;
    vec4 flags;
};
struct ShadowMap {
    mat4 lightSpaceMatrix;
    vec4 shadowParams;
    vec4 shadowParams2;
};
struct CascadedShadowMap {
    mat4 cascadeMatrices[8];
    vec4 cascadeSplits;
    vec4 cascadeSplits2;
    vec4 cascadeParams;
    vec4 cascadeParams2;
};
layout(std140) uniform LightData {
    Light lights[16];
    ShadowMap shadowMaps[8];
    CascadedShadowMap cascadedShadowMaps[8];
    vec4 ambientLight;
    vec4 lightCounts;
    vec4 fogColor;
    vec4 fogParams;
} u_Lights;

uniform sampler2D u_ShadowMap0;
uniform sampler2D u_ShadowMap1;
uniform sampler2D u_ShadowMap2;
uniform sampler2D u_ShadowMap3;
uniform sampler2D u_ShadowMap4;
uniform sampler2D u_ShadowMap5;
uniform sampler2D u_ShadowMap6;
uniform sampler2D u_ShadowMap7;
uniform samplerCube u_ShadowCubeMap0;
uniform samplerCube u_ShadowCubeMap1;
uniform samplerCube u_ShadowCubeMap2;
uniform samplerCube u_ShadowCubeMap3;
uniform samplerCube u_ShadowCubeMap4;
uniform samplerCube u_ShadowCubeMap5;
uniform samplerCube u_ShadowCubeMap6;
uniform samplerCube u_ShadowCubeMap7;

float sampleShadowMap(int index, vec2 uv) {
    if (index == 0) return texture(u_ShadowMap0, uv).r;
    else if (index == 1) return texture(u_ShadowMap1, uv).r;
    else if (index == 2) return texture(u_ShadowMap2, uv).r;
    else if (index == 3) return texture(u_ShadowMap3, uv).r;
    else if (index == 4) return texture(u_ShadowMap4, uv).r;
    else if (index == 5) return texture(u_ShadowMap5, uv).r;
    else if (index == 6) return texture(u_ShadowMap6, uv).r;
    else if (index == 7) return texture(u_ShadowMap7, uv).r;
    return 1.0;
}

float sampleShadowCubeMap(int index, vec3 dir) {
    vec4 s;
    if (index == 0) s = texture(u_ShadowCubeMap0, dir);
    else if (index == 1) s = texture(u_ShadowCubeMap1, dir);
    else if (index == 2) s = texture(u_ShadowCubeMap2, dir);
    else if (index == 3) s = texture(u_ShadowCubeMap3, dir);
    else if (index == 4) s = texture(u_ShadowCubeMap4, dir);
    else if (index == 5) s = texture(u_ShadowCubeMap5, dir);
    else if (index == 6) s = texture(u_ShadowCubeMap6, dir);
    else if (index == 7) s = texture(u_ShadowCubeMap7, dir);
    else return 1.0;
    return s.r;
}

out vec4 FragColor;

float calculateShadowPCF(mat4 lsMatrix, int smIndex, vec3 wPos, vec3 N, vec3 lDir,
                         float bias, float nBias, float blur, float opacity, float res) {
    vec4 lsPos = (lsMatrix * vec4(wPos, 1.0));
    vec3 proj = lsPos.xyz / lsPos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;
    float curDepth = proj.z;
    float NdL = max(dot(N, lDir), 0.0);
    float fBias = mix(bias * 2.0, bias, NdL);
    float shadow = 0.0;
    vec2 texelSz = 1.0 / vec2(res, res);
    int kr = clamp(int(blur), 1, 3);
    int sc = 0;
    for (int x = -kr; x <= kr; ++x) {
        for (int y = -kr; y <= kr; ++y) {
            vec2 off = vec2(float(x), float(y)) * texelSz * blur;
            float d = sampleShadowMap(smIndex, proj.xy + off);
            shadow += curDepth - fBias > d ? 1.0 : 0.0;
            sc++;
        }
    }
    shadow /= float(sc);
    shadow *= opacity;
    return 1.0 - shadow;
}

float calculateShadowCube(int cmIndex, vec3 lPos, vec3 wPos, vec3 N,
                          float bias, float blur, float opacity, float lRange) {
    vec3 frag2light = wPos - lPos;
    float curDepth = length(frag2light);
    float normDepth = curDepth / lRange;
    vec3 sDir = normalize(frag2light);
    float closest = sampleShadowCubeMap(cmIndex, sDir);
    vec3 lDir2 = -sDir;
    float NdL = max(dot(N, lDir2), 0.0);
    float nBias = bias / lRange;
    float fBias = mix(nBias * 3.0, nBias * 0.5, NdL);
    float shadow = (normDepth - fBias) > closest ? 1.0 : 0.0;
    shadow *= opacity;
    return 1.0 - shadow;
}

float calculateShadow(int smIndex, vec3 wPos, vec3 N, vec3 lDir, vec3 lPos) {
    if (smIndex < 0 || smIndex >= 8) return 1.0;
    ShadowMap sm = u_Lights.shadowMaps[smIndex];
    bool isCube = sm.shadowParams2.y > 0.5;
    if (isCube) {
        float lRange = sm.shadowParams2.z;
        return calculateShadowCube(smIndex, lPos, wPos, N,
            sm.shadowParams.x, sm.shadowParams.z, sm.shadowParams.w, lRange);
    } else {
        return calculateShadowPCF(sm.lightSpaceMatrix, smIndex, wPos, N, lDir,
            sm.shadowParams.x, sm.shadowParams.y, sm.shadowParams.z,
            sm.shadowParams.w, sm.shadowParams2.x);
    }
}

vec3 applyFog(vec3 color, vec3 wPos, vec3 camPos) {
    if (u_Lights.fogColor.w < 0.5) {
        return color;
    }
    float dist = length(wPos - camPos);
    float density = u_Lights.fogParams.x;
    float fogStart = u_Lights.fogParams.y;
    float fogEnd = u_Lights.fogParams.z;
    int mode = int(u_Lights.fogParams.w);
    float factor = 1.0;
    if (mode == 0) {
        factor = clamp((fogEnd - dist) / max(fogEnd - fogStart, 0.0001), 0.0, 1.0);
    } else if (mode == 1) {
        factor = exp(-density * max(dist - fogStart, 0.0));
    } else {
        float dd = density * max(dist - fogStart, 0.0);
        factor = exp(-dd * dd);
    }
    return mix(u_Lights.fogColor.rgb, color, clamp(factor, 0.0, 1.0));
}

vec3 getNormalFromMap() {
    if (u_TextureFlags.z < 0.5)
        return normalize(v_Normal);
    vec3 tNorm = texture(u_NormalTexture, v_TexCoord).xyz * 2.0 - 1.0;
    tNorm.xy *= u_MaterialParams1.z;
    vec3 Q1 = dFdx(v_WorldPos);
    vec3 Q2 = dFdy(v_WorldPos);
    vec2 st1 = dFdx(v_TexCoord);
    vec2 st2 = dFdy(v_TexCoord);
    vec3 Nn = normalize(v_Normal);
    vec3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    vec3 B = -normalize(cross(Nn, T));
    mat3 TBN = mat3(T, B, Nn);
    return normalize(TBN * tNorm);
}

    // Wrap NdotL from [-1, 1] to [0, 1] range for robust skeletal mesh lighting
    float wrapLighting(float NdotL) {
        return NdotL * 0.5 + 0.5;
    }

    // Cel shading: quantize value into discrete bands
    float celShade(float value, float bands, float threshold, float softness) {
        float adjustedValue = smoothstep(threshold - softness, threshold + softness, value);
        if (bands <= 1.0) {
            return adjustedValue;
        }
        float quantized = floor(adjustedValue * bands) / (bands - 1.0);
        return clamp(quantized, 0.0, 1.0);
    }

    // Improved cel shading with stacking bands
    float celShadeStacked(float value, float bands) {
        if (bands <= 1.0) {
            return step(0.5, value);
        }
        float quantized = floor(value * bands) / (bands - 1.0);
        return clamp(quantized, 0.0, 1.0);
    }

    // Calculate rim lighting (fresnel-based edge glow)
    float calculateRimLighting(vec3 N, vec3 V, float rimPower, float rimIntensity) {
        float rim = 1.0 - max(dot(N, V), 0.0);
        rim = pow(rim, rimPower) * rimIntensity;
        return rim;
    }


    void main() {
        // Sample albedo texture
        vec4 albedo = u_AlbedoColor;
        if (u_TextureFlags.x > 0.5) {
            albedo *= texture(u_AlbedoTexture, v_TexCoord);
        }

        // Alpha test (u_ToonMaterialParams2.x = alphaCutoff)
        if (albedo.a < u_ToonMaterialParams2.x) {
            discard;
        }

        // Unpack toon parameters with defaults
        // u_ToonMaterialParams: x=shadowBands, y=specularBands, z=normalScale, w=emissiveStrength
        float shadowBands = max(u_ToonMaterialParams.x, 2.0);
        float specularBands = max(u_ToonMaterialParams.y, 2.0);
        float normalScale = u_ToonMaterialParams.z;
        // u_ToonParams: x=shadowThreshold, y=shadowSoftness, z=specularThreshold, w=specularSoftness
        float shadowThreshold = u_ToonParams.x > 0.0 ? u_ToonParams.x : 0.5;
        float shadowSoftness = u_ToonParams.y > 0.0 ? u_ToonParams.y : 0.02;
        float specularThreshold = u_ToonParams.z > 0.0 ? u_ToonParams.z : 0.3;
        float specularSoftness = u_ToonParams.w > 0.0 ? u_ToonParams.w : 0.05;
        // u_ToonMaterialParams2: x=alphaCutoff, y=rimPower, z=rimIntensity, w=specularPower
        float rimPower = u_ToonMaterialParams2.y > 0.0 ? u_ToonMaterialParams2.y : 3.0;
        float rimIntensity = u_ToonMaterialParams2.z;
        float specularPower = u_ToonMaterialParams2.w > 0.0 ? u_ToonMaterialParams2.w : 32.0;

        // Normal mapping (screen-space TBN)
        vec3 N = normalize(v_Normal);
        if (u_TextureFlags.z > 0.5) {
            vec3 tangentNormal = texture(u_NormalTexture, v_TexCoord).rgb * 2.0 - 1.0;
            tangentNormal.xy *= normalScale;
            vec3 Q1 = dFdx(v_WorldPos);
            vec3 Q2 = dFdy(v_WorldPos);
            vec2 st1 = dFdx(v_TexCoord);
            vec2 st2 = dFdy(v_TexCoord);
            vec3 T = normalize(Q1 * st2.y - Q2 * st1.y);
            vec3 B = -normalize(cross(N, T));
            mat3 TBN = mat3(T, B, N);
            N = normalize((TBN * tangentNormal));
        }

        vec3 V = normalize(v_ViewDir);

        // Ambient from light UBO
        vec3 ambient = u_Lights.ambientLight.rgb;

        // Accumulate lighting from all dynamic lights
        vec3 totalDiffuse = vec3(0.0);
        vec3 totalSpecular = vec3(0.0);
        int numLights = int(u_Lights.lightCounts.x);

        for (int i = 0; i < 16; i++) {
            if (i >= numLights) break;

            vec3 lightColor = u_Lights.lights[i].color.rgb;
            float lightIntensity = u_Lights.lights[i].color.w;
            int lightType = int(u_Lights.lights[i].flags.x);
            vec3 lightPos = u_Lights.lights[i].positionOrDirection.xyz;

            // Compute light direction
            vec3 L;
            float attenuation = 1.0;

            if (lightType == 0) {
                // Directional light
                L = normalize(-u_Lights.lights[i].direction.xyz);
            } else {
                // Point or spot light
                vec3 toLight = lightPos - v_WorldPos;
                float dist = length(toLight);
                L = normalize(toLight);
                float lightRange = u_Lights.lights[i].params.x;
                float distFactor = clamp(dist / lightRange, 0.0, 1.0);
                attenuation = 1.0 / (1.0 + distFactor * distFactor);
                attenuation *= smoothstep(1.0, 0.5, distFactor);

                if (lightType == 2) {
                    // Spot light cone attenuation
                    vec3 spotDir = normalize(-u_Lights.lights[i].direction.xyz);
                    float cosAngle = dot(L, spotDir);
                    float innerCone = u_Lights.lights[i].params.y;
                    float outerCone = u_Lights.lights[i].params.z;
                    float epsilon = max(innerCone - outerCone, 0.0001);
                    attenuation *= clamp((cosAngle - outerCone) / epsilon, 0.0, 1.0);
                }
            }

            vec3 H = normalize(V + L);

            // Cel-shaded diffuse using wrapped NdotL
            float rawNdotL = dot(N, L);
            float wrappedNdotL = wrapLighting(rawNdotL);
            float NdotL = max(rawNdotL, 0.0);

            float celDiffuse = celShade(wrappedNdotL, shadowBands, shadowThreshold, shadowSoftness);

            // Cel-shaded specular (Blinn-Phong based)
            float NdotH = max(dot(N, H), 0.0);
            float specularRaw = pow(NdotH, specularPower);
            float specularFactor = specularRaw * NdotL;
            float specularStep = smoothstep(specularThreshold - specularSoftness,
                                            specularThreshold + specularSoftness,
                                            specularFactor);
            float celSpecular = specularBands > 1.0 ?
                celShadeStacked(specularStep, specularBands) : specularStep;

            // Shadow
            float shadow = 1.0;
            if (u_ReceiveShadow != 0 && u_Lights.lights[i].flags.y > 0.5) {
                shadow = calculateShadow(int(u_Lights.lights[i].flags.z), v_WorldPos, N, L, lightPos);
            }

            vec3 litColor = lightColor * lightIntensity * attenuation * shadow;
            totalDiffuse += litColor * celDiffuse;
            totalSpecular += litColor * celSpecular;
        }

        // Rim lighting
        float rim = calculateRimLighting(N, V, rimPower, rimIntensity);
        vec3 rimColor = ambient * rim;

        // Emissive (u_ToonMaterialParams.w = emissiveStrength)
        vec3 emissive = u_EmissiveColor.rgb * u_ToonMaterialParams.w;
        if (u_TextureFlags.w > 0.5) {
            emissive *= texture(u_EmissiveTexture, v_TexCoord).rgb;
        }

        // Final color composition
        vec3 diffuseColor = albedo.rgb * (ambient + totalDiffuse);
        vec3 specularColor = totalSpecular;
        vec3 color = diffuseColor + specularColor + rimColor + emissive;

        // Apply tint
        color *= u_TintColor.rgb;

        // Distance fog
        color = applyFog(color, v_WorldPos, u_CameraPosition);

        // Gamma correction
        color = pow(color, vec3(1.0 / 2.2));

        FragColor = vec4(color, albedo.a);
    }
