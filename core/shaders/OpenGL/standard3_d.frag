
#version 330 core

in vec3 v_Position;
in vec3 v_Normal;
in vec2 v_TexCoord;
in vec4 v_Color;

uniform sampler2D u_Texture;
uniform vec4 u_TintColor;
uniform bool u_UseTexture;

out vec4 FragColor;

void main() {
    vec3 normal = normalize(v_Normal);
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3)); // Simple directional light
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 lighting = vec3(0.3) + vec3(0.7) * diff; // Ambient + diffuse

    vec4 baseColor = v_Color * u_TintColor;
    if (u_UseTexture) {
        baseColor *= texture(u_Texture, v_TexCoord);
    }

    FragColor = vec4(baseColor.rgb * lighting, baseColor.a);
}
