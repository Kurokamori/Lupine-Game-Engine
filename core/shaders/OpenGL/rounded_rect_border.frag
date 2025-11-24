
#version 330 core

in vec2 v_TexCoord;
in vec4 v_Color;
in vec2 v_LocalPos;

uniform vec4 u_TintColor;
uniform vec4 u_Color;             // Direct color (used by 3D components)
uniform vec4 u_CornerRadius;     // Outer corner radius (x=top-left, y=top-right, z=bottom-right, w=bottom-left)
uniform vec2 u_Size;              // Outer size of the border rectangle
uniform vec4 u_BorderWidth;       // Border widths (x=top, y=right, z=bottom, w=left)
uniform bool u_EnableAntialiasing; // Enable/disable edge antialiasing (default: true)

out vec4 FragColor;

// Signed distance function for a rounded rectangle
// p: position relative to center (in pixel space)
// size: full size of rectangle
// radius: corner radii (top-left, top-right, bottom-right, bottom-left)
float sdRoundedBox(vec2 p, vec2 size, vec4 radius) {
    vec2 halfSize = size * 0.5;
    
    // Select the appropriate corner radius based on quadrant
    float r;
    if (p.x > 0.0) {
        r = (p.y > 0.0) ? radius.z : radius.y;  // bottom-right : top-right
    } else {
        r = (p.y > 0.0) ? radius.w : radius.x;  // bottom-left : top-left
    }
    
    // Clamp radius to not exceed half the minimum dimension
    r = min(r, min(halfSize.x, halfSize.y));
    
    // Calculate distance to rounded corner
    vec2 q = abs(p) - halfSize + r;
    float dist = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
    
    return dist;
}

void main() {
    // Convert normalized position (-0.5 to 0.5) to pixel space (centered at origin)
    vec2 pixelPos = v_LocalPos * u_Size;
    
    // Calculate distance to outer edge
    float outerDist = sdRoundedBox(pixelPos, u_Size, u_CornerRadius);

    // Antialiasing control
    float edgeSoftness = u_EnableAntialiasing ? 1.5 : 0.0;
    float outerAlpha = 1.0 - smoothstep(-edgeSoftness, edgeSoftness, outerDist);
    
    // Calculate inner size (subtract border widths from outer size)
    // borderWidth: x=top, y=right, z=bottom, w=left
    vec2 innerSize = u_Size - vec2(u_BorderWidth.w + u_BorderWidth.y, u_BorderWidth.x + u_BorderWidth.z);
    
    float innerAlpha = 1.0;  // Default: show border (no cutout)
    
    // Only calculate inner cutout if inner size is valid
    if (innerSize.x > 0.0 && innerSize.y > 0.0) {
        // Calculate inner corner radius
        // Each corner's inner radius = outer radius - adjacent border widths
        // For corners, we subtract the smaller of the two adjacent borders to maintain continuity
        vec4 innerRadius = vec4(
            max(0.0, u_CornerRadius.x - min(u_BorderWidth.x, u_BorderWidth.w)),  // top-left
            max(0.0, u_CornerRadius.y - min(u_BorderWidth.x, u_BorderWidth.y)),  // top-right
            max(0.0, u_CornerRadius.z - min(u_BorderWidth.z, u_BorderWidth.y)),  // bottom-right
            max(0.0, u_CornerRadius.w - min(u_BorderWidth.z, u_BorderWidth.w))   // bottom-left
        );
        
        // Offset position for inner rectangle (account for non-uniform borders)
        // The inner rect is offset by half the difference between opposite borders
        vec2 borderOffset = vec2(
            (u_BorderWidth.w - u_BorderWidth.y) * 0.5,  // x offset: (left - right) / 2
            (u_BorderWidth.x - u_BorderWidth.z) * 0.5   // y offset: (top - bottom) / 2
        );
        
        vec2 innerPos = pixelPos - borderOffset;
        float innerDist = sdRoundedBox(innerPos, innerSize, innerRadius);
        
        // If inside inner rect (negative distance), hide this pixel to create the border
        // smoothstep returns 0 when innerDist < -edgeSoftness (inside - should be transparent)
        // smoothstep returns 1 when innerDist > edgeSoftness (outside - should be opaque)
        innerAlpha = smoothstep(-edgeSoftness, edgeSoftness, innerDist);
    }
    
    // Final alpha: show where outer shape is AND where inner cutout isn't
    float alpha = outerAlpha * innerAlpha;

    // Apply color - use u_Color if set (alpha > 0), otherwise fall back to v_Color * u_TintColor
    // This allows 3D components to use u_Color directly while maintaining compatibility with 2D components
    vec4 color;
    if (u_Color.a > 0.0) {
        color = u_Color;
    } else {
        color = v_Color * u_TintColor;
    }
    color.a *= alpha;

    // Discard fully transparent pixels
    if (color.a < 0.01) {
        discard;
    }

    FragColor = color;
}
