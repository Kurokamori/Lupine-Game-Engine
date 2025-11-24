
#version 330 core

in vec2 v_TexCoord;
in vec4 v_Color;
in vec2 v_LocalPos;

uniform sampler2D u_Texture;
uniform vec4 u_TintColor;
uniform vec4 u_Color;          // Direct color (used by 3D components)
uniform bool u_UseTexture;
uniform vec4 u_CornerRadius;  // x=top-left, y=top-right, z=bottom-right, w=bottom-left
uniform vec2 u_Size;           // Width and height of the rectangle
uniform vec2 u_UVMin;          // Minimum UV coordinates (default 0,0)
uniform vec2 u_UVMax;          // Maximum UV coordinates (default 1,1)

out vec4 FragColor;

// Signed distance function for a rounded rectangle
float sdRoundedBox(vec2 p, vec2 size, vec4 radius) {
    // Convert position from center-based (-0.5 to 0.5) to size-based coordinates
    vec2 pos = p * size;
    vec2 halfSize = size * 0.5;
    
    // Select the appropriate corner radius based on quadrant
    float r;
    if (pos.x > 0.0) {
        r = (pos.y > 0.0) ? radius.z : radius.y;  // bottom-right : top-right
    } else {
        r = (pos.y > 0.0) ? radius.w : radius.x;  // bottom-left : top-left
    }
    
    // Clamp radius to not exceed half the minimum dimension
    r = min(r, min(halfSize.x, halfSize.y));
    
    // Calculate distance to rounded corner
    vec2 q = abs(pos) - halfSize + r;
    float dist = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
    
    return dist;
}

void main() {
    // Calculate distance to edge
    float dist = sdRoundedBox(v_LocalPos, u_Size, u_CornerRadius);

    // Smooth antialiasing at edges (1 pixel transition)
    float edgeSoftness = 1.5 / length(u_Size);  // Adjust based on size
    float alpha = 1.0 - smoothstep(-edgeSoftness, edgeSoftness, dist);

    // Base color - use u_Color if set (alpha > 0), otherwise fall back to v_Color * u_TintColor
    // This allows 3D components to use u_Color directly while maintaining compatibility with 2D components
    vec4 color;
    if (u_Color.a > 0.0) {
        color = u_Color;
    } else {
        color = v_Color * u_TintColor;
    }

    // Apply texture if enabled
    if (u_UseTexture) {
        // Remap texture coordinates from 0-1 to uvMin-uvMax range
        vec2 remappedUV = u_UVMin + (v_TexCoord * (u_UVMax - u_UVMin));
        color *= texture(u_Texture, remappedUV);
    }

    // Apply corner rounding alpha
    color.a *= alpha;

    // Discard fully transparent pixels for performance
    if (color.a < 0.01) {
        discard;
    }

    FragColor = color;
}
