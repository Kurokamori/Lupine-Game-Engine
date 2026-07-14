#version 300 es

precision highp float;
precision highp int;

in vec3 v_Position;
in vec3 v_TexCoord3D;

uniform mat4 u_ViewProjection;
uniform mat4 u_View;
uniform int u_SkyboxType;
uniform vec4 u_SkyboxColor;
uniform vec4 u_SkyTopColor;
uniform vec4 u_SkyHorizonColor;
uniform vec4 u_SkyBottomColor;
uniform samplerCube u_CubemapTexture;
uniform sampler2D u_PanoramicTexture;

out vec4 FragColor;


    vec2 cartesianToSpherical(vec3 dir) {
        vec3 n = normalize(dir);
        float u = 0.5 + atan(n.z, n.x) / (2.0 * 3.14159265359);
        float v = 0.5 + asin(n.y) / 3.14159265359;
        return vec2(u, v);
    }

    void main() {
        if (u_SkyboxType == 0) {
            FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        }
        else if (u_SkyboxType == 1) {
            FragColor = u_SkyboxColor;
        }
        else if (u_SkyboxType == 2) {
            vec3 dir = normalize(v_Position);
            float height = dir.y;

            vec4 color;
            if (height > 0.0) {
                color = mix(u_SkyHorizonColor, u_SkyTopColor, height);
            } else {
                color = mix(u_SkyBottomColor, u_SkyHorizonColor, height + 1.0);
            }

            FragColor = color;
        }
        else if (u_SkyboxType == 3) {
            FragColor = texture(u_CubemapTexture, v_TexCoord3D);
        }
        else if (u_SkyboxType == 4) {
            vec2 uv = cartesianToSpherical(v_Position);
            FragColor = texture(u_PanoramicTexture, uv);
        }
        else {
            FragColor = vec4(1.0, 0.0, 1.0, 1.0);
        }
    }
