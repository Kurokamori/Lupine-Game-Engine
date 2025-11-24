
#version 330 core

in vec3 v_Position;
in vec3 v_TexCoord;

// Skybox type: 0=None, 1=Color, 2=Procedural, 3=Cubemap, 4=Panoramic
uniform int u_SkyboxType;

// Color skybox
uniform vec4 u_SkyboxColor;

// Procedural skybox
uniform vec4 u_SkyTopColor;
uniform vec4 u_SkyHorizonColor;
uniform vec4 u_SkyBottomColor;

// Cubemap skybox
uniform samplerCube u_CubemapTexture;

// Panoramic skybox
uniform sampler2D u_PanoramicTexture;

out vec4 FragColor;

// Convert cartesian coordinates to spherical UV for panoramic mapping
vec2 cartesianToSpherical(vec3 dir) {
    // Normalize direction
    vec3 n = normalize(dir);
    
    // Calculate spherical coordinates
    float u = 0.5 + atan(n.z, n.x) / (2.0 * 3.14159265359);
    float v = 0.5 - asin(n.y) / 3.14159265359;
    
    return vec2(u, v);
}

void main() {
    if (u_SkyboxType == 0) {
        // None - transparent
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
    }
    else if (u_SkyboxType == 1) {
        // Solid color
        FragColor = u_SkyboxColor;
    }
    else if (u_SkyboxType == 2) {
        // Procedural gradient
        vec3 dir = normalize(v_Position);
        float height = dir.y;
        
        // Interpolate between bottom, horizon, and top colors
        vec4 color;
        if (height > 0.0) {
            // Above horizon - blend from horizon to top
            color = mix(u_SkyHorizonColor, u_SkyTopColor, height);
        } else {
            // Below horizon - blend from bottom to horizon
            color = mix(u_SkyBottomColor, u_SkyHorizonColor, height + 1.0);
        }
        
        FragColor = color;
    }
    else if (u_SkyboxType == 3) {
        // Cubemap
        FragColor = texture(u_CubemapTexture, v_TexCoord);
    }
    else if (u_SkyboxType == 4) {
        // Panoramic (equirectangular)
        vec2 uv = cartesianToSpherical(v_Position);
        FragColor = texture(u_PanoramicTexture, uv);
    }
    else {
        // Unknown type - magenta for debugging
        FragColor = vec4(1.0, 0.0, 1.0, 1.0);
    }
}

