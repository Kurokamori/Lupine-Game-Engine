
#version 330 core

in vec4 v_Color;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform mat4 u_NormalMatrix;
uniform vec4 u_TintColor;

out vec4 FragColor;


    void main() {
        FragColor = v_Color * u_TintColor;
    }
