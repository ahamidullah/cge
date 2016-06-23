#version 330

layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 in_color;

smooth out vec4 vert_color;

void main()
{
   gl_Position = pos;
   vert_color = in_color;
}
