
#version 330 core

in vec2 v_TexCoord;
in vec3 v_WorldPos;

// Output to color attachment (R32_FLOAT)
out float fragDepth;

// Material textures
uniform sampler2D u_AlbedoTexture;
uniform int u_HasAlbedoTexture;
uniform float u_AlphaCutoff;

// Light position and range for cube map shadows
uniform vec3 u_LightPos;
uniform float u_LightRange;

void main() {
    // Alpha testing for transparent materials (e.g., leaves, grass)
    if (u_HasAlbedoTexture == 1) {
        float alpha = texture(u_AlbedoTexture, v_TexCoord).a;
        if (alpha < u_AlphaCutoff) {
            discard; // Don't cast shadow for transparent pixels
        }
    }

    // Calculate linear depth (distance from light) and normalize to [0,1]
    float lightDistance = length(v_WorldPos - u_LightPos);
    float linearDepth = lightDistance / u_LightRange;

    // Clamp to [0,1] range to ensure valid depth values
    linearDepth = clamp(linearDepth, 0.0, 1.0);

    // Output the normalized linear depth to the color attachment
    // This will be sampled as-is in the PBR shader
    fragDepth = linearDepth;
}

