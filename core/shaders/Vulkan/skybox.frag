
#version 450

layout(location = 0) in vec3 v_Position;
layout(location = 1) in vec3 v_TexCoord3D;

layout(location = 0) out vec4 FragColor;

layout(push_constant) uniform PushConstants {
    mat4 u_ViewProjection;
} pc;

layout(set = 0, binding = 2) uniform MaterialData {
    mat4 u_View;
    int u_SkyboxType;
    vec4 u_SkyboxColor;
    vec4 u_SkyTopColor;
    vec4 u_SkyHorizonColor;
    vec4 u_SkyBottomColor;
} material;

layout(set = 0, binding = 4) uniform samplerCube u_CubemapTexture;
layout(set = 0, binding = 5) uniform sampler2D u_PanoramicTexture;


    vec2 cartesianToSpherical(vec3 dir) {
        vec3 n = normalize(dir);
        float u = 0.5 + atan(n.z, n.x) / (2.0 * 3.14159265359);
        float v = 0.5 + asin(n.y) / 3.14159265359;
        return vec2(u, v);
    }

    void main() {
        if (material.u_SkyboxType == 0) {
            FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        }
        else if (material.u_SkyboxType == 1) {
            FragColor = material.u_SkyboxColor;
        }
        else if (material.u_SkyboxType == 2) {
            vec3 dir = normalize(v_Position);
            float height = dir.y;

            vec4 color;
            if (height > 0.0) {
                color = mix(material.u_SkyHorizonColor, material.u_SkyTopColor, height);
            } else {
                color = mix(material.u_SkyBottomColor, material.u_SkyHorizonColor, height + 1.0);
            }

            FragColor = color;
        }
        else if (material.u_SkyboxType == 3) {
            FragColor = texture(u_CubemapTexture, v_TexCoord3D);
        }
        else if (material.u_SkyboxType == 4) {
            vec2 uv = cartesianToSpherical(v_Position);
            FragColor = texture(u_PanoramicTexture, uv);
        }
        else {
            FragColor = vec4(1.0, 0.0, 1.0, 1.0);
        }
    }
