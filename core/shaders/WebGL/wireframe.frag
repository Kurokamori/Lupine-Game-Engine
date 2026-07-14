#version 300 es

precision highp float;
precision highp int;

in vec4 v_Color;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform vec4 u_TintColor;

out vec4 FragColor;


    void main() {
        FragColor = v_Color * u_TintColor;
    }
