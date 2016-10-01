#version 330

layout (location = 0) in vec3 l_pos;
layout (location = 1) in vec3 l_normal;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

out vec3 t_normal;
out vec3 t_frag_pos;

void main()
{
	gl_Position = u_projection * u_view * u_model * vec4(l_pos, 1.0f);
	t_frag_pos = vec3(u_model * vec4(l_pos, 1.0f));
	t_normal = normalize(mat3(transpose(inverse(u_model))) * l_normal);
}
