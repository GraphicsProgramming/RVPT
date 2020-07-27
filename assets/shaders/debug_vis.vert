#version 450 core

layout(binding = 0) uniform Camera { mat4 matrix; }
cam;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_norm;

layout(location = 0) out vec3 out_norm;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
    gl_Position = cam.matrix * vec4(in_pos, 1);
	/* flip image vertically */
	gl_Position.y = -gl_Position.y;
    out_norm = in_norm;
}