layout (std140) uniform Matrices
{
	mat4 u_view;
	mat4 u_perspective_proj;
	mat4 u_ortho_proj;
};

#ifdef UI
	layout (location = 0) in vec3 l_pos;
	layout (location = 1) in vec3 l_color;
	out vec3 t_color;

	void main()
	{
		t_color = l_color;
		gl_Position = u_ortho_proj * vec4(l_pos, 1.0f);
	}

#elif defined(TEX) || defined(NO_TEX)
	uniform mat4 u_model;

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
		gl_Position = u_perspective_proj * u_view * u_model * vec4(l_pos, 1.0f);
		t_frag_pos = vec3(u_model * vec4(l_pos, 1.0f));
		t_normal = normalize(mat3(transpose(inverse(u_model))) * l_normal);
  #ifdef TEX
		t_tex_coords = l_tex_coords;
  #endif
	}

#else
#error "Missing shader lighting type: UI, NO_TEX, or TEX."
#endif
