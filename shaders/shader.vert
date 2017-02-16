layout (std140) uniform Matrices
{
	mat4 u_view;
	mat4 u_perspective_proj;
	mat4 u_ortho_proj;
};

#ifdef TEXT
	layout (location = 0) in vec3 l_pos;
	layout (location = 1) in vec2 l_uv;
	out vec2 t_uv;

	void main()
	{
		gl_Position = u_ortho_proj * vec4(l_pos, 1.0);
		//gl_Position = vec4(l_pos, 1.0);
		t_uv = l_uv;
	}

#elif defined(UI)
	layout (location = 0) in vec3 l_pos;
	layout (location = 1) in vec3 l_color;
	out vec3 t_color;

	void main()
	{
		t_color = l_color;
		gl_Position = u_ortho_proj * vec4(l_pos, 1.0f);
	}

#elif defined(TEXTURED_MESH) || defined(UNTEXTURED_MESH)
	uniform mat4 u_model;

	layout (location = 0) in vec3 l_pos;
	layout (location = 1) in vec3 l_normal;

	out vec3 t_normal;
	out vec3 t_frag_pos;
  #ifdef TEXTURED_MESH
	layout(location = 2) in vec2 l_uv;
	out vec2 t_uv;
  #endif
	void main()
	{
		gl_Position = u_perspective_proj * u_view * u_model * vec4(l_pos, 1.0f);
		t_frag_pos = vec3(u_model * vec4(l_pos, 1.0f));
		t_normal = normalize(mat3(transpose(inverse(u_model))) * l_normal);
  #ifdef TEXTURED_MESH
		t_uv = l_uv;
  #endif
	}

#else
#error "Missing shader lighting type: TEXT, UI, UNTEXTURED_MESH, or TEXTURED_MESH."
#endif
