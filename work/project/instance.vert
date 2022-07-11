#version 410 core
layout (location = 0) in vec3 pos;
layout (location = 3) in mat4 instanceMatrix;

uniform mat4 projection, view, modelMatrix;

void main()
{
    gl_Position = projection * view * modelMatrix * instanceMatrix * vec4(pos, 1.0f); 
}