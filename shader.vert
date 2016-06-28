#version 330

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec2 in_tex_coord;

out vec3 vert_color;
out vec2 tex_coord;

void main()
{
	gl_Position = vec4(pos, 1.0f);
	vert_color = in_color;
	tex_coord = in_tex_coord;
}
