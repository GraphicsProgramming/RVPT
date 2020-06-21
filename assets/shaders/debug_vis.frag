#version 450 core

layout(location = 0) in vec3 in_norm;

layout(location = 0) out vec4 out_color;

void main() { out_color = vec4(in_norm, 1.0); }