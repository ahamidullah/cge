#version 330

in vec2 tex_coord;

out vec4 color;

uniform sampler2D our_tex;

void main()
{
	color = texture(our_tex, tex_coord);
}
