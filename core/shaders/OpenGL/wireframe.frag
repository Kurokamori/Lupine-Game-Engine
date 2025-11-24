
#version 330 core

in vec4 v_Color;

uniform vec4 u_TintColor;

out vec4 FragColor;

void main() {
    FragColor = v_Color * u_TintColor;
}
