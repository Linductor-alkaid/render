#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;

out VS_OUT {
    vec4 color;
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vs_out.color = aColor;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}

