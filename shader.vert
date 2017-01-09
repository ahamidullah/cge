layout (std140) uniform Matrices
{
	mat4 u_projection;
	mat4 u_view;
};
uniform mat4 u_model;
//uniform mat4 u_view;
//uniform mat4 u_projection;

#ifdef NO_LIGHT
	layout (location = 0) in vec3 l_pos;
	
	void main()
	{
		gl_Position = u_projection * u_view * u_model * vec4(l_pos, 1.0f);
	}

#elif defined(TEX) || defined(NO_TEX)
	layout (location = 0) in vec3 l_pos;
	layout (location = 1) in vec3 l_normal;
#ifdef TEX
	layout(location = 2) in vec2 l_tex_coords;
#endif
	out vec3 t_normal;
	out vec3 t_frag_pos;
#ifdef TEX
	out vec2 t_tex_coords;
#endif
	void main()
	{
		gl_Position = u_projection * u_view * u_model * vec4(l_pos, 1.0f);
		t_frag_pos = vec3(u_model * vec4(l_pos, 1.0f));
		t_normal = normalize(mat3(transpose(inverse(u_model))) * l_normal);
#ifdef TEX
		t_tex_coords = l_tex_coords;
#endif
	}

#else
#error "Missing shader lighting type: NO_LIGHT, NO_TEX, or TEX."
#endif