
#version 330 core

in vec2 v_TexCoord;

// Material textures
uniform sampler2D u_AlbedoTexture;
uniform int u_HasAlbedoTexture;
uniform float u_AlphaCutoff;

void main() {
    // Alpha testing for transparent materials (e.g., leaves, grass)
    if (u_HasAlbedoTexture == 1) {
        float alpha = texture(u_AlbedoTexture, v_TexCoord).a;
        if (alpha < u_AlphaCutoff) {
            discard; // Don't cast shadow for transparent pixels
        }
    }

    // Depth is automatically written to the depth buffer
    // No color output needed for shadow maps
}
