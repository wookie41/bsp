#version 330 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;

out vec3 PointColor;

uniform mat4 projection;

void main()
{
    PointColor = color; 
    gl_Position = projection * vec4(position, -1, 1);
}