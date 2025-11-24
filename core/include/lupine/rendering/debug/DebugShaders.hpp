#pragma once

namespace lupine {
namespace DebugShaders {

/**
 * Debug line vertex shader
 */
const char* const DebugLine_Vertex = R"(
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

uniform mat4 u_ViewProjection;

out vec4 v_Color;

void main() {
    v_Color = a_Color;
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}
)";

/**
 * Debug line fragment shader
 */
const char* const DebugLine_Fragment = R"(
#version 330 core

in vec4 v_Color;

out vec4 FragColor;

void main() {
    FragColor = v_Color;
}
)";

/**
 * Debug solid vertex shader (for filled shapes)
 */
const char* const DebugSolid_Vertex = R"(
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec4 v_Color;

void main() {
    v_Color = a_Color;
    gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
}
)";

/**
 * Debug solid fragment shader
 */
const char* const DebugSolid_Fragment = R"(
#version 330 core

in vec4 v_Color;

out vec4 FragColor;

void main() {
    FragColor = v_Color;
}
)";

/**
 * Grid shader vertex
 */
const char* const Grid_Vertex = R"(
#version 330 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_ViewProjection;

out vec3 v_WorldPos;

void main() {
    v_WorldPos = a_Position;
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}
)";

/**
 * Grid shader fragment with procedural grid
 */
const char* const Grid_Fragment = R"(
#version 330 core

in vec3 v_WorldPos;

uniform float u_CellSize;
uniform vec4 u_GridColor;
uniform vec4 u_AxisColor;
uniform bool u_Is3D;

out vec4 FragColor;

void main() {
    vec2 coord = u_Is3D ? v_WorldPos.xz : v_WorldPos.xy;

    vec2 grid = abs(fract(coord / u_CellSize - 0.5) - 0.5) / fwidth(coord / u_CellSize);
    float line = min(grid.x, grid.y);
    float alpha = 1.0 - min(line, 1.0);

    // Fade out grid based on distance
    float dist = length(v_WorldPos);
    float fadeFactor = 1.0 - smoothstep(20.0, 50.0, dist);
    alpha *= fadeFactor;

    vec4 color = u_GridColor;

    // Highlight axes
    float axisThickness = 0.05;
    if (u_Is3D) {
        if (abs(coord.x) < axisThickness) {
            color = mix(color, u_AxisColor, 0.7); // Z axis
        }
        if (abs(coord.y) < axisThickness) {
            color = mix(color, u_AxisColor, 0.7); // X axis
        }
    } else {
        if (abs(coord.x) < axisThickness) {
            color = mix(color, u_AxisColor, 0.7); // Y axis
        }
        if (abs(coord.y) < axisThickness) {
            color = mix(color, u_AxisColor, 0.7); // X axis
        }
    }

    FragColor = vec4(color.rgb, color.a * alpha);

    if (FragColor.a < 0.01) {
        discard;
    }
}
)";

} // namespace DebugShaders
} // namespace lupine
