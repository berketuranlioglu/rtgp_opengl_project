#version 410 core
layout (location = 0) in vec3 position;

out vec4 ParticleColor;

uniform vec4 color;
uniform mat4 projection;
uniform mat4 view;
uniform mat4 modelMatrix;

void main()
{
    ParticleColor = color;
    gl_Position = projection * view * modelMatrix * vec4((position),  1.0f);
}