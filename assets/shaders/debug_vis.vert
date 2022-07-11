#version 450 core

layout(binding = 0) uniform Camera { mat4 matrix; }
cam;

layout(location = 0) in vec3 in_position;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
    gl_Position = cam.matrix * vec4(in_position, 1);
    gl_Position.y = -gl_Position.y;
}