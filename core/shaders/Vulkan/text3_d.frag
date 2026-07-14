
#version 450

layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec4 v_Color;

layout(location = 0) out vec4 FragColor;

layout(push_constant) uniform PushConstants {
    mat4 u_ViewProjection;
    mat4 u_Model;
    vec4 u_TintColor;
} pc;

layout(set = 0, binding = 4) uniform sampler2D u_FontAtlas;


    void main() {
        float alpha = texture(u_FontAtlas, v_TexCoord).r;

        if (alpha < 0.01) {
            discard;
        }

        vec4 finalColor = v_Color * pc.u_TintColor;
        FragColor = vec4(finalColor.rgb, finalColor.a * alpha);
    }
