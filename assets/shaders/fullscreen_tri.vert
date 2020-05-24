#version 450
#extension GL_ARB_separate_shader_objects : enable

vec2 positions[3] = vec2[](vec2(-1.0, -1.0), vec2(-1.0, 2.0), vec2(2.0, -1.0));

layout(location = 0) out vec4 position;
layout(location = 1) out vec2 uv;

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    position = gl_Position;
    uv = positions[gl_VertexIndex];
}
